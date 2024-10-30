
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

#ifndef _DEF_IN_MAKEFILE_
#define MAX_S (4)
#define MAX_K (9)
#define MAX_BETA_LENGTH (128)
#define MAX_Tif (8)
#define MAX_Tof (32)
#define MAX_Tr (12)
#define MAX_Tc (12)
#define LANE_NUM (8)
#endif

#define MAX_IB_H ((MAX_Tr-1)*MAX_S+MAX_K)
#define MAX_IB_W ((MAX_Tc-1)*MAX_S+MAX_K)

const uint32_t MAX_KxK = MAX_K*MAX_K;
const uint32_t MAX_Trc = MAX_Tr*MAX_Tc;
const uint32_t MAX_IB_HW = MAX_IB_H*MAX_IB_W;
const uint32_t Tmax_dx = (MAX_Tof+LANE_NUM-1)/LANE_NUM;
const uint32_t Tnax_dx = (MAX_Tif+LANE_NUM-1)/LANE_NUM;

#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))

//w_factor=360,  720=2x, 1080=3x, 2160=6x
//h_factor=640, 1280=2x, 1920=3x, 3840=6x

int32_t f2i32(float val_f32){
	int32_t val_i32;
	union {
		float f32;
		int32_t i32;
	}tmp_32b;

	tmp_32b.f32 = val_f32;
	val_i32 = tmp_32b.i32;

	return val_i32;
}

float i2f32(int32_t val_i32){
	float val_f32;
	union {
		float f32;
		int32_t i32;
	}tmp_32b;

	tmp_32b.i32 = val_i32;
	val_f32 = tmp_32b.f32;

	return val_f32;
}


float post_process(float tmp_in, float bias, float ifm_ex, uint8_t mode, bool bias_en){

    float tmp_out;
    if(bias_en){
        tmp_in = tmp_in + bias;
    }

    tmp_out = 0;
    if(mode==0){
        tmp_out = tmp_in;
    }else{
        if(mode==1){
            tmp_out = ifm_ex*tmp_in;
        }else{
            tmp_out = ifm_ex/tmp_in;
            assert(mode==2);
        }
    }

    return tmp_out;
}

void codec_ofm_store_chw_tile(int16_t *ofm, int32_t ofm_buf[Tmax_dx][MAX_Trc][LANE_NUM], int16_t *ifm_extra, float b_buf[((MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM)][LANE_NUM],
    uint16_t m, uint16_t r, uint16_t c, uint16_t TM_MIN, uint16_t TR_MIN, uint16_t TC_MIN, uint32_t OHW,
    uint16_t OH, uint16_t OW, uint8_t mode, bool bias_en, bool enable, uint8_t subK_num, uint32_t *TRxTC_M_offset,
    float ifm_scale[(MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM][LANE_NUM], float ofm_scale[(MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM][LANE_NUM],
    float kernel_scale[(MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM][LANE_NUM]){
//mode=0 normal, 1=src0*sqrt(src1), 2=src0/sqrt(src1)

    if(!enable)
        return;

	// int base_offset = m*OHW + r*OW + c;
    int base_offset = (m/LANE_NUM)*OHW + r*OW + c;

	bool IsColCont = (OW==TC_MIN);//equal FMCont
	bool IsRowCont = (OH==TR_MIN);
	bool IsAllCont = IsColCont && IsRowCont;

	uint16_t TM_MINax_dx = (TM_MIN + LANE_NUM -1)/LANE_NUM;

	int burstlen;
	uint16_t T1_MIN, T2_MIN;
	if(IsAllCont){
		burstlen = TM_MINax_dx*TR_MIN*TC_MIN;
		T1_MIN = 1;
		T2_MIN = 1;
	}else if(IsColCont){
		burstlen = TR_MIN*TC_MIN;
		T1_MIN = TM_MINax_dx;
		T2_MIN = 1;
	}else{
		burstlen = TC_MIN;
		T1_MIN = TM_MINax_dx;
		T2_MIN = TR_MIN;
	}
    uint32_t Tcomb_TC = T1_MIN*T2_MIN;

	uint16_t tr,tm,tc;
	uint16_t t1, t2;

    uint32_t cnt[16];
    for(uint8_t k=0; k<subK_num;k++){
        cnt[k] = TRxTC_M_offset[k];
    }

	t1 = 0; t2 = 0;
	for(uint32_t tcb = 0; tcb < Tcomb_TC; tcb++){
		tc = 0;
		if(IsAllCont){
			tm = 0;
			tr = 0;
		}else if(IsColCont){
			tm = t1;
			tr = 0;
		}else{
			tm = t1;
			tr = t2;
		}

		uint32_t ofm_offset0 = base_offset;
		uint32_t ofm_offset;
		if(IsAllCont){
			ofm_offset = ofm_offset0;
		}else if(IsColCont){
			ofm_offset = ofm_offset0 + t1*OHW;
		}else{
			ofm_offset = ofm_offset0 + t1*OHW + t2*OW;
		}

		int16_t *OFM_base = ofm + ofm_offset*LANE_NUM;
        int16_t *IFM_ex_base = ifm_extra + ofm_offset*LANE_NUM;
		for(int tbl_min=0; tbl_min < burstlen; tbl_min++){

            uint8_t z;
            if(subK_num == 4){
                z = (tr & 0x1)*2 + (tc & 0x1);
            }else if(subK_num == 16){
                z = (tr & 0x3)*4 + (tc & 0x3);
            }else{//subK_num==1
                z = 0;
            }
            if((tr==0)&&(tc==0)){
                for(int k=0; k<subK_num;k++){
                    cnt[k] = TRxTC_M_offset[k];
                }
            }
            uint32_t ofm_idx = cnt[z];
            cnt[z] = cnt[z] + 1;

            // float tmp_out = post_process(ofm_buf[tm][tr*TC_MIN+tc], bias[m+tm], IFM_ex_base[tbl_min], mode, bias_en);
            int16_t tmp_ifm_ex[LANE_NUM];
            if(mode!=0){
                memcpy(tmp_ifm_ex, IFM_ex_base + (tbl_min*LANE_NUM), sizeof(int16_t)*LANE_NUM);
            }else{
                for(uint32_t x=0; x<LANE_NUM; x++){
                    tmp_ifm_ex[x] = 0;
                }
            }

			int16_t tmp_f32i[LANE_NUM];
			for(uint32_t x=0; x<LANE_NUM; x++){
                    tmp_f32i[x] = post_process(ofm_buf[tm][ofm_idx][x]*kernel_scale[m/LANE_NUM + tm][x], b_buf[m/LANE_NUM + tm][x],
                                                    tmp_ifm_ex[x]*ifm_scale[m/LANE_NUM + tm][x], mode, bias_en)
                                                    *ofm_scale[m/LANE_NUM + tm][x];
			}

			memcpy(OFM_base + (tbl_min*LANE_NUM), tmp_f32i, sizeof(int16_t)*LANE_NUM);

			tc++;
			if(tc==TC_MIN){
				tc = 0;
				tr++;
				if(tr==TR_MIN){
					tr = 0;
					tm++;
				}
			}
		}

		t2++;
		if(t2==T2_MIN){
			t2=0;
			t1++;
		}
	}
}

void load_kernel_reorg(int16_t k_buf[Tmax_dx][MAX_Tif][MAX_KxK][LANE_NUM], int16_t *kernel, uint8_t m, uint8_t n,
                            uint8_t TM_MIN, uint8_t TN_MIN, uint8_t K, uint8_t KxK, uint8_t wQ){

	static int Woffset;
	if(m==0&&n==0)
		Woffset = 0;

	// float *weight_addr = kernel + Woffset*LANE_NUM;
    int16_t *weight_addr = kernel + Woffset*LANE_NUM;

	// uint32_t burstlen = TM_MIN*TN_MIN*KxK;
    uint32_t burstlen = ((TM_MIN+LANE_NUM-1)/LANE_NUM)*TN_MIN*KxK;
	Woffset += burstlen;

	uint8_t t1,t2,t3;
	for(t3 = 0;t3 <KxK; t3++){
// DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=K)
// DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=K)
// #pragma HLS PIPELINE II=1
	for(t1 = 0;t1 < Tmax_dx; t1++){
// DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=Tm)
	for(t2 = 0;t2 < MAX_Tif; t2++){
// DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=Tn)
        for(uint32_t x =0; x<LANE_NUM; x++){
            k_buf[t1][t2][t3][x] = 0;
        }
	}}}

	t1 = 0; t2 = 0; t3 = 0;
	for(uint16_t t=0; t<burstlen; t++){
// DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=TmxTnxKxK)
// #pragma HLS PIPELINE II=1

		int16_t tmp_f32i[LANE_NUM];
		memcpy(tmp_f32i, weight_addr + (t*LANE_NUM), sizeof(int16_t)*LANE_NUM);
		// float tmp_f32i[LANE_NUM];
		// memcpy(tmp_f32i, weight_addr + (t*LANE_NUM), sizeof(float)*LANE_NUM);
		for(uint32_t x=0; x<LANE_NUM; x++){
            k_buf[t1][t2][t3][x] = tmp_f32i[x] >> wQ;
		}

        t3++;
        if(t3==KxK){
            t3=0; t2++;
            if(t2==TN_MIN){
                t2=0; t1++;
            }
        }
	}

}

void row_init_cfg(uint16_t *TRow_lps, uint16_t IR, uint8_t ps_sf_bit, uint8_t ps_mask,
                    uint8_t c_stride, uint8_t pad, uint8_t K, uint16_t r, uint16_t TR_MIN,
                    uint16_t *px_t, uint16_t *px_b, uint16_t *row_len, int16_t *row_t, int16_t *row_b, bool init_en){

    if(!init_en)
        return;

    static int16_t Roffset;

    uint16_t TRow = (TR_MIN - 1)*c_stride + K;
    uint16_t TRow_lps_tmp = TRow >> ps_sf_bit;
    Roffset = (r*c_stride - pad) >> ps_sf_bit;

    uint16_t px_t_tmp, px_b_tmp;
    bool px_t_flag = 1;
    for(int j = 0; j < TRow_lps_tmp; j++){

        int16_t yoffset = Roffset + j;
        bool y_en = (yoffset >= 0)&&(yoffset <IR);
        if(y_en){
            if(px_t_flag){
                px_t_tmp = j;
                px_t_flag = 0;
            }
            px_b_tmp = j;
        }
    }

    int16_t row_t_tmp, row_b_tmp;
    row_t_tmp = px_t_tmp+Roffset;
    row_b_tmp = px_b_tmp+Roffset;

    *TRow_lps = TRow_lps_tmp;
    *row_len = row_b_tmp - row_t_tmp + 1;
    *px_t = px_t_tmp;
    *px_b = px_b_tmp;
    *row_t = row_t_tmp;
    *row_b = row_b_tmp;
}

void col_init_cfg(uint16_t *TCol_lps, uint16_t IC, uint8_t ps_sf_bit, uint8_t ps_mask,
    uint8_t c_stride, uint8_t pad, uint8_t K, uint16_t c, uint16_t TC_MIN,
    uint16_t *px_l, uint16_t *px_r, int16_t *col_l, int16_t *col_r, uint16_t *col_len, bool init_en){

    if(!init_en)
        return;

    static int16_t Coffset;

    uint16_t TCol = (TC_MIN - 1)*c_stride + K;
    uint16_t TCol_lps_tmp = TCol >> ps_sf_bit;
    Coffset = (c*c_stride - pad) >> ps_sf_bit;

    uint16_t px_l_tmp, px_r_tmp;
    int16_t col_l_tmp, col_r_tmp;

    bool px_l_flag = 1;
    for(int i = 0; i < TCol_lps_tmp; i++){
        int16_t xoffset = Coffset + i;
        bool x_en = (xoffset >= 0)&&(xoffset <IC);
        if(x_en){
            if(px_l_flag){
                px_l_tmp = i;
                px_l_flag = 0;
            }
            px_r_tmp = i;
        }
    }

    col_l_tmp = px_l_tmp+Coffset;
    col_r_tmp = px_r_tmp+Coffset;

    *px_l = px_l_tmp;
    *px_r = px_r_tmp;
    *col_l = col_l_tmp;
    *col_r = col_r_tmp;
    *col_len = col_r_tmp - col_l_tmp + 1;
    *TCol_lps = TCol_lps_tmp;
}

void clean_ifm_bank(float *ifm_buf, int TRow, int TCol, bool cl_en){

    if(!cl_en)
        return;

    for(int j = 0; j < TRow; j++)
    for(int i = 0; i < TCol; i++){

        ifm_buf[i + TCol*j] = 0;
    }
}

void load_ifm_pad_square(int16_t ifm_buf[Tnax_dx][MAX_IB_HW][LANE_NUM], int16_t *ifm, uint16_t IR, uint16_t IC, uint8_t K, uint32_t IHxIW, uint8_t p_stride,
        uint8_t c_stride, uint8_t pad, uint8_t ps_sf_bit, uint8_t ps_mask, bool square_en, uint16_t TN_MIN,
        uint16_t TR_MIN, uint16_t TC_MIN, uint16_t n, uint16_t r, uint16_t c, uint8_t lmode, bool r_init_en, bool c_init_en, uint16_t TCol_lps_n[1],
        uint8_t ifm_sqQ_cb){

//to find the top-left real px_loc
    static uint16_t px_l, px_t, px_r, px_b;
    static int16_t col_l, col_r, row_t, row_b;
    static uint16_t col_len, row_len;
    static uint8_t cl_status = 0;

    static uint16_t TRow_lps, TCol_lps;

    row_init_cfg( &TRow_lps, IR, ps_sf_bit, ps_mask, c_stride, pad, K, r, TR_MIN, &px_t, &px_b, &row_len, &row_t, &row_b, r_init_en);
    col_init_cfg( &TCol_lps, IC, ps_sf_bit, ps_mask, c_stride, pad, K, c, TC_MIN, &px_l, &px_r, &col_l, &col_r, &col_len, c_init_en);
    TCol_lps_n[0] = TCol_lps;

    bool cl_flag = 0;
    if(c_init_en&&(!n)){
        cl_status = 1;
        cl_flag = 1;
    }else{
        if(cl_status==1){
            cl_status = 0;
            cl_flag = 1;
        }
    }

    if(cl_flag){
        for(int k = 0; k < Tnax_dx; k++){
            // clean_ifm_bank(ifm_buf[k], TRow, TCol, (k < TN_MIN));
            // if(k<TN_MIN){
                for(int j = 0; j < TRow_lps; j++)
                for(int i = 0; i < TCol_lps; i++)
                for(uint32_t x=0; x<LANE_NUM; x++){
                    ifm_buf[k][i + TCol_lps*j][x] = 0;
                }
            // }
        }
    }

    assert((col_len <= IC)&&(col_len > 0));
    assert((row_len <= IR)&&(row_len > 0));

	bool IsColCont = (col_len == IC);
	bool IsRowCont = (row_len == IR);
	bool IsAllCont = IsRowCont && IsColCont;

    uint16_t TN_MINax_dx = (TN_MIN + LANE_NUM -1)/LANE_NUM;
	uint16_t nax_dx = n/LANE_NUM;

	uint32_t burstlen;
	uint32_t offset_base;
	uint16_t T1_MIN, T2_MIN;
	if(IsAllCont){
		burstlen = TN_MINax_dx*row_len*col_len;
		offset_base = nax_dx*IHxIW;
		T1_MIN = 1;
		T2_MIN = 1;
	}else if(IsColCont){
		burstlen = row_len*col_len;
		offset_base = nax_dx*IHxIW + row_t*IC;
		T1_MIN = TN_MINax_dx;
		T2_MIN = 1;
	}else{
		burstlen = col_len;
		offset_base = nax_dx*IHxIW + row_t*IC + col_l;
		T1_MIN = TN_MINax_dx;
		T2_MIN = row_len;
	}

    uint16_t t1, t2, t3;
    t1 = 0; t2 = 0; t3 = 0;
    for( uint16_t t1m = 0;t1m < T1_MIN; t1m++){
    for( uint16_t t2m = 0;t2m < T2_MIN; t2m++){
            uint32_t offset_ex = offset_base;
            t3 = 0;
            if(IsAllCont){
                    t1 = 0; t2 = 0;
            }else if(IsColCont){
                    t1 = t1m; t2 = 0;
                    offset_ex += (t1m*IHxIW);
            }else{
                    t1 = t1m; t2 = t2m;
                    offset_ex += ( t1m*IHxIW + t2m*IC);
            }

            int16_t *ifm_addr = ifm + offset_ex*LANE_NUM;
            for(uint32_t tbl=0; tbl<burstlen; tbl++){
// DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=TnxIB_HxIB_W)
// #pragma HLS PIPELINE II=1
                int16_t i = t3 + px_l;
                int16_t j = t2 + px_t;
                uint32_t ifm_idx = i + TCol_lps*j;

                int16_t tmp_f32i[LANE_NUM];
                memcpy(tmp_f32i, ifm_addr + (tbl*LANE_NUM), sizeof(int16_t)*LANE_NUM);

                // for(uint32_t x=0; x<LANE_NUM; x++){
                //     // float tmp_out;
                //     int16_t tmp_out;
                //     int16_t tmp_in = tmp_f32i[x];
                //     if(square_en){
                //         int32_t tmp_out_i32 = tmp_in*tmp_in;
                //         int16_t tmp_out_sr_i16 = tmp_out_i32 >> ifm_sqQ;
                //         // tmp_out = tmp_out_sr_i16*pow(0.5, 28-ifm_sqQ);
                //         tmp_out = tmp_out_sr_i16;
                //     }
                //     else{
                //         // tmp_out = tmp_in*pow(0.5, 14);
                //         tmp_out = tmp_in;
                //     }

                //     ifm_buf[t1][ifm_idx][x] = tmp_out >> iQ;
                // }

                for(uint32_t x=0; x<LANE_NUM; x++){
                    int16_t tmp_in = tmp_f32i[x];
                    int16_t tmp_out_i16;
                    if(square_en&& (tmp_in<0))
                        tmp_out_i16 = -tmp_in;
                    else
                        tmp_out_i16 = tmp_in;

                    ifm_buf[t1][ifm_idx][x] = tmp_out_i16 >> ifm_sqQ_cb;
                }

                t3++;
                // if(IsColCont)
                if(t3==col_len){
                    t3 = 0;
                    t2++;
                    // if(IsAllCont)
                    if(t2==row_len){
                            t2 = 0;
                            t1++;
                }}
            }

    }}
}

void load_ifm_k_wrapper(int16_t ifm_buf[Tnax_dx][MAX_IB_HW][LANE_NUM], int16_t k_buf[Tmax_dx][MAX_Tif][MAX_KxK][LANE_NUM], int16_t *ifm, int16_t *kernel,
        uint8_t m, uint8_t n, uint16_t r, uint16_t c, uint8_t TM_MIN, uint16_t TR_MIN, uint16_t TC_MIN, uint8_t K, uint8_t KxK, uint16_t TN, uint16_t IF, uint16_t IR, uint16_t IC,
        uint16_t OR, uint16_t OC, uint32_t IHxIW, uint8_t p_stride, uint8_t c_stride, uint8_t pad,
        uint8_t ps_sf_bit, uint8_t ps_mask, bool sq_en, uint8_t lmode, bool r_init_en, bool c_init_en, uint16_t TCol_lps_n[1], bool enable,
        uint8_t ifm_sqQ_cb, uint8_t wQ){

    if(!enable)
        return;

    uint16_t TN_MIN = MIN(TN, IF-n);

    load_ifm_pad_square( ifm_buf, ifm, IR, IC, K, IHxIW, p_stride, c_stride, pad, ps_sf_bit, ps_mask, sq_en,
                TN_MIN, TR_MIN, TC_MIN, n, r, c, lmode, r_init_en, c_init_en, TCol_lps_n, ifm_sqQ_cb);
    load_kernel_reorg(k_buf, kernel, m, n, TM_MIN, TN_MIN, K, KxK, wQ);
}

// void Compute_wrapper_sub(int16_t ifm_buf[Tnax_dx][MAX_IB_HW][LANE_NUM], int32_t ofm_buf[Tmax_dx][MAX_Trc][LANE_NUM],
//                     int16_t k_buf[Tmax_dx][MAX_Tif][MAX_KxK][LANE_NUM],
//                     uint8_t Ky, uint8_t Kx, uint16_t TR_MIN, uint16_t TC_MIN, uint8_t c_stride, uint16_t TCol, uint32_t ifm_offset, bool init_en,
//                     uint8_t k_offset, uint32_t ofm_offset, uint8_t interQ){

//     assert((ofm_offset >= 0)&&(ofm_offset < MAX_Trc));

//     int32_t partial_sum[MAX_Tof];
//     int32_t partial_add[MAX_Tof][MAX_Tif];

//     for(uint8_t ky=0; ky<Ky; ky++)
//     for(uint8_t kx=0; kx<Kx; kx++)
//     for(uint16_t tr=0; tr<TR_MIN; tr++)
//     for(uint16_t tc=0; tc<TC_MIN; tc++){

//         uint32_t ifm_idx = (c_stride*tr+ky)*TCol + (c_stride*tc+kx) + ifm_offset;
//         uint8_t k_idx = ky*Kx + kx + k_offset;
//         uint32_t ofm_idx = tr*TC_MIN + tc + ofm_offset;

//         for(int tm=0; tm< MAX_Tof; tm++){
//             if(ky==0&&kx==0&&init_en)
//                 partial_sum[tm] = 0;
//             else{
//                 partial_sum[tm] = ofm_buf[tm/LANE_NUM][ofm_idx][tm%LANE_NUM];
//             }

//             for(int tn = 0;tn < MAX_Tif; tn++)
//             {
//                 assert(ifm_idx < MAX_IB_HW);
//                 int16_t w_16i = k_buf[tm/LANE_NUM][tn][k_idx][tm%LANE_NUM];
//                 int16_t ifm_16i = ifm_buf[tn/LANE_NUM][ifm_idx][tn%LANE_NUM];
//                 int32_t tmp_mul_i32 = w_16i*ifm_16i;

//                 int32_t	tmp_sl_i32 = tmp_mul_i32 >> interQ;
//                 partial_add[tm][tn] = tmp_sl_i32;
//             }

//             for(int tn = 0;tn < MAX_Tif; tn++)
//             {
//                 partial_sum[tm] += partial_add[tm][tn];
//             }

//             ofm_buf[tm/LANE_NUM][ofm_idx][tm%LANE_NUM] = partial_sum[tm];
//         }
//     }
// }

// void Compute_wrapper_sub(int16_t ifm_buf[Tnax_dx][MAX_IB_HW][LANE_NUM], int32_t ofm_buf[Tmax_dx][MAX_Trc][LANE_NUM],
//                     int16_t k_buf[Tmax_dx][MAX_Tif][MAX_KxK][LANE_NUM],
//                     uint8_t Ky, uint8_t Kx, uint16_t TR_MIN, uint16_t TC_MIN, uint8_t c_stride, uint16_t TCol, uint32_t ifm_offset, bool init_en,
//                     uint8_t k_offset, uint32_t ofm_offset, uint8_t interQ){

//     int32_t partial_mul[MAX_Tof][MAX_Tif];
// #pragma HLS ARRAY_PARTITION variable=partial_mul complete dim=1
// #pragma HLS ARRAY_PARTITION variable=partial_mul complete dim=2

//     for(uint8_t ky=0; ky<Ky; ky++){
// // DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=MAX_K)
//     for(uint8_t kx=0; kx<Kx; kx++){
// // DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=MAX_K)
//     for(uint16_t tr=0; tr<TR_MIN; tr++){
// // DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=MAX_Trc)
//     for(uint16_t tc=0; tc<TC_MIN; tc++){
// // DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=MAX_Trc)
// #pragma HLS PIPELINE II=1

//         uint32_t ifm_idx = (c_stride*tr+ky)*TCol + (c_stride*tc+kx) + ifm_offset;
//         uint8_t k_idx = ky*Kx + kx + k_offset;
//         uint32_t ofm_idx = tr*TC_MIN + tc + ofm_offset;

//         for(int tm=0; tm< MAX_Tof; tm++){
// //#pragma HLS DEPENDENCE variable=ofm_buf inter false
//         	int32_t partial_add;
//             if(ky==0&&kx==0&&init_en)
//             	partial_add = 0;
//             else{
//             	partial_add = ofm_buf[tm/LANE_NUM][ofm_idx][tm%LANE_NUM];
//             }

//             for(int tn = 0;tn < MAX_Tif; tn++)
//             {
// //                assert(ifm_idx < MAX_IB_HW);
//                 int16_t w_16i = k_buf[tm/LANE_NUM][tn][k_idx][tm%LANE_NUM];
//                 int16_t ifm_16i = ifm_buf[tn/LANE_NUM][ifm_idx][tn%LANE_NUM];
//                 int32_t tmp_mul_i32 = w_16i*ifm_16i;

//                 int32_t	tmp_sl_i32 = tmp_mul_i32 >> interQ;
//                 partial_mul[tm][tn] = tmp_sl_i32;
//             }

//             int32_t partial_sum = partial_add;
//             for(int tn = 0;tn < MAX_Tif; tn++)
//             {
//                 partial_sum = partial_sum + partial_mul[tm][tn];
//             }

//             ofm_buf[tm/LANE_NUM][ofm_idx][tm%LANE_NUM] = partial_sum;
//         }
//     }}}}
// }

void Compute_wrapper_sub(int16_t ifm_buf[Tnax_dx][MAX_IB_HW][LANE_NUM], int32_t ofm_buf[Tmax_dx][MAX_Trc][LANE_NUM],
                    int16_t k_buf[Tmax_dx][MAX_Tif][MAX_KxK][LANE_NUM],
                    uint8_t Ky, uint8_t Kx, uint16_t TR_MIN, uint16_t TC_MIN, uint8_t c_stride, uint16_t TCol, uint32_t ifm_offset, bool init_en,
                    uint8_t k_offset, uint32_t ofm_offset){

    const uint64_t MAX_KyxTRC_MIN = MAX_KxK*MAX_Trc;

    int32_t partial_add[MAX_Tof];
#pragma HLS ARRAY_PARTITION variable=partial_add complete dim=1
    int32_t partial_mul[MAX_Tof][MAX_Tif];
#pragma HLS ARRAY_PARTITION variable=partial_mul complete dim=1
#pragma HLS ARRAY_PARTITION variable=partial_mul complete dim=2

    // uint8_t iQ = interQ/2;
    // uint8_t wQ = interQ/2 + (interQ & 0x1);

//#pragma HLS DEPENDENCE variable=ofm_buf inter false
    uint8_t ky, kx;
    uint16_t tr,tc;
    ky = 0; kx = 0; tr = 0; tc = 0;

    uint64_t Ncomb = Ky*Kx*TR_MIN*TC_MIN;
    assert(Ncomb <= MAX_KyxTRC_MIN);
    for(uint64_t nc=0; nc < Ncomb; nc++){

#pragma HLS PIPELINE II=1
        uint32_t ifm_idx = (c_stride*tr+ky)*TCol + (c_stride*tc+kx) + ifm_offset;
        uint8_t k_idx = ky*Kx + kx + k_offset;
        uint32_t ofm_idx = tr*TC_MIN + tc + ofm_offset;

        for(int tm=0; tm< MAX_Tof; tm++){

            if(ky==0&&kx==0&&init_en)
            	partial_add[tm] = 0;
            else{
            	partial_add[tm] = ofm_buf[tm/LANE_NUM][ofm_idx][tm%LANE_NUM];
            }

            for(int tn = 0;tn < MAX_Tif; tn++)
            {
                // int16_t w_16i = k_buf[tm/LANE_NUM][tn][k_idx][tm%LANE_NUM];
                // int16_t ifm_16i = ifm_buf[tn/LANE_NUM][ifm_idx][tn%LANE_NUM];
                // int32_t tmp_mul_i32 = w_16i*ifm_16i;
                // int32_t	tmp_sl_i32 = tmp_mul_i32 >> interQ;

                // int16_t w_16i = k_buf[tm/LANE_NUM][tn][k_idx][tm%LANE_NUM] >> iQ;
                // int16_t ifm_16i = ifm_buf[tn/LANE_NUM][ifm_idx][tn%LANE_NUM] >> wQ;
                int16_t w_16i = k_buf[tm/LANE_NUM][tn][k_idx][tm%LANE_NUM];
                int16_t ifm_16i = ifm_buf[tn/LANE_NUM][ifm_idx][tn%LANE_NUM];
                int32_t tmp_mul_i32 = w_16i*ifm_16i;
                int32_t	tmp_sl_i32 = tmp_mul_i32;

                partial_mul[tm][tn] = tmp_sl_i32;
            }

            int32_t partial_sum = partial_add[tm];
            for(int tn = 0;tn < MAX_Tif; tn++)
            {
                partial_sum = partial_sum + partial_mul[tm][tn];
            }

            ofm_buf[tm/LANE_NUM][ofm_idx][tm%LANE_NUM] = partial_sum;
        }

        tc++;
        if(tc==TC_MIN){
            tc = 0; tr++;
            if(tr==TR_MIN){
                tr = 0; kx++;
                if(kx==Kx){
                    kx = 0; ky++;
                }
            }
        }
    }
}

void conv2d_offset_calc_sub(int K, int TCol_lps, uint8_t mode, uint8_t subK_num,
     uint8_t Ky_s[16], uint8_t Kx_s[16], uint8_t k_offset_s[16], uint32_t ifm_offset_s[16], bool enable){

    if(!enable)
        return;

    const uint8_t R_offset[4] = {0, 0, 1, 1};
    const uint8_t C_offset[4] = {0, 1, 0, 1};
    const uint8_t LUT_sub_Ky[4]  = {3, 3, 2, 2};
    const uint8_t LUT_sub_Kx[4]  = {3, 2, 3, 2};
    const uint8_t k_offset_sub5x5[4] = {0, 9, 15, 21};
    const uint8_t k_offset_sub9x9[16] = {0, 9, 15, 21, 27, 33, 37, 41, 45, 51, 55, 59, 63, 69, 73, 77};

    for(uint8_t z=0; z<subK_num; z++){
        uint8_t k_offset_cur;
        uint8_t ifm_sel;
        uint8_t Ky, Kx;
        if(mode==3){
            // ifm_sel = ((z/4)>0?2:0) + ((z&0x3)>0?1:0);
            int tmp_0, tmp_1;
            if((z/4)> 0)
                tmp_0 = 2;
            else
                tmp_0 = 0;
            if((z&0x3) > 0)
                tmp_1 = 1;
            else
                tmp_1 = 0;
            ifm_sel = tmp_0 + tmp_1;

            Ky = LUT_sub_Ky[ifm_sel];
            Kx = LUT_sub_Kx[ifm_sel];
            k_offset_cur = k_offset_sub9x9[z];
        }else if(mode == 2){
            ifm_sel = z;
            Ky = LUT_sub_Ky[ifm_sel];
            Kx = LUT_sub_Kx[ifm_sel];
            k_offset_cur = k_offset_sub5x5[z];
        }else{
            ifm_sel = 0;
            Ky = K;
            Kx = K;
            k_offset_cur = 0;
        }
        assert(ifm_sel>=0&&ifm_sel<=3);
        uint32_t ifm_offset = (C_offset[ifm_sel] + TCol_lps*R_offset[ifm_sel]);
        Ky_s[z] = Ky;
        Kx_s[z] = Kx;
        k_offset_s[z] = k_offset_cur;
        ifm_offset_s[z] = ifm_offset;
    }
}

void conv2d_Acc_sub(int16_t ifm_buf[Tnax_dx][MAX_IB_HW][LANE_NUM], int32_t ofm_buf[Tmax_dx][MAX_Trc][LANE_NUM],
     int16_t k_buf[Tmax_dx][MAX_Tif][MAX_KxK][LANE_NUM],
     int K, int TCol_lps, uint8_t c_stride, bool init_en, uint32_t *TRxTC_M_offset, uint8_t mode, uint8_t subK_num,
     uint16_t *TR_MIN_sub, uint16_t *TC_MIN_sub, bool enable){//NHWC, kernel KKNM

    if(!enable)
        return ;

    static uint8_t Ky_s[16], Kx_s[16], k_offset_s[16];
    static uint32_t ifm_offset_s[16];

    conv2d_offset_calc_sub( K, TCol_lps, mode, subK_num, Ky_s, Kx_s, k_offset_s, ifm_offset_s, init_en);

    for(uint8_t z=0; z<subK_num; z++){

        Compute_wrapper_sub( ifm_buf, ofm_buf, k_buf, Ky_s[z], Kx_s[z], TR_MIN_sub[z], TC_MIN_sub[z], c_stride, TCol_lps,
                                ifm_offset_s[z], init_en, k_offset_s[z], TRxTC_M_offset[z]);
    }

}

void lc_wrapper(int16_t *ifm, int16_t *kernel, int32_t ofm_buf[Tmax_dx][MAX_Trc][LANE_NUM],
                int IF, int IR, int IC, int OF, int OR, int OC, int pad, int K,
                uint8_t p_stride, uint8_t c_stride, uint8_t ps_sf_bit, uint8_t ps_mask, bool sq_en,
                uint16_t TR, uint16_t TC, uint16_t TM, uint16_t TN, uint8_t KxK, uint32_t IHxIW, uint16_t NTif,
                uint8_t lmode, uint16_t m, uint16_t r, uint16_t c, bool r_init_en, bool c_init_en, uint16_t TX_MIN_n1[3], uint16_t tx_n1[3], bool pp,
                bool in_flag, bool proc_flag, bool lc_enable, uint8_t subK_num, uint8_t c_mode, uint32_t *TRxTC_M_offset_n1,
                uint8_t ifm_sqQ_cb, uint8_t wQ){

    if(!lc_enable)
        return;

    static int16_t ifm_buf0[Tnax_dx][MAX_IB_HW][LANE_NUM];
    static int16_t ifm_buf1[Tnax_dx][MAX_IB_HW][LANE_NUM];
    static int16_t k_buf0[Tmax_dx][MAX_Tif][MAX_KxK][LANE_NUM];
    static int16_t k_buf1[Tmax_dx][MAX_Tif][MAX_KxK][LANE_NUM];

	static uint16_t tx_n[2][3];//tmtrtc, between load2compute
	static uint16_t TX_MIN_n[2][3];
    static uint16_t TCol_lps_n0[1], TCol_lps_n1[1];

    static uint16_t TM_MIN, TR_MIN, TC_MIN;

    static uint16_t TR_MIN_sub[16], TC_MIN_sub[16];
    static uint32_t TRxTC_M_offset[17];

    static uint16_t TR_MIN_sub_n[2][16], TC_MIN_sub_n[2][16];
    static uint32_t TRxTC_M_offset_n[2][16];

    TM_MIN = MIN(TM, OF-m);
    if(r_init_en){
        TR_MIN = MIN(TR, OR-r);
    }
    if(c_init_en){
        TC_MIN = MIN(TC, OC-c);

        assert((subK_num>=1)&&(subK_num<=16));
        TRxTC_M_offset[0] = 0;
        if(c_mode==0){
            TR_MIN_sub[0] = TR_MIN;
            TC_MIN_sub[0] = TC_MIN;
        }else{
            assert((c_mode==2)||(c_mode==3));
            uint16_t TR_MIN_aX = 0x0;
            uint16_t TC_MIN_aX = 0x0;
            uint8_t ba_num = 0x0;
            uint8_t sr_bit = 0x0;
            if(c_mode==2){
                ba_num = 0x1;
                sr_bit = 1;
                TR_MIN_aX = TR_MIN + 1;
                TC_MIN_aX = TC_MIN + 1;
            }else if(c_mode==3){
                ba_num = 0x3;
                sr_bit = 2;
                TR_MIN_aX = TR_MIN + 3;
                TC_MIN_aX = TC_MIN + 3;
            }
            assert((sr_bit==1)||(sr_bit==2));
            assert((ba_num==0x1)||(ba_num==0x3));

            uint32_t tmp_in = 0;
            for(uint8_t z=0; z<subK_num; z++){
                uint8_t x = z & ba_num;
                uint8_t y = z >> sr_bit;
                TR_MIN_sub[z] = (TR_MIN_aX-y) >> sr_bit;
                TC_MIN_sub[z] = (TC_MIN_aX-x) >> sr_bit;
                tmp_in = tmp_in + TR_MIN_sub[z]*TC_MIN_sub[z];
                TRxTC_M_offset[z+1] = tmp_in;
            }
        }
    }

	if(lmode==0){
		if(pp){
            load_ifm_k_wrapper( ifm_buf0, k_buf0, ifm, kernel, m, 0, r, c, TM_MIN, TR_MIN, TC_MIN, K, KxK, TN, IF, IR, IC, OR, OC, IHxIW,
                p_stride, c_stride, pad, ps_sf_bit, ps_mask, sq_en, lmode, r_init_en, c_init_en, TCol_lps_n0, in_flag, ifm_sqQ_cb, wQ);

            // codec_conv2d_chw_tile( ifm_buf1, ofm_buf, k_buf1, K, c_stride, TX_MIN_n[1][1], TX_MIN_n[1][2], 0, TCol_lps_n1[0], proc_flag);
            conv2d_Acc_sub(ifm_buf1, ofm_buf, k_buf1, K, TCol_lps_n1[0], c_stride, 1, TRxTC_M_offset_n[1], c_mode, subK_num,
                                TR_MIN_sub_n[1], TC_MIN_sub_n[1], proc_flag);

			tx_n[0][0] = m; tx_n[0][1] = r; tx_n[0][2] = c;
			TX_MIN_n[0][0] = TM_MIN; TX_MIN_n[0][1] = TR_MIN; TX_MIN_n[0][2] = TC_MIN;

            for(uint8_t z=0; z<subK_num; z++){
                TR_MIN_sub_n[0][z] = TR_MIN_sub[z];
                TC_MIN_sub_n[0][z] = TC_MIN_sub[z];
            }

			tx_n1[0] = tx_n[1][0]; tx_n1[1] = tx_n[1][1]; tx_n1[2] = tx_n[1][2];
			TX_MIN_n1[0] = TX_MIN_n[1][0]; TX_MIN_n1[1] = TX_MIN_n[1][1]; TX_MIN_n1[2] = TX_MIN_n[1][2];
            for(uint8_t z=0; z<subK_num; z++){
                TRxTC_M_offset_n[0][z] = TRxTC_M_offset[z];
                TRxTC_M_offset_n1[z] = TRxTC_M_offset_n[1][z];
            }
		}else{
            load_ifm_k_wrapper( ifm_buf1, k_buf1, ifm, kernel, m, 0, r, c, TM_MIN, TR_MIN, TC_MIN, K, KxK, TN, IF, IR, IC, OR, OC, IHxIW,
                p_stride, c_stride, pad, ps_sf_bit, ps_mask, sq_en, lmode, r_init_en, c_init_en, TCol_lps_n1, in_flag, ifm_sqQ_cb, wQ);

            // codec_conv2d_chw_tile( ifm_buf0, ofm_buf, k_buf0, K, c_stride, TX_MIN_n[0][1], TX_MIN_n[0][2], 0, TCol_lps_n0[0], proc_flag);
            conv2d_Acc_sub(ifm_buf0, ofm_buf, k_buf0, K, TCol_lps_n0[0], c_stride, 1, TRxTC_M_offset_n[0], c_mode, subK_num,
                                TR_MIN_sub_n[0], TC_MIN_sub_n[0], proc_flag);

			tx_n[1][0] = m; tx_n[1][1] = r; tx_n[1][2] = c;
			TX_MIN_n[1][0] = TM_MIN; TX_MIN_n[1][1] = TR_MIN; TX_MIN_n[1][2] = TC_MIN;

            for(uint8_t z=0; z<subK_num; z++){
                TR_MIN_sub_n[1][z] = TR_MIN_sub[z];
                TC_MIN_sub_n[1][z] = TC_MIN_sub[z];
            }

			tx_n1[0] = tx_n[0][0]; tx_n1[1] = tx_n[0][1]; tx_n1[2] = tx_n[0][2];
			TX_MIN_n1[0] = TX_MIN_n[0][0]; TX_MIN_n1[1] = TX_MIN_n[0][1]; TX_MIN_n1[2] = TX_MIN_n[0][2];
            for(uint8_t z=0; z<subK_num; z++){
                TRxTC_M_offset_n[1][z] = TRxTC_M_offset[z];
                TRxTC_M_offset_n1[z] = TRxTC_M_offset_n[0][z];
            }
		}
	}else{
            bool pp_tn = 1;
            uint16_t tn_n[2];
            uint16_t tn_r = 0;
            for(uint16_t tn = 0; tn < NTif+1; tn++)
            {
                bool in_flag = tn < NTif;
                bool proc_flag = tn > 0;
                // uint16_t tn_r = tn*TN;
                if(pp_tn){
                    load_ifm_k_wrapper( ifm_buf0, k_buf0, ifm, kernel, m, tn_r, r, c, TM_MIN, TR_MIN, TC_MIN, K, KxK, TN, IF, IR, IC, OR, OC, IHxIW,
                        p_stride, c_stride, pad, ps_sf_bit, ps_mask, sq_en, lmode, r_init_en, c_init_en, TCol_lps_n0, in_flag, ifm_sqQ_cb, wQ);

                    // codec_conv2d_chw_tile( ifm_buf1, ofm_buf, k_buf1, K, c_stride, TX_MIN_n[1][1], TX_MIN_n[1][2], tn_n[1], TCol_lps_n1[0], proc_flag);
                    conv2d_Acc_sub(ifm_buf1, ofm_buf, k_buf1, K, TCol_lps_n1[0], c_stride, (tn_n[1]==0), TRxTC_M_offset_n[1], c_mode,
                                    subK_num, TR_MIN_sub_n[1], TC_MIN_sub_n[1], proc_flag);

                    tn_n[0] = tn_r;
                    tx_n[0][0] = m; tx_n[0][1] = r; tx_n[0][2] = c;
                    TX_MIN_n[0][0] = TM_MIN; TX_MIN_n[0][1] = TR_MIN; TX_MIN_n[0][2] = TC_MIN;

                    for(uint8_t z=0; z<subK_num; z++){
                        TR_MIN_sub_n[0][z] = TR_MIN_sub[z];
                        TC_MIN_sub_n[0][z] = TC_MIN_sub[z];
                    }

                    for(uint8_t z=0; z<subK_num; z++){
                        TRxTC_M_offset_n[0][z] = TRxTC_M_offset[z];
                    }

                    tn_r += TN;

                    pp_tn = 0;
                }else{
                    load_ifm_k_wrapper( ifm_buf1, k_buf1, ifm, kernel, m, tn_r, r, c, TM_MIN, TR_MIN, TC_MIN, K, KxK, TN, IF, IR, IC, OR, OC, IHxIW,
                        p_stride, c_stride, pad, ps_sf_bit, ps_mask, sq_en, lmode, r_init_en, c_init_en, TCol_lps_n1, in_flag, ifm_sqQ_cb, wQ);

                    // codec_conv2d_chw_tile( ifm_buf0, ofm_buf, k_buf0, K, c_stride, TX_MIN_n[0][1], TX_MIN_n[0][2], tn_n[0], TCol_lps_n0[0], proc_flag);
                    conv2d_Acc_sub(ifm_buf0, ofm_buf, k_buf0, K, TCol_lps_n0[0], c_stride, (tn_n[0]==0), TRxTC_M_offset_n[0], c_mode,
                                    subK_num, TR_MIN_sub_n[0], TC_MIN_sub_n[0], proc_flag);

                    tn_n[1] = tn_r;
                    tx_n[1][0] = m; tx_n[1][1] = r; tx_n[1][2] = c;
                    TX_MIN_n[1][0] = TM_MIN; TX_MIN_n[1][1] = TR_MIN; TX_MIN_n[1][2] = TC_MIN;

                    for(uint8_t z=0; z<subK_num; z++){
                        TR_MIN_sub_n[1][z] = TR_MIN_sub[z];
                        TC_MIN_sub_n[1][z] = TC_MIN_sub[z];
                    }

                    for(uint8_t z=0; z<subK_num; z++){
                        TRxTC_M_offset_n[1][z] = TRxTC_M_offset[z];
                    }


                    tn_r += TN;

                    pp_tn = 1;
                }
            }

            tx_n1[0] = m; tx_n1[1] = r; tx_n1[2] = c;
			TX_MIN_n1[0] = TM_MIN; TX_MIN_n1[1] = TR_MIN; TX_MIN_n1[2] = TC_MIN;
            for(uint8_t z=0; z<subK_num; z++){
                TRxTC_M_offset_n1[z] = TRxTC_M_offset[z];
            }
	}
}

// void aligned_bias_scale_mmcpy(float dst_buf[((MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM)][LANE_NUM], float *src, int data_num, bool enable){

//     if(!enable)
//         return ;

//     for(int x=0; x<data_num; x++)
//     for(int y=0; y<LANE_NUM; y++){
//         dst_buf[x][y] = src[x*LANE_NUM+y];
//     }
// }

void aligned_bias_scale_mmcpy(float dst_buf[((MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM)][LANE_NUM], float *src, int data_num, bool enable){

    if(!enable)
        return ;

    // for(int x=0; x<data_num; x++)
    // for(int z=0; z<2; z++)
    // for(int y=0; y<LANE_NUM/2; y++)
    // {
    //     dst_buf[x][z*(LANE_NUM/2) + y] = src[x*LANE_NUM + z*(LANE_NUM/2) + y];
    // }

    // for(int x=0; x<data_num; x++)
    // for(int z=0; z<2; z++){
    //     float tmp_128b[LANE_NUM/2];

    //     memcpy(tmp_128b, src + (x*2 + z)*(LANE_NUM/2), (LANE_NUM/2)*sizeof(float));

    //     for(int y=0; y<LANE_NUM/2; y++)
    //     {
    //         dst_buf[x][z*(LANE_NUM/2) + y] = tmp_128b[y];
    //     }
    // }


    for(int x=0; x<data_num; x++){
    for(int z=0; z<2; z++){
    	float tmp_128b[LANE_NUM/2];
       memcpy(tmp_128b, src + (x*2 + z)*(LANE_NUM/2), (LANE_NUM/2)*sizeof(float));
        for(int y=0; y<LANE_NUM/2; y++)
        {
        	// uint32_t tmp_u32 = tmp_128b.range(y*32+31, y*32);
            dst_buf[x][z*(LANE_NUM/2) + y] = tmp_128b[y];
        }
    }}

}

void IC_codec(int16_t *ofm, int16_t *ifm, int16_t *kernel, float *bias,
                float *ifm_scale, float *ofm_scale, float *kernel_scale,
                int IF, int IR, int IC, int OF, int OR, int OC, int pad, int K, int S, int ltype,
                uint8_t p_stride, uint8_t c_stride, uint8_t wb_mode, uint8_t ps_sf_bit, uint8_t ps_mask, bool sq_en,
                uint16_t TR, uint16_t TC, uint16_t TM, uint16_t TN, uint32_t OHW, uint8_t KxK,
                uint32_t IHxIW, uint16_t p_stridexIR, uint16_t p_stridexIC, bool use_bias,
                uint16_t NToy, uint16_t NTox, uint16_t NTof, uint64_t NTcomb, uint16_t NTif, uint8_t lmode, uint64_t NTcomb_l,
                uint8_t ifm_sqQ, uint8_t interQ){

    int32_t ofm_buf0[Tmax_dx][MAX_Trc][LANE_NUM];
    int32_t ofm_buf1[Tmax_dx][MAX_Trc][LANE_NUM];
    float b_buf[((MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM)][LANE_NUM];
    float ofm_scale_l[(MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM][LANE_NUM];
    float kernel_scale_l[(MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM][LANE_NUM];
    float ifm_scale_l[(MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM][LANE_NUM];

    uint8_t c_mode;
    uint8_t subK_num;
    if(K==5&&c_stride==1){//K==5&cs==1 CONVT:2
        c_mode = 2;
        subK_num = 4;
    }else if(K==9&&c_stride==1){//K==9&cs==1 CONVT:3
        c_mode = 3;
        subK_num = 16;
    }else{//normal:0
        c_mode = 0;
        subK_num = 1;
    }

    uint16_t ofm_num_dx = ((OF+LANE_NUM-1)/LANE_NUM);
    uint16_t ifm_num_dx = ((IF+LANE_NUM-1)/LANE_NUM);

    aligned_bias_scale_mmcpy(b_buf, bias, ofm_num_dx, use_bias);
    aligned_bias_scale_mmcpy(ifm_scale_l, ifm_scale, ifm_num_dx, wb_mode!=0);
    aligned_bias_scale_mmcpy(ofm_scale_l, ofm_scale, ofm_num_dx, 1);
    aligned_bias_scale_mmcpy(kernel_scale_l, kernel_scale, ofm_num_dx, 1);

    uint8_t iQ = interQ/2;
    uint8_t wQ = interQ/2 + (interQ & 0x1);
    uint8_t ifm_sqQ_cb = ifm_sqQ + iQ;

    // if(use_bias){
    //     // for(uint16_t x=0; x<ofm_num_dx; x++){
    //     //     memcpy(b_buf[x], bias + x*LANE_NUM, LANE_NUM*sizeof(float));
    //     // }
    //     for(uint16_t x=0; x<ofm_num_dx; x++)
    //     for(int y=0; y<LANE_NUM; y++){
    //         b_buf[x][y] = bias[x*LANE_NUM+y];
    //     }
    // }

    // if(wb_mode!=0){
    //     for(uint16_t x=0; x<ifm_num_dx; x++)
    //     for(int y=0; y<LANE_NUM; y++){
    //         ifm_scale_l[x][y] = ifm_scale[x*LANE_NUM+y];
    //     }
    // }

    // for(uint16_t x=0; x<ofm_num_dx; x++)
    // for(int y=0; y<LANE_NUM; y++){
    //     ofm_scale_l[x][y] = ofm_scale[x*LANE_NUM+y];
    //     kernel_scale_l[x][y] = kernel_scale[x*LANE_NUM+y];
    // }

    uint16_t TX_MIN_n10[3], TX_MIN_n11[3];//TM_MIN, TR_MIN, TC_MIN
    uint16_t tx_n10[3], tx_n11[3];//m, r, c
    uint32_t TRxTC_M_offset_n10[16], TRxTC_M_offset_n11[16];

	uint16_t tr, tc, tm;
	uint16_t TR_MIN, TC_MIN, TM_MIN;
	uint16_t tm_r, tr_r, tc_r;
	bool in_flag, proc_flag, out_flag, lc_enable;

	tr = 0; tc = 0; tm = 0;
	tm_r = 0; tr_r = 0; tc_r = 0;
	bool pp = 1;
	bool r_init_en = 1, c_init_en = 1;
	for(uint64_t ntc = 0; ntc < NTcomb_l; ntc++){

		if(lmode==0){
			in_flag = ntc < NTcomb;
			proc_flag = (ntc > 0)&& (ntc < NTcomb + 1);
			out_flag = ntc > 1;
			// lc_enable = 1;
            lc_enable = (ntc < NTcomb + 1);
		}else{
			in_flag = 1;
			proc_flag = 1;
			out_flag = ntc > 0;
			lc_enable = ntc < NTcomb;
		}

		if(pp){
                lc_wrapper(ifm, kernel, ofm_buf0, IF, IR, IC, OF, OR, OC, pad, K, p_stride, c_stride, ps_sf_bit, ps_mask, sq_en,
                            TR, TC, TM, TN, KxK, IHxIW, NTif, lmode, tm_r, tr_r, tc_r, r_init_en, c_init_en,
                            TX_MIN_n10, tx_n10, pp, in_flag, proc_flag, lc_enable, subK_num, c_mode, TRxTC_M_offset_n10,
                            ifm_sqQ_cb, wQ);

                codec_ofm_store_chw_tile(ofm, ofm_buf1, ifm, b_buf,
                    tx_n11[0], tx_n11[1], tx_n11[2], TX_MIN_n11[0], TX_MIN_n11[1], TX_MIN_n11[2],
                    OHW, OR, OC, wb_mode, use_bias, out_flag, subK_num, TRxTC_M_offset_n11, ifm_scale_l, ofm_scale_l, kernel_scale_l);

			pp = 0;
		}else{
                lc_wrapper(ifm, kernel, ofm_buf1, IF, IR, IC, OF, OR, OC, pad, K, p_stride, c_stride, ps_sf_bit, ps_mask, sq_en,
                            TR, TC, TM, TN, KxK, IHxIW, NTif, lmode, tm_r, tr_r, tc_r, r_init_en, c_init_en,
                            TX_MIN_n11, tx_n11, pp, in_flag, proc_flag, lc_enable, subK_num, c_mode, TRxTC_M_offset_n11,
                            ifm_sqQ_cb, wQ);

                codec_ofm_store_chw_tile(ofm, ofm_buf0, ifm, b_buf,
                    tx_n10[0], tx_n10[1], tx_n10[2], TX_MIN_n10[0], TX_MIN_n10[1], TX_MIN_n10[2],
                    OHW, OR, OC, wb_mode, use_bias, out_flag, subK_num, TRxTC_M_offset_n10, ifm_scale_l, ofm_scale_l, kernel_scale_l);

			pp = 1;
		}

		tm++;
		tm_r += TM;
		r_init_en = 0; c_init_en = 0;
		if(tm==NTof)
		{
			tm=0; tc++;
			tm_r = 0; tc_r += TC; c_init_en = 1;//update col
			if(tc==NTox)
			{
				tc=0; tr++;
				tc_r = 0; tr_r += TR; r_init_en = 1;//update row
		}}
	}
}
