/*
 * Decoder for dedup files
 *
 * Copyright 2010 Princeton University.
 * All rights reserved.
 *
 * Originally written by Minlan Yu.
 * Largely rewritten by Christian Bienia.
 * Largely rewritten by I-Ting Angelina Lee
 */

/*
 * The pipeline model for Encode is 
 *      Fragment->FragmentRefine->Deduplicate->Compress->Reorder
 * Each stage has basically three steps:
 * 1. fetch a group of items from the queue
 * 2. process the items
 * 3. put them in the queue for the next stage
 */

#include <assert.h>
#include <strings.h>
#include <math.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <chrono>
#include <iostream>

#include "config.h"
#include "debug.h"
#include "dedupdef.h"
#include "encoder.h"
#include "util/ktiming.h"
#include "util/util.h"
#include "util/hashtable.h"
#include "util/rabin.h"


#include <cilk/cilk.h>
#include <future.hpp>

#define TIMING 0
#if TIMING
#define WHEN_TIMING(...) __VA_ARGS__ 
#else
#define WHEN_TIMING(...)
#endif



#ifdef ENABLE_GZIP_COMPRESSION
#include <zlib.h>
#endif //ENABLE_GZIP_COMPRESSION

#ifdef ENABLE_BZIP2_COMPRESSION
#include <bzlib.h>
#endif //ENABLE_BZIP2_COMPRESSION

#ifdef ENABLE_STATISTICS
#include "util/stats.h"
//variable with global statistics
stats_t stats;
#endif


WHEN_TIMING(
static uint64_t preproc_time = 0;
static uint64_t read_time = 0;
static uint64_t dedup_time = 0;
static uint64_t comp_time = 0;
static uint64_t write_time = 0;
static uint64_t pipe_time = 0;
)

// The configuration block defined in main
static config_t * conf;
// Hash table data structure & utility functions
static struct hashtable *cache;

static unsigned int hash_from_key_fn( void *k ) {
    // NOTE: sha1 sum is integer-aligned
    return ((unsigned int *)k)[0];
}

static int keys_equal_fn( void *key1, void *key2 ) {
    return (memcmp(key1, key2, SHA1_LEN) == 0);
}

// Arguments 
typedef struct file_info {
    // file descriptor for input file, first pipeline stage only
    int fd_in;
    // file descriptor for output file, 
    int fd_out;
    // input file buffer, first pipeline stage & preloading only
    unsigned char *buffer; // holds the content from input file
    size_t buf_seek;  // where we are reading in the buffer
    // meaningful content left in the buffer after buf_seek
    size_t bytes_left;
    int next_offset;  // the next offset returned by rabinseg after buf_seek
} file_info_t;

// Simple write utility function
static int write_file(int fd, u_char type, size_t len, u_char * content) {
    if (xwrite(fd, &type, sizeof(type)) < 0){
        perror("xwrite:");
        EXIT_TRACE("xwrite type fails\n");
        return -1;
    }
    if (xwrite(fd, &len, sizeof(len)) < 0){
        EXIT_TRACE("xwrite content fails\n");
    }
    if (xwrite(fd, content, len) < 0){
        EXIT_TRACE("xwrite content fails\n");
    }
    return 0;
}

/*
 * Helper function that writes a chunk to an output file depending on
 * its state. The function will write the SHA1 sum if the chunk has
 * already been written before, or it will write the compressed data
 * of the chunk if it has not been written yet.
 *
 * This function will block if the compressed data is not available yet.
 * This function might update the state of the chunk if there are any changes.
 */
// NOTE: The serial version relies on the fact that chunks are processed 
// in-order, which means if it reaches the function it is guaranteed all 
// data is ready.
static void write_chunk_to_file(int fd, chunk_t *chunk) {
    assert(chunk!=NULL);

    if(!chunk->isDuplicate) {
        // Unique chunk, data has not been written yet, do so now
        write_file(fd, TYPE_COMPRESS, chunk->len, chunk->data);
    } else {
        // Duplicate chunk, data has been written to file; just write SHA1
        write_file( fd, TYPE_FINGERPRINT, SHA1_LEN, 
                    (unsigned char *)(chunk->sha1) );
    }
}


/*
 * Computational kernel of compression stage
 *
 * Actions performed:
 *  - Compress a data chunk
 */
static void sub_Compress(chunk_t *const chunk) {

    assert(chunk!=NULL);
    // compress the item and add it to the database
    switch (conf->compress_type) {
        case COMPRESS_NONE:
            break;  // nothing to do 

        #ifdef ENABLE_GZIP_COMPRESSION
        case COMPRESS_GZIP: {
            // Gzip compression buffer must be at least 0.1% larger than 
            // the source buffer plus 12 bytes
            size_t len = chunk->len + (chunk->len >> 9) + 12;
            unsigned char *comp_data = (unsigned char *) malloc(len); 
            if(comp_data == NULL) {
                EXIT_TRACE("Malloc for space of compression data failed.\n");
            }
            size_t old_len = len;
            // compress the block
            int r = compress(comp_data, &len, chunk->data, chunk->len);
            if (r != Z_OK) {
                EXIT_TRACE("Compression failed\n");
            }
            if(len < old_len) { // Shrink buffer to actual size
                unsigned char *new_mem = (unsigned char *) realloc(comp_data, len);
                assert(new_mem != NULL);
                comp_data = new_mem;
            }
            // no need to free the old data, because those are just pointer 
            // pointing into the buffer that stores the data read from the 
            // input file
            chunk->data = comp_data;
            chunk->len = len;
            break;
        }
        #endif // ENABLE_GZIP_COMPRESSION

        #ifdef ENABLE_BZIP2_COMPRESSION
        case COMPRESS_BZIP2: {
            // Bzip compression buffer must be at least 1% larger than 
            // the source buffer plus 600 bytes
            size_t len = chunk->len + (chunk->len >> 6) + 600;
            unsigned char *comp_data = (unsigned char *) malloc(len); 
            if(comp_data == NULL) {
                EXIT_TRACE("Malloc for space of compression data failed.\n");
            }
            size_t old_len = len;
            // compress the block
            int ret = BZ2_bzBuffToBuffCompress(comp_data, &len, chunk->data, 
                                               chunk->len, 9, 0, 30);
            if (r != BZ_OK) {
                EXIT_TRACE("Compression failed\n");
            }
            // Shrink buffer to actual size
            if(len < old_len) {
                unsigned char *new_mem = (unsigned char *) realloc(comp_data, len);
                assert(new_mem != NULL);
                comp_data = new_mem;
            }
            // no need to free the old data, because those are just pointer
            // pointing into the buffer that stores data read from input file
            chunk->data = comp_data;
            chunk->len = len;
            break;
        }
        #endif // ENABLE_BZIP2_COMPRESSION

        default:
            EXIT_TRACE("Compression type not implemented.\n");
            break;
    }
#ifdef ENABLE_STATISTICS
    stats.total_compressed += chunk->len;
#endif

    return;
}


/*
 * Computational kernel of deduplication stage
 *
 * Actions performed:
 *  - Calculate SHA1 signature for each incoming data chunk
 *  - Perform database lookup to determine chunk redundancy status
 *  - On miss add chunk to database
 *  - Returns chunk redundancy status
 */
static int sub_Deduplicate(chunk_t *const chunk) {
    assert(chunk != NULL);
    assert(chunk->data != NULL);

    SHA1_Digest(chunk->data, chunk->len, (unsigned char *)(chunk->sha1));

    // Query database to determine whether we've seen the data chunk before
    int found = hashtable_search_key(cache, (void *)(chunk->sha1));
    chunk->isDuplicate = found;
    if(found == 0) {
        // Miss: Create entry in hashtable and forward data to compression stage
        // Insert the key into the hashtable so the later iterations know that
        // this particular data has been seen by earlier iteration 
        if(hashtable_insert(cache, (void *)(chunk->sha1), chunk) == 0) {
            EXIT_TRACE("hashtable_insert_key failed");
        }
    }
    // There is nothing to do if it's a hit

#ifdef ENABLE_STATISTICS
    if(found) {
        stats.nDuplicates++;
    } else {
        stats.total_dedup += chunk->len;
    }
#endif 

    return found;
}
   
// Never changes; pracitcally a define
static const int rf_win_dataprocess = 0;

// Read 'bytes_to_read' into buffer and returns actual bytes read
// Would like to use xread but can't --- it returns 0 if the file hits EOF
// before bytes_to_read number of bytes are read
static inline size_t
read_next_chunk(int fd, unsigned char *buffer, size_t bytes_to_read) {
    ssize_t ret;
    ssize_t bytes_read = 0;
    while(bytes_read < bytes_to_read) {
        ret = read(fd, buffer, bytes_to_read - bytes_read);
        if(ret < 0) {
            perror("I/O error:");
            EXIT_TRACE("file read fails\n");
        } else if(ret == 0) { // done reading the file
            break;
        }
        bytes_read += ret;
    }

    return bytes_read;
}

static inline void
setup_initial_buffer(file_info_t *const args, 
                     u32int *const rabintab, u32int *const rabinwintab) {

    if(conf->preloading == 0) {
        assert(args->buffer != NULL);
        size_t bytes_read = read_next_chunk(args->fd_in, args->buffer, MAXBUF);
        if(bytes_read == 0) EXIT_TRACE("Input file empty.");
        args->bytes_left = bytes_read;
    }
    args->next_offset = rabinseg(args->buffer, args->bytes_left, 
                                 rf_win_dataprocess, rabintab, rabinwintab);
    assert(args->next_offset <= args->bytes_left);
}

// get the next chunk and setup the file buffer, buffer seek, and next offset
// for the next chunk.  If we hit the end of the file, the next offset will 
// be set to 0 after the last chunk is retrieved.  Each malloced-buffer that
// holds the input file content will be freed when the last chunk pointing
// to it has been written out to the output file, except for the very last
// buffer, which is freed in Encode when the pipeline ends.
static chunk_t *
get_next_chunk(file_info_t *const args,
               u32int *const rabintab,
               u32int *const rabinwintab) {

    // static local variable to preserve the values between invocations
    size_t buf_seek = args->buf_seek;
    size_t bytes_left = args->bytes_left;
    unsigned char *const buffer = args->buffer;
    size_t next_offset = args->next_offset;
    
    // XXX: this differs from the encode-serial version.
    // we need a chunk per active iteration, since they can go in parallel
    // also, we want to use the return value of this function as an indication
    // of when we should stop the pipe iter, so it ends up being more 
    // convenient to malloc chunk in here --- would be nice to always 
    // detach after first stage; then we can do this in the iter body.
    chunk_t *chunk = NULL;
    if(next_offset > 0) {
        chunk = (chunk_t *) malloc(sizeof(chunk_t)); 
        if(chunk == NULL) EXIT_TRACE("Memory allocation failed.\n"); 
        chunk->buffer_to_free = (unsigned char *) NULL;

        chunk->data = buffer + buf_seek;
        chunk->len = next_offset;
        bytes_left -= next_offset;
        buf_seek += next_offset;
        assert(bytes_left >= 0);

        if(bytes_left != 0) { 
            // find the next chunk to split off 
            next_offset = rabinseg(buffer + buf_seek, bytes_left, 
                                   rf_win_dataprocess, rabintab, rabinwintab);
            if( (conf->preloading == 0) && (next_offset == bytes_left) ) {
                // this will be the last chunk pointing to the old buffer
                unsigned char *new_buf = (unsigned char *) malloc(MAXBUF);
                size_t bytes_read = read_next_chunk(args->fd_in,
                                                    new_buf+bytes_left,
                                                    MAXBUF-bytes_left); 

                if(bytes_read > 0) { // there is actually more input
                    // copy the left over
                    memcpy(new_buf, buffer + buf_seek, bytes_left); 
                    // replace the old buffer with the new one and 
                    // be sure to free the old one
                    args->buffer = new_buf; 
                    chunk->buffer_to_free = buffer; 
                    buf_seek = 0; // reset the seek index 
                    bytes_left += bytes_read; // reset the other stuff 
                    next_offset = rabinseg(args->buffer, bytes_left, 
                            rf_win_dataprocess, rabintab, rabinwintab);
                } else {
                    // otherwise we have hit the end of the file, and there is 
                    // no point moving data into new buf; we will realize that
                    // we are done the next time this function is invoked,
                    //  via the fact that bytes_left will be 0 
                    free(new_buf);
                }
            }
            assert(next_offset <= bytes_left && next_offset >= 0); 

        } else {  // we are indeed done after this chunk returns
            next_offset = 0;
        }

        args->next_offset = next_offset;
        args->buf_seek = buf_seek;
        args->bytes_left = bytes_left;
    }
    
    return chunk;
}

int compAndWrite(cilk::future<int> *prevIterWriteFuture, 
                 int isDuplicate, chunk_t *chunk, int fd_out) {

    // If chunk is unique compress & archive it.
    if(isDuplicate == 0) {
      sub_Compress(chunk); // compress the entire chunk
      // chunk.data will point to a newly-malloc-ed memory
    }

    if(prevIterWriteFuture != NULL) {
        prevIterWriteFuture->get();
        delete prevIterWriteFuture;
    }

    write_chunk_to_file(fd_out, chunk);
    // pipe_continue(0);

    // since we have written out the chunk, now we can free the buffer
    // if this was the last chunk pointing into the buffer
    if(chunk->buffer_to_free) {
      free(chunk->buffer_to_free);
      chunk->buffer_to_free = (unsigned char *) NULL;
    }
    // the SHA1 is in the hashtable, so we can't free the chunk yet
    if(chunk->isDuplicate == 0) { 
      // the compressed data has been written out, so we can free it
      free(chunk->data);
      // but we cannot free chunk yet, because its SHA1 needs to be
      // in the hashtable
    } else {  // otherwise, we can free it
      free(chunk);
    }
  
    return 0;
}

cilk::future<int> *
dedupAndOnward(cilk::future<cilk::future<int>*> *prevIterDedupFuture, 
               chunk_t *chunk, int fd_out) {
    
    cilk::future<int> *prevIterWriteFuture = NULL;

    if(prevIterDedupFuture != NULL) { // not first iter
        prevIterWriteFuture = prevIterDedupFuture->get();   // wait for prev iter of DedupStage to finish
        delete prevIterDedupFuture;
    }

    // Deduplicate: check if in the hashtable; if so, get the 
    // pointer to the chunk that contains the compressed data
    int isDuplicate = sub_Deduplicate(chunk);

    cilk::future<int> *writeFuture = (cilk::future<int> *) malloc(sizeof(cilk::future<int>));
    reasync_helper<int, cilk::future<int> *, int, chunk_t *, int>
        (writeFuture, compAndWrite, prevIterWriteFuture, isDuplicate, chunk, fd_out);

    return writeFuture;
}

/* 
 * Integrate all computationally intensive pipeline
 * stages to improve cache efficiency.
 * 
 * Ange: The file is read in and chopped into chunks.
 * Each chunk is then furhter segmented into smaller segments using Rabin 
 * finger printing (and each segment is variable size --- Rabin finger 
 * printing finds a good place to split the chunk so that even if the file 
 * changes, where the segments split stays roughly the same).
 *
 * Each segment goes through the stage of 
 * -- check duplicates (compute SHA1 key)
 * -- compress the segment if this is first time it's seen
 * -- write the segment (or SHA1) to file, depending on whether it is a 
 *    duplicate or not
 */
void *SerialIntegratedPipeline(file_info_t* args) {

    WHEN_TIMING( clockmark_t first, last; )
    WHEN_TIMING( clockmark_t begin, end; )

    WHEN_TIMING( first = ktiming_getmark(); )

    // int fd_out = create_output_file(conf->outfile);
    // XXX: Write header
    if (write_header(args->fd_out, conf->compress_type)) {
        EXIT_TRACE("Cannot write output file header.\n");
    }

    u32int *const rabintab = (u32int *) malloc(256*sizeof rabintab[0]);
    u32int *const rabinwintab = (u32int *) malloc(256*sizeof rabintab[0]);

    if(rabintab == NULL || rabinwintab == NULL) {
        EXIT_TRACE("Memory allocation failed.\n");
    }
    rabininit(rf_win_dataprocess, rabintab, rabinwintab);

    WHEN_TIMING( begin = ktiming_getmark(); )
    setup_initial_buffer(args, rabintab, rabinwintab);
    WHEN_TIMING({
        end = ktiming_getmark();
        read_time += ktiming_diff_usec(&begin, &end);
    })

    chunk_t *chunk;
    cilk::future<cilk::future<int>*> *prevDedupFuture = NULL;
    cilk::future<cilk::future<int>*> *dedupFuture = NULL;

    while( (chunk = get_next_chunk(args, rabintab, rabinwintab)) != NULL ) {

        assert(chunk->len > 0);
        // keep of the stats on the sizes of the uncompressed chunks seen
        #ifdef ENABLE_STATISTICS
        // update statistics
        stats.nChunks[CHUNK_SIZE_TO_SLOT(chunk->len)]++;
        #endif 

        prevDedupFuture = dedupFuture;
        dedupFuture = (cilk::future<cilk::future<int>*> *) 
                          malloc(sizeof(cilk::future<cilk::future<int>*>));
        reasync_helper<cilk::future<int> *,
                       cilk::future<cilk::future<int>*> *, chunk_t *, int>
            (dedupFuture, dedupAndOnward, prevDedupFuture, chunk, args->fd_out);
    }

    // finish the last iter
    cilk::future<int> *lastWriteFuture = dedupFuture->get();
    lastWriteFuture->get();

    delete dedupFuture;
    delete lastWriteFuture;

    free(rabintab);
    free(rabinwintab);

    WHEN_TIMING({
        last = ktiming_getmark();
        pipe_time = ktiming_diff_usec(&first, &last);
    })
    
    return NULL;
}


/*--------------------------------------------------------------------------*/
/* Encode
 * Compress an input stream
 *
 * Arguments:
 *   conf:    Configuration parameters
 *
 */
void Encode(config_t * _conf) {

    struct stat filestat;

    // timing stuff
    //    float preload_time = 0.0f;
    //float process_time = 0.0f;
    //clockmark_t begin, end, preload_end; 

    /*
    int res = fib(40); 
    fprintf(stderr, "fib of 30 is: %d.\n", res);
    */

    conf = _conf;

#ifdef ENABLE_STATISTICS
    init_stats(&stats);
#endif

    // Create chunk cache
    cache = hashtable_create(65536, hash_from_key_fn, keys_equal_fn, FALSE);
    if(cache == NULL) {
        printf("ERROR: Out of memory\n");
        exit(1);
    }

    file_info_t args;

    /* src file stat */
    if (stat(conf->infile, &filestat) < 0) 
        EXIT_TRACE("stat() %s failed: %s\n", conf->infile, strerror(errno));

    if (!S_ISREG(filestat.st_mode)) 
        EXIT_TRACE("not a normal file: %s\n", conf->infile);
#ifdef ENABLE_STATISTICS
    stats.total_input = filestat.st_size;
#endif

    /* src file open */
    if((args.fd_in = open(conf->infile, O_RDONLY | O_LARGEFILE)) < 0) { 
        EXIT_TRACE("%s file open error %s\n", conf->infile, strerror(errno));
    }
    /* output file open */
    if((args.fd_out = open(conf->outfile, 
                           O_CREAT | O_TRUNC | O_WRONLY | O_TRUNC, 
                           S_IRGRP | S_IWUSR | S_IRUSR | S_IROTH)) < 0) {
        EXIT_TRACE("%s output file open error %s\n", conf->outfile, 
                   strerror(errno));
    }

    //begin = ktiming_getmark();
    // Sanity check
    if(MAXBUF < 8 * ANCHOR_JUMP) {
        printf("WARNING: I/O buffer size is small. Performance degraded.\n");
        fflush(NULL);
    }

    if(conf->preloading) {
        // Load entire file into memory if requested by user
        unsigned char *file_buffer = (unsigned char *) malloc(filestat.st_size);
        if(file_buffer == NULL)
            EXIT_TRACE("Error allocating memory for input buffer.\n");

        ssize_t bytes_read=0;

        // Read data until buffer full
        while(bytes_read < filestat.st_size) {
            ssize_t r = read(args.fd_in, file_buffer + bytes_read, 
                            filestat.st_size - bytes_read);
            if(r < 0) {
                perror("I/O error: ");
            } else if(r == 0) {
                break;
            }
            bytes_read += r;
        }
        args.bytes_left = filestat.st_size;
        args.buffer = file_buffer;

        //preload_end = ktiming_getmark();
        //preload_time = ktiming_diff_usec(&begin, &preload_end);
    
    } else {
        args.buffer = (unsigned char *) malloc(MAXBUF);
    } 
    args.buf_seek = 0;

    // XXX This is where the old ROI timing begin
    auto start = std::chrono::steady_clock::now();

    // Do the processing
    SerialIntegratedPipeline(&args);

    // XXX This is where the old ROI timing end
    //end = ktiming_getmark();
    //process_time = ktiming_diff_usec(&begin, &end);
    auto end = std::chrono::steady_clock::now();
    auto time = std::chrono::duration <double, std::milli> (end-start).count();
    std::cout << "Benchmark time: " << time << " ms" << std::endl;


    // clean up 
    free(args.buffer);

    /* clean up with the src file */
    if(conf->infile != NULL)
        close(args.fd_in);

    close(args.fd_out);

    hashtable_destroy(cache, TRUE);


    WHEN_TIMING({
        printf("Preproc time   = %.4f seconds\n", (double)preproc_time*1.0e-9);
        printf("Reading time   = %.4f seconds\n", (double)read_time*1.0e-9);
        printf("Dedup   time   = %.4f seconds\n", (double)dedup_time*1.0e-9);
        printf("Compress time  = %.4f seconds\n", (double)comp_time*1.0e-9);
        printf("Writing time   = %.4f seconds\n", (double)write_time*1.0e-9);
        printf("Mist. time     = %.4f seconds\n",
               (double)(pipe_time - preproc_time - read_time
                        - dedup_time - comp_time - write_time)*1.0e-9);
    })

    // if(preload_time) {
    //     printf("Preloading time = %.4f seconds\n", preload_time * 1.0e-9);
    // }
    // printf("Processing time = %.4f seconds\n", process_time * 1.0e-9);

#ifdef ENABLE_STATISTICS
    fprintf(stderr, "Printing out stats.\n");
    // dest file stat
    if (stat(conf->outfile, &filestat) < 0) 
        EXIT_TRACE("stat() %s failed: %s\n", conf->outfile, strerror(errno));
    stats.total_output = filestat.st_size;
    // Analyze and print statistics
    // if(conf->verbose) 
    print_stats(&stats);
#endif 
}

