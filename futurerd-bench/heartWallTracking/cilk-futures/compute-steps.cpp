#include <assert.h> 
#include <math.h> 
#include <future.hpp>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include "define.hpp"

//=========================================================================
//=========================================================================
//	COMPUTE FUNCTION
//=========================================================================
//=========================================================================

// compute_startup is only invoked for when frame_no = 0
void compute_startup(const public_struct *pub, private_struct *priv) {
    // generate templates based on the first frame only
    // update temporary row/col coordinates
    int pointer = priv->point_no * pub->frames + pub->frame_no;
    priv->d_tRowLoc[pointer] = priv->d_Row[priv->point_no];
    priv->d_tColLoc[pointer] = priv->d_Col[priv->point_no];

    // pointers to: current frame, template for current point
    fp *d_in = &priv->d_T[priv->in_pointer];

    // update template, limit the number of working threads to the size of template
    for(int col=0; col<pub->in_mod_cols; col++) {
        for(int row=0; row<pub->in_mod_rows; row++) {
            // figure out row/col location in corresponding new template area in image 
            // and give to every thread (get top left corner and progress down and right)
            int ori_row = priv->d_Row[priv->point_no] - 25 + row - 1;
            int ori_col = priv->d_Col[priv->point_no] - 25 + col - 1;
            int ori_pointer = ori_col*pub->frame_rows+ori_row;
            // update template
            d_in[col*pub->in_mod_rows+row] = pub->d_frame[ori_pointer];
        }
    }
}

// All compute steps are for when pub->frame_no != 0
int compute_step1(const public_struct *pub, private_struct *priv) {
    //===============================
    //	PROCESS POINTS
    //===============================

    // process points in all frames except for the first one
    //====================================================================
    //	INPUTS
    //====================================================================

    //====================================================================
    //	1) SETUP POINTER TO POINT TO CURRENT FRAME FROM BATCH
    //	2) SELECT INPUT 2 (SAMPLE AROUND POINT) FROM FRAME    SAVE IN d_in2
    //	        (NOT LINEAR IN MEMORY, SO NEED TO SAVE OUTPUT FOR LATER EASY USE)
    //	3) SQUARE INPUT 2    SAVE IN d_in2_sqr
    //====================================================================

    // pointers and variables
    int in2_rowlow = priv->d_Row[priv->point_no] - pub->sSize; // (1 to n+1)
    int in2_collow = priv->d_Col[priv->point_no] - pub->sSize;

    // work
    for(int col=0; col<pub->in2_cols; col++) {
        for(int row=0; row<pub->in2_rows; row++) {
            // figure out corresponding location in old matrix and copy values to new matrix
            int ori_row = row + in2_rowlow - 1;
            int ori_col = col + in2_collow - 1;
            fp temp = pub->d_frame[ori_col*pub->frame_rows+ori_row];
            priv->d_in2[col*pub->in2_rows+row] = temp;
            priv->d_in2_sqr[col*pub->in2_rows+row] = temp*temp;
        }
    }

    return pub->frame_no;
}

int compute_step2(const public_struct *pub, private_struct *priv) {
    //==================================================
    //	1) GET POINTER TO INPUT 1 (TEMPLATE FOR THIS POINT) IN TEMPLATE ARRAY
    //	        (LINEAR IN MEMORY, SO DONT NEED TO SAVE, JUST GET POINTER)
    //	2) ROTATE INPUT 1    SAVE IN d_in_mod
    //	3) SQUARE INPUT 1    SAVE IN d_in_sqr
    //==================================================

    // variables
    fp *d_in = &(priv->d_T[priv->in_pointer]);

    // work
    for(int col=0; col<pub->in_mod_cols; col++) {
        for(int row=0; row<pub->in_mod_rows; row++) {
            // rotated coordinates
            int rot_row = (pub->in_mod_rows-1) - row;
            int rot_col = (pub->in_mod_rows-1) - col;
            int pointer = rot_col*pub->in_mod_rows+rot_row;
            // execution
            fp temp = d_in[pointer];
            priv->d_in_mod[col*pub->in_mod_rows+row] = temp;
            priv->d_in_sqr[pointer] = temp * temp;
        }
    }

    return pub->frame_no;
}

int compute_step3(const public_struct *pub, private_struct *priv) {

    //==================================================
    //	1) GET SUM OF INPUT 1
    //	2) GET SUM OF INPUT 1 SQUARED
    //==================================================

    fp *d_in = &(priv->d_T[priv->in_pointer]);

    // ANGE: compute_part3: if(frame_no != 0) kenel_part3
    priv->in_final_sum = 0;
    for(int i = 0; i < pub->in_mod_elem; i++) {
        priv->in_final_sum = priv->in_final_sum + d_in[i];
    }

    fp in_sqr_final_sum = 0;
    for(int i = 0; i < pub->in_mod_elem; i++) {
        in_sqr_final_sum = in_sqr_final_sum + priv->d_in_sqr[i];
    }

    //==================================================
    //	3) DO STATISTICAL CALCULATIONS
    //	4) GET DENOMINATOR T
    //==================================================

    fp mean = priv->in_final_sum / pub->in_mod_elem; // gets mean (average) value of element in ROI
    fp mean_sqr = mean * mean;
    fp variance  = (in_sqr_final_sum / pub->in_mod_elem) - mean_sqr; // gets variance of ROI
    fp deviation = sqrt(variance); // gets standard deviation of ROI

    priv->denomT = sqrt((fp)(pub->in_mod_elem-1))*deviation;

    return pub->frame_no;
}

int compute_step4(const public_struct *pub, private_struct *priv) {

    //=====================================================================
    //	1) CONVOLVE INPUT 2 WITH ROTATED INPUT 1    SAVE IN d_conv
    //=====================================================================

    // work
    for(int col=1; col <= pub->conv_cols; col++) {
        // column setup
        int j = col + pub->joffset;
        int jp1 = j + 1;
        int ja1, ja2;
        if(pub->in2_cols < jp1) { ja1 = jp1 - pub->in2_cols; }
        else{ ja1 = 1; }

        if(pub->in_mod_cols < j) { ja2 = pub->in_mod_cols; }
        else{ ja2 = j; }

        for(int row=1; row<=pub->conv_rows; row++) {
            // row range setup
            int i = row + pub->ioffset;
            int ip1 = i + 1;
            int ia1, ia2;

            if(pub->in2_rows < ip1) { ia1 = ip1 - pub->in2_rows; }
            else{ ia1 = 1; }

            if(pub->in_mod_rows < i) { ia2 = pub->in_mod_rows; }
            else{ ia2 = i; }

            fp s = 0;
            // getting data
            for(int ja=ja1; ja<=ja2; ja++) {
                int jb = jp1 - ja;
                for(int ia=ia1; ia<=ia2; ia++) {
                    int ib = ip1 - ia;
                    s = s + (priv->d_in_mod[pub->in_mod_rows*(ja-1)+ia-1] * 
                             priv->d_in2[pub->in2_rows*(jb-1)+ib-1]);
                }
            }
            priv->d_conv[(col-1)*pub->conv_rows+(row-1)] = s;
        }
    }

    return pub->frame_no;
}

int compute_step5(const public_struct *pub, private_struct *priv) {

    //=====================================================================
    //	LOCAL SUM 1
    //=====================================================================
    //==================================================
    //	1) PADD ARRAY    SAVE IN d_in2_pad
    //==================================================

    // work
    for(int col=0; col<pub->in2_pad_cols; col++) {
        for(int row=0; row<pub->in2_pad_rows; row++) {
            // execution
            if(row > (pub->in2_pad_add_rows-1) && // do if has numbers in original array
                    row < (pub->in2_pad_add_rows+pub->in2_rows) && 
                    col > (pub->in2_pad_add_cols-1) && 
                    col < (pub->in2_pad_add_cols+pub->in2_cols)) {
                int ori_row = row - pub->in2_pad_add_rows;
                int ori_col = col - pub->in2_pad_add_cols;
                priv->d_in2_pad[col*pub->in2_pad_rows+row] = 
                            priv->d_in2[ori_col*pub->in2_rows+ori_row];
            } else{ // do if otherwise
                priv->d_in2_pad[col*pub->in2_pad_rows+row] = 0;
            }
        }
    }

    //==================================================
    //	1) GET VERTICAL CUMULATIVE SUM    SAVE IN d_in2_pad
    //==================================================

    for(int ei_new = 0; ei_new < pub->in2_pad_cols; ei_new++) {
        // figure out column position
        int pos_ori = ei_new*pub->in2_pad_rows;
        // loop through all rows
        int sum = 0;
        for(int position = pos_ori; position < pos_ori+pub->in2_pad_rows; position = position + 1) {
            priv->d_in2_pad[position] = priv->d_in2_pad[position] + sum;
            sum = priv->d_in2_pad[position];
        }
    }

    //==================================================
    //	1) MAKE 1st SELECTION FROM VERTICAL CUMULATIVE SUM
    //	2) MAKE 2nd SELECTION FROM VERTICAL CUMULATIVE SUM
    //	3) SUBTRACT THE TWO SELECTIONS    SAVE IN d_in2_sub
    //==================================================

    // work
    for(int col=0; col<pub->in2_sub_cols; col++) {
        for(int row=0; row<pub->in2_sub_rows; row++) {
            // figure out corresponding location in old matrix and copy values to new matrix
            int ori_row = row + pub->in2_pad_cumv_sel_rowlow - 1;
            int ori_col = col + pub->in2_pad_cumv_sel_collow - 1;
            fp temp = priv->d_in2_pad[ori_col*pub->in2_pad_rows+ori_row];
            // figure out corresponding location in old matrix and copy values to new matrix
            ori_row = row + pub->in2_pad_cumv_sel2_rowlow - 1;
            ori_col = col + pub->in2_pad_cumv_sel2_collow - 1;
            fp temp2 = priv->d_in2_pad[ori_col*pub->in2_pad_rows+ori_row];
            // subtraction
            priv->d_in2_sub[col*pub->in2_sub_rows+row] = temp - temp2;
        }
    }

    return pub->frame_no;
}

int compute_step6(const public_struct *pub, private_struct *priv) {

    //==================================================
    //	1) GET HORIZONTAL CUMULATIVE SUM    SAVE IN d_in2_sub
    //==================================================

    for(int ei_new = 0; ei_new < pub->in2_sub_rows; ei_new++) {
        // figure out row position
        int pos_ori = ei_new;
        // loop through all rows
        int sum = 0;
        for(int position = pos_ori; 
                position < pos_ori+pub->in2_sub_elem; 
                position = position + pub->in2_sub_rows) {
            priv->d_in2_sub[position] = priv->d_in2_sub[position] + sum;
            sum = priv->d_in2_sub[position];
        }
    }

    //==================================================
    //	1) MAKE 1st SELECTION FROM HORIZONTAL CUMULATIVE SUM
    //	2) MAKE 2nd SELECTION FROM HORIZONTAL CUMULATIVE SUM
    //	3) SUBTRACT THE TWO SELECTIONS TO GET LOCAL SUM 1
    //	4) GET CUMULATIVE SUM 1 SQUARED    SAVE IN d_in2_sub2_sqr
    //	5) GET NUMERATOR    SAVE IN d_conv
    //==================================================

    // work
    for(int col=0; col<pub->in2_sub2_sqr_cols; col++) {
        for(int row=0; row<pub->in2_sub2_sqr_rows; row++) {
            // figure out corresponding location in old matrix and copy values to new matrix
            int ori_row = row + pub->in2_sub_cumh_sel_rowlow - 1;
            int ori_col = col + pub->in2_sub_cumh_sel_collow - 1;
            fp temp = priv->d_in2_sub[ori_col*pub->in2_sub_rows+ori_row];

            // figure out corresponding location in old matrix and copy values to new matrix
            ori_row = row + pub->in2_sub_cumh_sel2_rowlow - 1;
            ori_col = col + pub->in2_sub_cumh_sel2_collow - 1;
            fp temp2 = priv->d_in2_sub[ori_col*pub->in2_sub_rows+ori_row];
            // subtraction
            temp2 = temp - temp2;
            // squaring
            priv->d_in2_sub2_sqr[col*pub->in2_sub2_sqr_rows+row] = temp2 * temp2; 
            // numerator
            priv->d_conv[col*pub->in2_sub2_sqr_rows+row] =  
                priv->d_conv[col*pub->in2_sub2_sqr_rows+row] - 
                temp2 * priv->in_final_sum / pub->in_mod_elem;
        }
    }

    return pub->frame_no;
}

int compute_step7(const public_struct *pub, private_struct *priv) {

    //===========================================================
    //	LOCAL SUM 2
    //===========================================================
    //==================================================
    //	1) PAD ARRAY    SAVE IN d_in2_pad
    //==================================================

    // work
    for(int col=0; col<pub->in2_pad_cols; col++) {
        for(int row=0; row<pub->in2_pad_rows; row++) {
            // execution
            if(row > (pub->in2_pad_add_rows-1) && // do if has numbers in original array
                    row < (pub->in2_pad_add_rows+pub->in2_rows) && 
                    col > (pub->in2_pad_add_cols-1) && 
                    col < (pub->in2_pad_add_cols+pub->in2_cols)) {
                int ori_row = row - pub->in2_pad_add_rows;
                int ori_col = col - pub->in2_pad_add_cols;
                priv->d_in2_pad[col*pub->in2_pad_rows+row] = 
                    priv->d_in2_sqr[ori_col*pub->in2_rows+ori_row];
            } else{ // do if otherwise
                priv->d_in2_pad[col*pub->in2_pad_rows+row] = 0;
            }
        }
    }

    //==================================================
    //	2) GET VERTICAL CUMULATIVE SUM    SAVE IN d_in2_pad
    //==================================================

    //work
    for(int ei_new = 0; ei_new < pub->in2_pad_cols; ei_new++) {
        // figure out column position
        int pos_ori = ei_new*pub->in2_pad_rows;
        // loop through all rows
        int sum = 0;
        for(int position = pos_ori; position < pos_ori+pub->in2_pad_rows; position = position + 1) {
            priv->d_in2_pad[position] = priv->d_in2_pad[position] + sum;
            sum = priv->d_in2_pad[position];
        }
    }

    return pub->frame_no;
}

int compute_step8(const public_struct *pub, private_struct *priv) {

    //==================================================
    //	1) MAKE 1st SELECTION FROM VERTICAL CUMULATIVE SUM
    //	2) MAKE 2nd SELECTION FROM VERTICAL CUMULATIVE SUM
    //	3) SUBTRACT THE TWO SELECTIONS    SAVE IN d_in2_sub
    //==================================================

    // ANGE: compute_part8: if(frame_no != 0) kenel_part8
    // work
    for(int col=0; col<pub->in2_sub_cols; col++) {
        for(int row=0; row<pub->in2_sub_rows; row++) {
            // figure out corresponding location in old matrix and copy values to new matrix
            int ori_row = row + pub->in2_pad_cumv_sel_rowlow - 1;
            int ori_col = col + pub->in2_pad_cumv_sel_collow - 1;
            fp temp = priv->d_in2_pad[ori_col*pub->in2_pad_rows+ori_row];

            // figure out corresponding location in old matrix and copy values to new matrix
            ori_row = row + pub->in2_pad_cumv_sel2_rowlow - 1;
            ori_col = col + pub->in2_pad_cumv_sel2_collow - 1;
            fp temp2 = priv->d_in2_pad[ori_col*pub->in2_pad_rows+ori_row];
            // subtract
            priv->d_in2_sub[col*pub->in2_sub_rows+row] = temp - temp2;
        }
    }

    //==================================================
    //	1) GET HORIZONTAL CUMULATIVE SUM   SAVE IN d_in2_sub
    //==================================================

    for(int ei_new = 0; ei_new < pub->in2_sub_rows; ei_new++) {
        // figure out row position
        int pos_ori = ei_new;
        // loop through all rows
        int sum = 0;
        for(int position = pos_ori; 
                position < pos_ori+pub->in2_sub_elem; 
                position = position + pub->in2_sub_rows) {
            priv->d_in2_sub[position] = priv->d_in2_sub[position] + sum;
            sum = priv->d_in2_sub[position];
        }
    }

    //==================================================
    //	1) MAKE 1st SELECTION FROM HORIZONTAL CUMULATIVE SUM
    //	2) MAKE 2nd SELECTION FROM HORIZONTAL CUMULATIVE SUM
    //	3) SUBTRACT THE TWO SELECTIONS TO GET LOCAL SUM 2
    //	4) GET DIFFERENTIAL LOCAL SUM
    //	5) GET DENOMINATOR A
    //	6) GET DENOMINATOR
    //	7) DIVIDE NUMBERATOR BY DENOMINATOR TO GET CORRELATION	SAVE IN d_conv
    //==================================================

    // work
    for(int col=0; col<pub->conv_cols; col++) {
        for(int row=0; row<pub->conv_rows; row++) {
            // figure out corresponding location in old matrix and copy values to new matrix
            int ori_row = row + pub->in2_sub_cumh_sel_rowlow - 1;
            int ori_col = col + pub->in2_sub_cumh_sel_collow - 1;
            fp temp = priv->d_in2_sub[ori_col*pub->in2_sub_rows+ori_row];

            // figure out corresponding location in old matrix and copy values to new matrix
            ori_row = row + pub->in2_sub_cumh_sel2_rowlow - 1;
            ori_col = col + pub->in2_sub_cumh_sel2_collow - 1;
            fp temp2 = priv->d_in2_sub[ori_col*pub->in2_sub_rows+ori_row];

            // subtract
            temp2 = temp - temp2;

            // diff_local_sums
            temp2 = temp2 - (priv->d_in2_sub2_sqr[col*pub->conv_rows+row] / pub->in_mod_elem);

            // denominator A
            if(temp2 < 0) {
                temp2 = 0;
            }
            temp2 = sqrt(temp2);
            // denominator
            temp2 = priv->denomT * temp2;
            // correlation
            priv->d_conv[col*pub->conv_rows+row] = priv->d_conv[col*pub->conv_rows+row] / temp2;
        }
    }

    return pub->frame_no;
}

int compute_step9(const public_struct *pub, private_struct *priv) {

    //===================================================================
    //	TEMPLATE MASK CREATE
    //===================================================================

    // parameters
    int cent = pub->sSize + pub->tSize + 1;
    int pointer = pub->frame_no-1+priv->point_no*pub->frames;
    int tMask_row = cent + priv->d_tRowLoc[pointer] - priv->d_Row[priv->point_no] - 1;
    int tMask_col = cent + priv->d_tColLoc[pointer] - priv->d_Col[priv->point_no] - 1;

    // work
    for(int ei_new = 0; ei_new < pub->tMask_elem; ei_new++) {
        priv->d_tMask[ei_new] = 0;
    }
    priv->d_tMask[tMask_col*pub->tMask_rows + tMask_row] = 1;

    return pub->frame_no;
}

int compute_step10(const public_struct *pub, private_struct *priv) {

    //====================================================================================================
    //	1) MASK CONVOLUTION
    //	2) MULTIPLICATION
    //====================================================================================================

    // work
    for(int col=1; col<=pub->mask_conv_cols; col++) {
        // col setup
        int j = col + pub->mask_conv_joffset;
        int jp1 = j + 1;
        int ja1, ja2;
        if(pub->mask_cols < jp1) { ja1 = jp1 - pub->mask_cols; }
        else{ ja1 = 1; }

        if(pub->tMask_cols < j) { ja2 = pub->tMask_cols; }
        else{ ja2 = j; }

        for(int row=1; row<=pub->mask_conv_rows; row++) {
            // row setup
            int i = row + pub->mask_conv_ioffset;
            int ip1 = i + 1;
            int ia1, ia2;

            if(pub->mask_rows < ip1) { ia1 = ip1 - pub->mask_rows; }
            else{ ia1 = 1; }
            if(pub->tMask_rows < i) { ia2 = pub->tMask_rows; }
            else{ ia2 = i; }

            fp s = 0;
            // get data
            for(int ja=ja1; ja<=ja2; ja++) {
                for(int ia=ia1; ia<=ia2; ia++) {
                    s = s + priv->d_tMask[pub->tMask_rows*(ja-1)+ia-1] * 1;
                }
            }
            priv->d_mask_conv[(col-1)*pub->conv_rows+(row-1)] = 
                priv->d_conv[(col-1)*pub->conv_rows+(row-1)] * s;
        }
    }

    //====================================================================================================
    //	MAXIMUM VALUE
    //====================================================================================================

    //==================================================
    //	SEARCH
    //==================================================

    fp fin_max_val = 0;
    int fin_max_coo = 0;
    for(int i=0; i<pub->mask_conv_elem; i++) {
        if(priv->d_mask_conv[i]>fin_max_val) {
            fin_max_val = priv->d_mask_conv[i];
            fin_max_coo = i;
        }
    }

    //==================================================
    //	OFFSET
    //==================================================

    // convert coordinate to row/col form
    int largest_row = (fin_max_coo+1) % pub->mask_conv_rows - 1; // (0-n) row
    int largest_col = (fin_max_coo+1) / pub->mask_conv_rows; // (0-n) column
    if((fin_max_coo+1) % pub->mask_conv_rows == 0) {
        largest_row = pub->mask_conv_rows - 1;
        largest_col = largest_col - 1;
    }

    // calculate offset
    largest_row = largest_row + 1; // compensate to match MATLAB format (1-n)
    largest_col = largest_col + 1; // compensate to match MATLAB format (1-n)
    int offset_row = largest_row - pub->in_mod_rows - (pub->sSize - pub->tSize);
    int offset_col = largest_col - pub->in_mod_cols - (pub->sSize - pub->tSize);
    int pointer = priv->point_no*pub->frames+pub->frame_no;
    priv->d_tRowLoc[pointer] = priv->d_Row[priv->point_no] + offset_row;
    priv->d_tColLoc[pointer] = priv->d_Col[priv->point_no] + offset_col;

    //===============================
    //	COORDINATE AND TEMPLATE UPDATE
    //===============================

    // if the last frame in the batch, update template
    if(pub->frame_no%10 == 0) {

        fp *d_in = &priv->d_T[priv->in_pointer];

        // update coordinate 
        int loc_pointer = priv->point_no*pub->frames+pub->frame_no; 
        priv->d_Row[priv->point_no] = priv->d_tRowLoc[loc_pointer]; 
        priv->d_Col[priv->point_no] = priv->d_tColLoc[loc_pointer];

        // update template, limit the number of working threads to the size of template
        for(int col=0; col<pub->in_mod_cols; col++) {
            for(int row=0; row<pub->in_mod_rows; row++) {
                // figure out row/col location in corresponding new template area 
                // in image and give to every thread (get top left corner and progress down and right)
                int ori_row = priv->d_Row[priv->point_no] - 25 + row - 1;
                int ori_col = priv->d_Col[priv->point_no] - 25 + col - 1;
                int ori_pointer = ori_col*pub->frame_rows+ori_row;

                // update template
                d_in[col*pub->in_mod_rows+row] = 
                    pub->alpha*d_in[col*pub->in_mod_rows+row] + 
                    (1.00-pub->alpha)*pub->d_frame[ori_pointer];
            }
        }
    }

    return pub->frame_no;
}

#ifdef STRUCTURED_FUTURES
void compute_kernel(const public_struct *pub, private_struct *priv) {
    int frame_no = pub->frame_no;

    if(pub->frame_no == 0) {
        compute_startup(pub, priv);
    } else {
        int s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
        //cilk::future<int> f1, f2, f3, f4, f5, f6, f7, f8, f9, f10;
        char handle_mem[sizeof(cilk::future<int>) * 10];
        cilk::future<int>* fhandles = (cilk::future<int>*)handle_mem;

        reasync_helper<int, const public_struct *, private_struct *>
          (&fhandles[0], compute_step1, pub, priv);
        reasync_helper<int, const public_struct *, private_struct *>
          (&fhandles[1], compute_step2, pub, priv);
        reasync_helper<int, const public_struct *, private_struct *>
          (&fhandles[8], compute_step9, pub, priv);
        s2 = fhandles[1].get();

        reasync_helper<int, const public_struct *, private_struct *>
          (&fhandles[2], compute_step3, pub, priv);
        s1 = fhandles[0].get();

        reasync_helper<int, const public_struct *, private_struct *>
          (&fhandles[3], compute_step4, pub, priv);
        reasync_helper<int, const public_struct *, private_struct *>
          (&fhandles[4], compute_step5, pub, priv);
        s5 = fhandles[4].get();
        reasync_helper<int, const public_struct *, private_struct *>
          (&fhandles[6], compute_step7, pub, priv);
        s3 = fhandles[2].get();
        s4 = fhandles[3].get();
        reasync_helper<int, const public_struct *, private_struct *>
          (&fhandles[5], compute_step6, pub, priv);
        s7 = fhandles[6].get();
        s6 = fhandles[5].get();
        reasync_helper<int, const public_struct *, private_struct *>
          (&fhandles[7], compute_step8, pub, priv);
        s9 = fhandles[8].get();
        s8 = fhandles[7].get();
        reasync_helper<int, const public_struct *, private_struct *>
          (&fhandles[9], compute_step10, pub, priv);
        s10 = fhandles[9].get();

        assert(s1 == frame_no);
        assert(s2 == frame_no);
        assert(s3 == frame_no);
        assert(s4 == frame_no);
        assert(s5 == frame_no);
        assert(s6 == frame_no);
        assert(s7 == frame_no);
        assert(s8 == frame_no);
        assert(s9 == frame_no);
        assert(s10 == frame_no);
    }
}
#endif

#ifdef NONBLOCKING_FUTURES

int compute_step1_with_get(const public_struct *pub, private_struct *priv, 
                           cilk::future<int> *fhandles, int frame_no) {
  int s1 = compute_step1(pub, priv); 
  assert(s1 == frame_no);
  return s1;
}

int compute_step2_with_get(const public_struct *pub, private_struct *priv, 
                           cilk::future<int> *fhandles, int frame_no) {
  int s2 = compute_step2(pub, priv);
  assert(s2 == frame_no);
  return s2;
}

int compute_step3_with_get(const public_struct *pub, private_struct *priv, 
                           cilk::future<int> *fhandles, int frame_no) {
  int s2 = fhandles[1].get(); // step 2 needs to finish
  assert(s2 == frame_no);
  return compute_step3(pub, priv);
}

int compute_step4_with_get(const public_struct *pub, private_struct *priv, 
                           cilk::future<int> *fhandles, int frame_no) {
  int s1 = fhandles[0].get(); // step 1 needs to finish
  int s2 = fhandles[1].get(); // step 2 needs to finish
  assert(s1 == frame_no && s2 == frame_no);
  return compute_step4(pub, priv);
}

int compute_step5_with_get(const public_struct *pub, private_struct *priv, 
                           cilk::future<int> *fhandles, int frame_no) {
  int s1 = fhandles[0].get(); // step 1 needs to finish
  assert(s1 == frame_no);
  return compute_step5(pub, priv);
}

int compute_step6_with_get(const public_struct *pub, private_struct *priv, 
                           cilk::future<int> *fhandles, int frame_no) {
  int s3 = fhandles[2].get(); // step 3 needs to finish
  int s4 = fhandles[3].get(); // step 4 needs to finish
  int s5 = fhandles[4].get(); // step 5 needs to finish
  assert(s3 == frame_no && s4 == frame_no && s5 == frame_no);
  return compute_step6(pub, priv);
}

int compute_step7_with_get(const public_struct *pub, private_struct *priv, 
                           cilk::future<int> *fhandles, int frame_no) {
  int s5 = fhandles[4].get(); // step 5 needs to finish
  assert(s5 == frame_no);
  return compute_step7(pub, priv);
}

int compute_step8_with_get(const public_struct *pub, private_struct *priv, 
                           cilk::future<int> *fhandles, int frame_no) {
  int s6 = fhandles[5].get(); // step 6 needs to finish
  int s7 = fhandles[6].get(); // step 7 needs to finish
  assert(s6 == frame_no && s7 == frame_no);
  return compute_step8(pub, priv);
}

int compute_step9_with_get(const public_struct *pub, private_struct *priv, 
                           cilk::future<int> *fhandles, int frame_no) {
  int s9 = compute_step9(pub, priv);
  assert(s9 == frame_no);
  return s9;
}

int compute_step10_with_get(const public_struct *pub, private_struct *priv, 
                            cilk::future<int> *fhandles, int frame_no) {
  int s8 = fhandles[7].get(); // step 8 needs to finish
  int s9 = fhandles[8].get(); // step 9 needs to finish
  assert(s8 == frame_no && s9 == frame_no);
  return compute_step10(pub, priv);
}

typedef int (*compute_func_ptr_t) (const public_struct *, private_struct *, cilk::future<int> *, int);

void compute_kernel(const public_struct *pub, private_struct *priv) {
    
    int frame_no = pub->frame_no;

    if(pub->frame_no == 0) {
        compute_startup(pub, priv);
    } else {
        
        int s10 = 0;
        //cilk::future<int> fhandles[10];
        char handle_mem[sizeof(cilk::future<int>) * 10];
        cilk::future<int>* fhandles = (cilk::future<int>*)handle_mem;
        compute_func_ptr_t func_ptr[10] = { compute_step1_with_get, compute_step2_with_get, 
            compute_step3_with_get, compute_step4_with_get, compute_step5_with_get, 
            compute_step6_with_get, compute_step7_with_get, compute_step8_with_get, 
            compute_step9_with_get, compute_step10_with_get };

        // spawn off the computation; could be a sequential loop
        cilk_for(int i=0; i < 10; i++) {
          cilk::future<int> *f = &fhandles[i];
            // reuse_future(int, f, func_ptr[i], pub, priv, fhandles, frame_no); 
            reasync_helper<int, const public_struct *, private_struct *,
                           cilk::future<int> *, int>
              (f, func_ptr[i], pub, priv, fhandles, frame_no);
        }
        s10 = fhandles[9].get(); // make sure we finish the last step before returning
        assert(s10 == frame_no);
    }
}
#endif


#if 0
void compute_kernel(const public_struct *pub, private_struct *priv) {
    
    int frame_no = pub->frame_no;

    if(pub->frame_no == 0) {
        compute_startup(pub, priv);
    } else {
        int s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;

        s1 = compute_step1(pub, priv);
        s2 = compute_step2(pub, priv);
        s3 = compute_step3(pub, priv);
        s4 = compute_step4(pub, priv);
        s5 = compute_step5(pub, priv);
        s6 = compute_step6(pub, priv);
        s7 = compute_step7(pub, priv);
        s8 = compute_step8(pub, priv);
        s9 = compute_step9(pub, priv);
        s10 = compute_step10(pub, priv);
    
        assert(s1 == frame_no);
        assert(s2 == frame_no);
        assert(s3 == frame_no);
        assert(s4 == frame_no);
        assert(s5 == frame_no);
        assert(s6 == frame_no);
        assert(s7 == frame_no);
        assert(s8 == frame_no);
        assert(s9 == frame_no);
        assert(s10 == frame_no);
    }
}
#endif

