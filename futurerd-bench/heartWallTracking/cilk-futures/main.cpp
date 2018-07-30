//==============================================================================
//==============================================================================
//	DEFINE / INCLUDE
//==============================================================================
//==============================================================================

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <chrono>
#include <cilk/cilk.h>

#include "avilib.hpp"
#include "avimod.hpp"
#include "define.hpp"

#include "../../util/util.hpp"
#include <future.hpp>
#include <rd.h>

using namespace std;

// defined in compute-steps.c
extern void compute_kernel(const public_struct *pub, private_struct *priv); 

//==============================================================================
//	WRITE DATA FUNCTION
//==============================================================================

#ifdef OUTPUT
static void write_data(const char *filename, int frameNo, int frames_processed,
                int endoPoints, int* input_a, int* input_b, int epiPoints,
                int* input_2a, int* input_2b) {

    //================================================================================
    //	VARIABLES
    //================================================================================

    FILE* fid;
    int i,j;

    //================================================================================
    //	OPEN FILE FOR READING
    //================================================================================

    fid = fopen(filename, "w+");
    if( fid == NULL ) {
        printf( "The file was not opened for writing\n" );
        return;
    }

    //================================================================================
    //	WRITE VALUES TO THE FILE
    //================================================================================

    fprintf(fid, "Total AVI Frames: %d\n", frameNo);	
    fprintf(fid, "Frames Processed: %d\n", frames_processed);	
    fprintf(fid, "endoPoints: %d\n", endoPoints);
    fprintf(fid, "epiPoints: %d", epiPoints);

    for(j=0; j<frames_processed;j++) {
        fprintf(fid, "\n---Frame %d---",j);
        fprintf(fid, "\n--endo--\n");
        for(i=0; i<endoPoints; i++) {
            fprintf(fid, "%d\t", input_a[j+i*frameNo]);
        }
        fprintf(fid, "\n");
        for(i=0; i<endoPoints; i++) {
            // if(input_b[j*size+i] > 2000) input_b[j*size+i]=0;
            fprintf(fid, "%d\t", input_b[j+i*frameNo]);
        }
        fprintf(fid, "\n--epi--\n");
        for(i=0; i<epiPoints; i++) {
            //if(input_2a[j*size_2+i] > 2000) input_2a[j*size_2+i]=0;
            fprintf(fid, "%d\t", input_2a[j+i*frameNo]);
        }
        fprintf(fid, "\n");
        for(i=0; i<epiPoints; i++) {
            //if(input_2b[j*size_2+i] > 2000) input_2b[j*size_2+i]=0;
            fprintf(fid, "%d\t", input_2b[j+i*frameNo]);
        }
    }

    //================================================================================
    //		CLOSE FILE
    //================================================================================

    fclose(fid);
}
#endif

static void init_public_and_private_struct(public_struct *pub, private_struct *priv) {
    //==============================================================================m======================
    //	ENDO POINTS
    //====================================================================================================

    pub->endoPoints = ENDO_POINTS;
    pub->d_endo_mem = sizeof(int) * pub->endoPoints;
    pub->d_endoRow = (int *)malloc(pub->d_endo_mem);
    pub->d_endoRow[ 0] = 369;
    pub->d_endoRow[ 1] = 400;
    pub->d_endoRow[ 2] = 429;
    pub->d_endoRow[ 3] = 452;
    pub->d_endoRow[ 4] = 476;
    pub->d_endoRow[ 5] = 486;
    pub->d_endoRow[ 6] = 479;
    pub->d_endoRow[ 7] = 458;
    pub->d_endoRow[ 8] = 433;
    pub->d_endoRow[ 9] = 404;
    pub->d_endoRow[10] = 374;
    pub->d_endoRow[11] = 346;
    pub->d_endoRow[12] = 318;
    pub->d_endoRow[13] = 294;
    pub->d_endoRow[14] = 277;
    pub->d_endoRow[15] = 269;
    pub->d_endoRow[16] = 275;
    pub->d_endoRow[17] = 287;
    pub->d_endoRow[18] = 311;
    pub->d_endoRow[19] = 339;
    pub->d_endoCol = (int *)malloc(pub->d_endo_mem);
    pub->d_endoCol[ 0] = 408;
    pub->d_endoCol[ 1] = 406;
    pub->d_endoCol[ 2] = 397;
    pub->d_endoCol[ 3] = 383;
    pub->d_endoCol[ 4] = 354;
    pub->d_endoCol[ 5] = 322;
    pub->d_endoCol[ 6] = 294;
    pub->d_endoCol[ 7] = 270;
    pub->d_endoCol[ 8] = 250;
    pub->d_endoCol[ 9] = 237;
    pub->d_endoCol[10] = 235;
    pub->d_endoCol[11] = 241;
    pub->d_endoCol[12] = 254;
    pub->d_endoCol[13] = 273;
    pub->d_endoCol[14] = 300;
    pub->d_endoCol[15] = 328;
    pub->d_endoCol[16] = 356;
    pub->d_endoCol[17] = 383;
    pub->d_endoCol[18] = 401;
    pub->d_endoCol[19] = 411;
    pub->d_tEndoRowLoc = (int *)malloc(pub->d_endo_mem * pub->frames);
    pub->d_tEndoColLoc = (int *)malloc(pub->d_endo_mem * pub->frames);

    //====================================================================================================
    //	EPI POINTS
    //====================================================================================================

    pub->epiPoints = EPI_POINTS;
    pub->d_epi_mem = sizeof(int) * pub->epiPoints;
    pub->d_epiRow = (int *)malloc(pub->d_epi_mem);
    pub->d_epiRow[ 0] = 390;
    pub->d_epiRow[ 1] = 419;
    pub->d_epiRow[ 2] = 448;
    pub->d_epiRow[ 3] = 474;
    pub->d_epiRow[ 4] = 501;
    pub->d_epiRow[ 5] = 519;
    pub->d_epiRow[ 6] = 535;
    pub->d_epiRow[ 7] = 542;
    pub->d_epiRow[ 8] = 543;
    pub->d_epiRow[ 9] = 538;
    pub->d_epiRow[10] = 528;
    pub->d_epiRow[11] = 511;
    pub->d_epiRow[12] = 491;
    pub->d_epiRow[13] = 466;
    pub->d_epiRow[14] = 438;
    pub->d_epiRow[15] = 406;
    pub->d_epiRow[16] = 376;
    pub->d_epiRow[17] = 347;
    pub->d_epiRow[18] = 318;
    pub->d_epiRow[19] = 291;
    pub->d_epiRow[20] = 275;
    pub->d_epiRow[21] = 259;
    pub->d_epiRow[22] = 256;
    pub->d_epiRow[23] = 252;
    pub->d_epiRow[24] = 252;
    pub->d_epiRow[25] = 257;
    pub->d_epiRow[26] = 266;
    pub->d_epiRow[27] = 283;
    pub->d_epiRow[28] = 305;
    pub->d_epiRow[29] = 331;
    pub->d_epiRow[30] = 360;
    pub->d_epiCol = (int *)malloc(pub->d_epi_mem);
    pub->d_epiCol[ 0] = 457;
    pub->d_epiCol[ 1] = 454;
    pub->d_epiCol[ 2] = 446;
    pub->d_epiCol[ 3] = 431;
    pub->d_epiCol[ 4] = 411;
    pub->d_epiCol[ 5] = 388;
    pub->d_epiCol[ 6] = 361;
    pub->d_epiCol[ 7] = 331;
    pub->d_epiCol[ 8] = 301;
    pub->d_epiCol[ 9] = 273;
    pub->d_epiCol[10] = 243;
    pub->d_epiCol[11] = 218;
    pub->d_epiCol[12] = 196;
    pub->d_epiCol[13] = 178;
    pub->d_epiCol[14] = 166;
    pub->d_epiCol[15] = 157;
    pub->d_epiCol[16] = 155;
    pub->d_epiCol[17] = 165;
    pub->d_epiCol[18] = 177;
    pub->d_epiCol[19] = 197;
    pub->d_epiCol[20] = 218;
    pub->d_epiCol[21] = 248;
    pub->d_epiCol[22] = 276;
    pub->d_epiCol[23] = 304;
    pub->d_epiCol[24] = 333;
    pub->d_epiCol[25] = 361;
    pub->d_epiCol[26] = 391;
    pub->d_epiCol[27] = 415;
    pub->d_epiCol[28] = 434;
    pub->d_epiCol[29] = 448;
    pub->d_epiCol[30] = 455;
    pub->d_tEpiRowLoc = (int *)malloc(pub->d_epi_mem * pub->frames);
    pub->d_tEpiColLoc = (int *)malloc(pub->d_epi_mem * pub->frames);

    //====================================================================================================
    //	ALL POINTS
    //====================================================================================================

    pub->allPoints = ALL_POINTS;

    //=====================
    //	CONSTANTS
    //=====================

    pub->tSize = 25;
    pub->sSize = 40;
    pub->maxMove = 10;
    pub->alpha = 0.87;

    //=====================
    //	SUMS
    //=====================

    for(int i=0; i<pub->allPoints; i++) {
        priv[i].in_partial_sum = (fp *)malloc(sizeof(fp) * 2*pub->tSize+1);
        priv[i].in_sqr_partial_sum = (fp *)malloc(sizeof(fp) * 2*pub->tSize+1);
        priv[i].par_max_val = (fp *)malloc(sizeof(fp) * (2*pub->tSize+2*pub->sSize+1));
        priv[i].par_max_coo = (int *)malloc(sizeof(int) * (2*pub->tSize+2*pub->sSize+1));
    }

    //=====================
    // 	INPUT 2 (SAMPLE AROUND POINT)
    //=====================

    pub->in2_rows = 2 * pub->sSize + 1;
    pub->in2_cols = 2 * pub->sSize + 1;
    pub->in2_elem = pub->in2_rows * pub->in2_cols;
    pub->in2_mem = sizeof(fp) * pub->in2_elem;

    for(int i=0; i < pub->allPoints; i++) {
        priv[i].d_in2 = (fp *)malloc(pub->in2_mem);
        priv[i].d_in2_sqr = (fp *)malloc(pub->in2_mem);
    }

    //=====================
    // 	INPUT (POINT TEMPLATE)
    //=====================

    pub->in_mod_rows = pub->tSize+1+pub->tSize;
    pub->in_mod_cols = pub->in_mod_rows;
    pub->in_mod_elem = pub->in_mod_rows * pub->in_mod_cols;
    pub->in_mod_mem = sizeof(fp) * pub->in_mod_elem;

    for(int i=0; i < pub->allPoints; i++) {
        priv[i].d_in_mod = (fp *)malloc(pub->in_mod_mem);
        priv[i].d_in_sqr = (fp *)malloc(pub->in_mod_mem);
    }

    //=====================
    // 	ARRAY OF TEMPLATES FOR ALL POINTS
    //=====================

    pub->d_endoT = (fp *)malloc(pub->in_mod_mem * pub->endoPoints);
    pub->d_epiT = (fp *)malloc(pub->in_mod_mem * pub->epiPoints);

    //=====================
    // 	SETUP priv POINTERS TO ROWS, COLS  AND TEMPLATE
    //=====================

    for(int i=0; i< pub->endoPoints; i++) {
        priv[i].point_no = i;
        priv[i].in_pointer = priv[i].point_no * pub->in_mod_elem;
        priv[i].d_Row = pub->d_endoRow; // original row coordinates
        priv[i].d_Col = pub->d_endoCol; // original col coordinates
        priv[i].d_tRowLoc = pub->d_tEndoRowLoc; // updated row coordinates
        priv[i].d_tColLoc = pub->d_tEndoColLoc; // updated row coordinates
        priv[i].d_T = pub->d_endoT; // templates
    }

    for(int i = pub->endoPoints; i < pub->allPoints; i++) {
        priv[i].point_no = i-pub->endoPoints;
        priv[i].in_pointer = priv[i].point_no * pub->in_mod_elem;
        priv[i].d_Row = pub->d_epiRow;
        priv[i].d_Col = pub->d_epiCol;
        priv[i].d_tRowLoc = pub->d_tEpiRowLoc;
        priv[i].d_tColLoc = pub->d_tEpiColLoc;
        priv[i].d_T = pub->d_epiT;
    }

    //=====================
    // 	CONVOLUTION
    //=====================
    
    pub->ioffset = 0;
    pub->joffset = 0;
    pub->conv_rows = pub->in_mod_rows + pub->in2_rows - 1; // number of rows in I
    pub->conv_cols = pub->in_mod_cols + pub->in2_cols - 1; // number of columns in I
    pub->conv_elem = pub->conv_rows * pub->conv_cols; // number of elements
    pub->conv_mem = sizeof(fp) * pub->conv_elem;
    for(int i=0; i < pub->allPoints; i++) {
        priv[i].d_conv = (fp *)malloc(pub->conv_mem);
    }

    //=====================
    // 	CUMULATIVE SUM
    //=====================

    //====================================================================================================
    //	PAD ARRAY
    //====================================================================================================
    //====================================================================================================
    //	VERTICAL CUMULATIVE SUM
    //====================================================================================================

    pub->in2_pad_add_rows = pub->in_mod_rows;
    pub->in2_pad_add_cols = pub->in_mod_cols;
    pub->in2_pad_rows = pub->in2_rows + 2*pub->in2_pad_add_rows;
    pub->in2_pad_cols = pub->in2_cols + 2*pub->in2_pad_add_cols;
    pub->in2_pad_elem = pub->in2_pad_rows * pub->in2_pad_cols;
    pub->in2_pad_mem = sizeof(fp) * pub->in2_pad_elem;

    for(int i=0; i < pub->allPoints; i++) {
        priv[i].d_in2_pad = (fp *)malloc(pub->in2_pad_mem);
    }

    //====================================================================================================
    //	SELECTION, SELECTION 2, SUBTRACTION
    //====================================================================================================
    //====================================================================================================
    //	HORIZONTAL CUMULATIVE SUM
    //====================================================================================================
    
    pub->in2_pad_cumv_sel_rowlow = 1 + pub->in_mod_rows; // (1 to n+1)
    pub->in2_pad_cumv_sel_rowhig = pub->in2_pad_rows - 1;
    pub->in2_pad_cumv_sel_collow = 1;
    pub->in2_pad_cumv_sel_colhig = pub->in2_pad_cols;
    pub->in2_pad_cumv_sel2_rowlow = 1;
    pub->in2_pad_cumv_sel2_rowhig = pub->in2_pad_rows - pub->in_mod_rows - 1;
    pub->in2_pad_cumv_sel2_collow = 1;
    pub->in2_pad_cumv_sel2_colhig = pub->in2_pad_cols;
    pub->in2_sub_rows = pub->in2_pad_cumv_sel_rowhig - pub->in2_pad_cumv_sel_rowlow + 1;
    pub->in2_sub_cols = pub->in2_pad_cumv_sel_colhig - pub->in2_pad_cumv_sel_collow + 1;
    pub->in2_sub_elem = pub->in2_sub_rows * pub->in2_sub_cols;
    pub->in2_sub_mem = sizeof(fp) * pub->in2_sub_elem;

    for(int i=0; i < pub->allPoints; i++) {
        priv[i].d_in2_sub = (fp *)malloc(pub->in2_sub_mem);
    }

    //====================================================================================================
    //	SELECTION, SELECTION 2, SUBTRACTION, SQUARE, NUMERATOR
    //====================================================================================================

    pub->in2_sub_cumh_sel_rowlow = 1;
    pub->in2_sub_cumh_sel_rowhig = pub->in2_sub_rows;
    pub->in2_sub_cumh_sel_collow = 1 + pub->in_mod_cols;
    pub->in2_sub_cumh_sel_colhig = pub->in2_sub_cols - 1;
    pub->in2_sub_cumh_sel2_rowlow = 1;
    pub->in2_sub_cumh_sel2_rowhig = pub->in2_sub_rows;
    pub->in2_sub_cumh_sel2_collow = 1;
    pub->in2_sub_cumh_sel2_colhig = pub->in2_sub_cols - pub->in_mod_cols - 1;
    pub->in2_sub2_sqr_rows = pub->in2_sub_cumh_sel_rowhig - pub->in2_sub_cumh_sel_rowlow + 1;
    pub->in2_sub2_sqr_cols = pub->in2_sub_cumh_sel_colhig - pub->in2_sub_cumh_sel_collow + 1;
    pub->in2_sub2_sqr_elem = pub->in2_sub2_sqr_rows * pub->in2_sub2_sqr_cols;
    pub->in2_sub2_sqr_mem = sizeof(fp) * pub->in2_sub2_sqr_elem;

    for(int i=0; i < pub->allPoints; i++) {
        priv[i].d_in2_sub2_sqr = (fp *)malloc(pub->in2_sub2_sqr_mem);
    }

    //=====================
    //	CUMULATIVE SUM 2
    //=====================

    //====================================================================================================
    //	PAD ARRAY
    //====================================================================================================
    //====================================================================================================
    //	VERTICAL CUMULATIVE SUM
    //====================================================================================================

    //====================================================================================================
    //	SELECTION, SELECTION 2, SUBTRACTION
    //====================================================================================================
    //====================================================================================================
    //	HORIZONTAL CUMULATIVE SUM
    //====================================================================================================

    //====================================================================================================
    //	SELECTION, SELECTION 2, SUBTRACTION, DIFFERENTIAL LOCAL SUM, DENOMINATOR A, DENOMINATOR, CORRELATION
    //====================================================================================================

    //=====================
    //	TEMPLATE MASK CREATE
    //=====================

    pub->tMask_rows = pub->in_mod_rows + (pub->sSize+1+pub->sSize) - 1;
    pub->tMask_cols = pub->tMask_rows;
    pub->tMask_elem = pub->tMask_rows * pub->tMask_cols;
    pub->tMask_mem = sizeof(fp) * pub->tMask_elem;

    for(int i=0; i < pub->allPoints; i++) {
        priv[i].d_tMask = (fp *)malloc(pub->tMask_mem);
    }

    //=====================
    //	POINT MASK INITIALIZE
    //=====================

    pub->mask_rows = pub->maxMove;
    pub->mask_cols = pub->mask_rows;
    pub->mask_elem = pub->mask_rows * pub->mask_cols;
    pub->mask_mem = sizeof(fp) * pub->mask_elem;

    //=====================
    //	MASK CONVOLUTION
    //=====================

    pub->mask_conv_rows = pub->tMask_rows; // number of rows in I
    pub->mask_conv_cols = pub->tMask_cols; // number of columns in I
    pub->mask_conv_elem = pub->mask_conv_rows * pub->mask_conv_cols; // number of elements
    pub->mask_conv_mem = sizeof(fp) * pub->mask_conv_elem;
    pub->mask_conv_ioffset = (pub->mask_rows-1)/2;
    if((pub->mask_rows-1) % 2 > 0.5) {
        pub->mask_conv_ioffset = pub->mask_conv_ioffset + 1;
    }
    pub->mask_conv_joffset = (pub->mask_cols-1)/2;
    if((pub->mask_cols-1) % 2 > 0.5) {
        pub->mask_conv_joffset = pub->mask_conv_joffset + 1;
    }

    for(int i=0; i < pub->allPoints; i++) {
        priv[i].d_mask_conv = (fp *)malloc(pub->mask_conv_mem);
    }
}

static void cleanup(public_struct *pub, private_struct *priv) {

    //====================================================================================================
    //	POINTERS
    //====================================================================================================

    for(int i=0; i < pub->allPoints; i++) {
        free(priv[i].in_partial_sum);
        free(priv[i].in_sqr_partial_sum);
        free(priv[i].par_max_val);
        free(priv[i].par_max_coo);

        free(priv[i].d_in2);
        free(priv[i].d_in2_sqr);
        free(priv[i].d_in_mod);
        free(priv[i].d_in_sqr);

        free(priv[i].d_conv);
        free(priv[i].d_in2_pad);
        free(priv[i].d_in2_sub);
        free(priv[i].d_in2_sub2_sqr);
        free(priv[i].d_tMask);
        free(priv[i].d_mask_conv);
    }

    //====================================================================================================
    //	COMMON
    //====================================================================================================

    free(pub->d_endoRow);
    free(pub->d_endoCol);
    free(pub->d_tEndoRowLoc);
    free(pub->d_tEndoColLoc);
    free(pub->d_endoT);

    free(pub->d_epiRow);
    free(pub->d_epiCol);
    free(pub->d_tEpiRowLoc);
    free(pub->d_tEpiColLoc);
    free(pub->d_epiT);
}

//==============================================================================
//==============================================================================
//	MAIN FUNCTION
//==============================================================================
//==============================================================================
int main(int argc, char *argv []) {
#if (!RACE_DETECT) && REACH_MAINT
  futurerd_disable_shadowing();
#endif
  
    //=====================
    //	VARIABLES
    //=====================

    // counters
    int frames_processed;

    // parameters
    public_struct pub;
    private_struct priv[ALL_POINTS];
    
    // futurerd::set_policy(futurerd::DetectPolicy::SILENT);
 
    //=====================
    // 	FRAMES
    //=====================
    if(argc!=4) {
        printf("ERROR: usage: heartwall <inputfile> <num of frames> <num of threads>\n");
        exit(1);
    }

    ensure_serial_execution();

    char* video_file_name;
    video_file_name = argv[1];

    avi_t* d_frames = (avi_t*)AVI_open_input_file(video_file_name, 1); // added casting
    if (d_frames == NULL)  {
        AVI_print_error((char *) "Error with AVI_open_input_file");
        return -1;
    }

    pub.d_frames = d_frames;
    pub.frames = AVI_video_frames(pub.d_frames);
    pub.frame_rows = AVI_video_height(pub.d_frames);
    pub.frame_cols = AVI_video_width(pub.d_frames);
    pub.frame_elem = pub.frame_rows * pub.frame_cols;
    pub.frame_mem = sizeof(fp) * pub.frame_elem;

    //=====================
    // 	CHECK INPUT ARGUMENTS
    //=====================
    frames_processed = atoi(argv[2]);
    if(frames_processed<0 || frames_processed>pub.frames) {
        printf("ERROR: %d is an incorrect number of frames specified\n.", frames_processed);
        printf("Select in the range of 0-%d\n", pub.frames);
        return 0;
    }

    //=====================
    //	INPUTS
    //=====================
    init_public_and_private_struct(&pub, priv);

    //=====================
    //	PRINT FRAME PROGRESS START
    //=====================
    printf("frame progress: ");
    fflush(NULL);

    //=====================
    //	KERNEL
    //=====================
    auto start = std::chrono::steady_clock::now();
    for(pub.frame_no=0; pub.frame_no<frames_processed; pub.frame_no++) {
        //====================================================================================================
        //	GETTING FRAME
        //====================================================================================================

        // Extract a cropped version of the first frame from the video file
        pub.d_frame = get_frame(pub.d_frames, // pointer to video file
                pub.frame_no, // number of frame that needs to be returned
                0,  // cropped?
                0,  // scaled?
                1); // converted

        //====================================================================================================
        //	PROCESSING
        //====================================================================================================
        cilk_for(int i=0; i<pub.allPoints; i++) {
          compute_kernel(&pub, &(priv[i]));
        }

        //====================================================================================================
        //	FREE MEMORY FOR FRAME
        //====================================================================================================

        // free frame after each loop iteration, since AVI library allocates memory for every frame fetched
        free(pub.d_frame);

        //====================================================================================================
        //	PRINT FRAME PROGRESS
        //====================================================================================================
        printf("%d ", pub.frame_no);
        fflush(NULL);
    }

    //=====================
    //	PRINT FRAME PROGRESS END
    //=====================
    printf("\n");
    fflush(NULL);

    auto end = std::chrono::steady_clock::now();
    auto time = std::chrono::duration <double, std::milli> (end-start).count();
    printf("Benchmark time: %f ms\n", time);

    //=====================
    //	DEALLOCATION
    //=====================

    //==================================================
    //	DUMP DATA TO FILE
    //==================================================
#ifdef OUTPUT
    write_data("result.txt",
            pub.frames,
            frames_processed,		
            pub.endoPoints,
            pub.d_tEndoRowLoc,
            pub.d_tEndoColLoc,
            pub.epiPoints,
            pub.d_tEpiRowLoc,
            pub.d_tEpiColLoc);

#endif
    cleanup(&pub, priv);

    return 0;
}

//=======================================================================
//=======================================================================
//	END OF FILE
//=======================================================================
//=======================================================================
