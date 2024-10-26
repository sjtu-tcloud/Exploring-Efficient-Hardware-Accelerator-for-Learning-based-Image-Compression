
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

#define MAX_Pif (4)
#define MAX_Pof (2)
#define MAX_Poy (4)
#define MAX_Pox (4)
#endif

const uint32_t MAX_KxK = MAX_K*MAX_K;
const uint32_t MAX_Trc = 512*(MAX_Poy*MAX_Pox);
const uint32_t MAX_IB_HW = 512;

const uint32_t Tmax_dx = (MAX_Tof+LANE_NUM-1)/LANE_NUM;
const uint32_t Tnax_dx = (MAX_Tif+LANE_NUM-1)/LANE_NUM;

#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))

float post_process(float tmp_in, float bias, float ifm_ex, uint8_t mode, bool bias_en){

    float tmp_in0, tmp1, tmp_out;
    if(bias_en){
        tmp_in0 = tmp_in + bias;
    }else
        tmp_in0 = tmp_in;

    tmp_out = 0;
    if(mode==0){
        tmp_out = tmp_in0;
    }else{
        if(mode==1){
            tmp_out = ifm_ex*tmp_in0;
        }else{
            tmp_out = ifm_ex/tmp_in0;
            assert(mode==2);
        }
    }

    return tmp_out;
}

void codec_ofm_store_chw_tile(float *ofm, float ofm_buf_Poy[MAX_Poy][MAX_Pox][Tmax_dx*LANE_NUM][MAX_Trc/(MAX_Poy*MAX_Pox)], float *ifm_extra,
    float b_buf[((MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM)][LANE_NUM],
    uint16_t m, uint16_t r, uint16_t c, uint16_t TM_MIN, uint16_t TR_MIN, uint16_t TC_MIN, uint32_t OHW,
    uint16_t OH, uint16_t OW, uint8_t mode, bool bias_en, bool enable, uint8_t subK_num, uint16_t TR_Range, uint16_t TC_Range){
//mode=0 normal, 1=src0*sqrt(src1), 2=src0/sqrt(src1)

    if(!enable)
        return;

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

		float *OFM_base = ofm + ofm_offset*LANE_NUM;
        float *IFM_ex_base = ifm_extra + ofm_offset*LANE_NUM;
		for(int tbl_min=0; tbl_min < burstlen; tbl_min++){

            uint16_t poy = tr/TR_Range;
            uint16_t pox = tc/TC_Range;
            uint32_t ofm_idx = (tr%TR_Range)*TC_Range + (tc%TC_Range);

            float tmp_ifm_ex[LANE_NUM];
            if(mode!=0){
                memcpy(tmp_ifm_ex, IFM_ex_base + (tbl_min*LANE_NUM), sizeof(float)*LANE_NUM);
            }else{
                for(uint32_t x=0; x<LANE_NUM; x++){
                    tmp_ifm_ex[x] = 0;
                }
            }

			float tmp_f32i[LANE_NUM];
			for(uint32_t x=0; x<LANE_NUM; x++){
                tmp_f32i[x] = post_process(ofm_buf_Poy[poy][pox][tm*LANE_NUM+x][ofm_idx], b_buf[m/LANE_NUM + tm][x], tmp_ifm_ex[x], mode, bias_en);
			}

			memcpy(OFM_base + (tbl_min*LANE_NUM), tmp_f32i, sizeof(float)*LANE_NUM);

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

void load_kernel_reorg(float k_buf[Tmax_dx*LANE_NUM][MAX_Tif][MAX_KxK], float *kernel, uint8_t m, uint8_t n, uint8_t TM_MIN, uint8_t TN_MIN, uint8_t K, uint8_t KxK){

	static int Woffset;
	if(m==0&&n==0)
		Woffset = 0;

	float *weight_addr = kernel + Woffset*LANE_NUM;
	// uint32_t burstlen = TM_MIN*TN_MIN*KxK;
    uint32_t burstlen = ((TM_MIN+LANE_NUM-1)/LANE_NUM)*TN_MIN*KxK;
	Woffset += burstlen;

	uint8_t t1,t2,t3;
	for(t3 = 0;t3 <KxK; t3++){
// #pragma HLS PIPELINE II=1
	for(t1 = 0;t1 < Tmax_dx*LANE_NUM; t1++){
	for(t2 = 0;t2 < MAX_Tif; t2++){
            k_buf[t1][t2][t3] = 0;
	}}}

	t1 = 0; t2 = 0; t3 = 0;
	for(uint16_t t=0; t<burstlen; t++){
// DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=TmxTnxKxK)
// #pragma HLS PIPELINE II=1
		float tmp_f32i[LANE_NUM];
		memcpy(tmp_f32i, weight_addr + (t*LANE_NUM), sizeof(float)*LANE_NUM);
		for(uint32_t x=0; x<LANE_NUM; x++){
			k_buf[t1*LANE_NUM+x][t2][t3] = tmp_f32i[x];
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

void load_ifm_pad_square(float ifm_buf_Poy[MAX_Poy][MAX_Pox][Tnax_dx*LANE_NUM][MAX_IB_HW], float *ifm, uint16_t IR, uint16_t IC, uint8_t K, uint32_t IHxIW, uint8_t p_stride,
        uint8_t c_stride, uint8_t pad, uint8_t ps_sf_bit, uint8_t ps_mask, bool square_en, uint16_t TN_MIN,
        uint16_t TR_MIN, uint16_t TC_MIN, uint16_t n, uint16_t r, uint16_t c, uint8_t lmode, bool r_init_en, bool c_init_en,
        uint16_t TRow_lps_n[1], uint16_t TCol_lps_n[1]){

//to find the top-left real px_loc
    static uint16_t px_l, px_t, px_r, px_b;
    static int16_t col_l, col_r, row_t, row_b;
    static uint16_t col_len, row_len;
    static uint8_t cl_status = 0;

    static uint16_t TRow_lps, TCol_lps;

    row_init_cfg( &TRow_lps, IR, ps_sf_bit, ps_mask, c_stride, pad, K, r, TR_MIN, &px_t, &px_b, &row_len, &row_t, &row_b, r_init_en);
    col_init_cfg( &TCol_lps, IC, ps_sf_bit, ps_mask, c_stride, pad, K, c, TC_MIN, &px_l, &px_r, &col_l, &col_r, &col_len, c_init_en);
    TCol_lps_n[0] = TCol_lps;
    TRow_lps_n[0] = TRow_lps;

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
        for(int k = 0; k < Tnax_dx*LANE_NUM; k++){
                for(int j = 0; j < TRow_lps; j++)
                for(int i = 0; i < TCol_lps; i++)
                for(int x = 0; x< MAX_Poy; x++)
                for(int z = 0; z< MAX_Pox; z++)
                    ifm_buf_Poy[x][z][k][i + TCol_lps*j] = 0;
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

            float *ifm_addr = ifm + offset_ex*LANE_NUM;
            for(uint32_t tbl=0; tbl<burstlen; tbl++){
// DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=TnxIB_HxIB_W)
// #pragma HLS PIPELINE II=1
                int16_t i = t3 + px_l;
                int16_t j = t2 + px_t;
                uint32_t ifm_idx = i + TCol_lps*j;

                float tmp_f32i[LANE_NUM];
                memcpy(tmp_f32i, ifm_addr + (tbl*LANE_NUM), sizeof(float)*LANE_NUM);
                for(uint32_t x=0; x<LANE_NUM; x++){
                    float tmp_in, tmp_out;
                    tmp_in = tmp_f32i[x];

                    if(square_en&&(tmp_in<0)){
                        tmp_out = -tmp_in;
                    }
                    else
                        tmp_out = tmp_in;

                    for(int y = 0; y< MAX_Poy; y++)
                    for(int z = 0; z< MAX_Pox; z++)
                        ifm_buf_Poy[y][z][t1*LANE_NUM + x][ifm_idx] = tmp_out;
                }

                t3++;
                if(t3==col_len){
                    t3 = 0;
                    t2++;
                    if(t2==row_len){
                            t2 = 0;
                            t1++;
                }}
            }

    }}
}

void load_ifm_k_wrapper(float ifm_buf_Poy[MAX_Poy][MAX_Pox][Tnax_dx*LANE_NUM][MAX_IB_HW], float k_buf[Tmax_dx*LANE_NUM][MAX_Tif][MAX_KxK], float *ifm, float *kernel,
        uint8_t m, uint8_t n, uint16_t r, uint16_t c, uint8_t TM_MIN, uint16_t TR_MIN, uint16_t TC_MIN, uint8_t K, uint8_t KxK, uint16_t TN, uint16_t IF, uint16_t IR, uint16_t IC,
        uint16_t OR, uint16_t OC, uint32_t IHxIW, uint8_t p_stride, uint8_t c_stride, uint8_t pad,
        uint8_t ps_sf_bit, uint8_t ps_mask, bool sq_en, uint8_t lmode, bool r_init_en, bool c_init_en,
        uint16_t TRow_lps_n[1], uint16_t TCol_lps_n[1], uint16_t *TN_MIN_n, bool enable){

    if(!enable)
        return;

    uint16_t TN_MIN = MIN(TN, IF-n);

    load_ifm_pad_square( ifm_buf_Poy, ifm, IR, IC, K, IHxIW, p_stride, c_stride, pad, ps_sf_bit, ps_mask, sq_en,
                TN_MIN, TR_MIN, TC_MIN, n, r, c, lmode, r_init_en, c_init_en, TRow_lps_n, TCol_lps_n);
    load_kernel_reorg(k_buf, kernel, m, n, TM_MIN, TN_MIN, K, KxK);

    *TN_MIN_n = TN_MIN;
}

template<typename T, int N>
T adder_tree(T *data){
#pragma HLS INLINE
    // Perform a non-recursive parallel reduction
    for (int stride = 1; stride < N; stride *= 2){
        for (int i = 0; i < N; i += 2 * stride){
            T sum = data[i] + ((i + stride < N) ? data[i + stride] : 0);
            data[i] = sum;
        }
    }
    return data[0];
}

void conv2d_offset_calc_sub(int K, int TCol_lps, uint8_t mode, uint8_t subK_num, uint8_t c_stride, uint16_t *TR_MIN_sub, uint16_t *TC_MIN_sub,
        uint16_t TR_MIN_d_Poy, uint16_t TC_MIN_d_Poy, uint8_t Ky_s[16], uint8_t Kx_s[16], uint8_t k_offset_s[16], uint32_t ifm_offset_s[16],
        uint64_t KyxTRC_MIN_s[16], uint64_t *Tcb_sk_num, uint16_t *cs_x_TColps, uint32_t ifm_idx_poy_ofs[MAX_Poy][MAX_Pox], bool enable){

    if(!enable)
        return;

    const uint8_t R_offset[4] = {0, 0, 1, 1};
    const uint8_t C_offset[4] = {0, 1, 0, 1};
    const uint8_t LUT_sub_Ky[4]  = {3, 3, 2, 2};
    const uint8_t LUT_sub_Kx[4]  = {3, 2, 3, 2};
    const uint8_t k_offset_sub5x5[4] = {0, 9, 15, 21};
    const uint8_t k_offset_sub9x9[16] = {0, 9, 15, 21, 27, 33, 37, 41, 45, 51, 55, 59, 63, 69, 73, 77};

    uint64_t Tcb_sk_num_tmp = 0;

    for(uint8_t z=0; z<subK_num; z++){
        uint8_t k_offset_cur;
        uint8_t ifm_sel;
        uint8_t Ky, Kx;
        if(mode==3){
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

        uint16_t TC_MIN = TC_MIN_sub[z];
        KyxTRC_MIN_s[z] = Ky*Kx*TR_MIN_d_Poy*TC_MIN_d_Poy;
        Tcb_sk_num_tmp = Tcb_sk_num_tmp + KyxTRC_MIN_s[z];
    }

    uint16_t cs_x_TColps_t = c_stride*TCol_lps;
    uint32_t cs_x_TColps_x_TRMdPoy = cs_x_TColps_t*TR_MIN_d_Poy;
    for(uint16_t poy = 0; poy < MAX_Poy; poy++)
    for(uint16_t pox = 0; pox < MAX_Pox; pox++){
        ifm_idx_poy_ofs[poy][pox] = poy*cs_x_TColps_x_TRMdPoy + pox*c_stride*TC_MIN_d_Poy;
    }

    *cs_x_TColps = cs_x_TColps_t;
    *Tcb_sk_num = Tcb_sk_num_tmp;
}

void MAX_Tof_PEs_Piof_Poy(float ifm_buf[Tnax_dx*LANE_NUM][MAX_IB_HW], float ofm_buf[Tmax_dx*LANE_NUM][MAX_Trc/(MAX_Poy*MAX_Pox)],
     float k_buf[Tmax_dx*LANE_NUM][MAX_Tif][MAX_KxK], bool init_flag, uint32_t ifm_idx, uint8_t k_idx, uint32_t ofm_idx,
     uint16_t pif, uint16_t pof){

    float partial_sum[MAX_Pof];
    float partial_add[MAX_Pof][MAX_Pif];

    for(int pm=0; pm< MAX_Pof; pm++){
        int tm = pof*MAX_Pof + pm;
        if(init_flag)
            partial_sum[pm] = 0;
        else{
            partial_sum[pm] = ofm_buf[tm][ofm_idx];
        }
        for(int pn = 0;pn < MAX_Pif; pn++)
        {
            int tn = pif*MAX_Pif + pn;
            partial_add[pm][pn] = k_buf[tm][tn][k_idx]*ifm_buf[tn][ifm_idx];
        }
        float psum_total = adder_tree<float, (MAX_Pif)>(partial_add[pm]);
        ofm_buf[tm][ofm_idx] = psum_total + partial_sum[pm];
    }
}

void conv2d_Acc_sub(float ifm_buf_Poy[MAX_Poy][MAX_Pox][Tnax_dx*LANE_NUM][MAX_IB_HW], float ofm_buf_Poy[MAX_Poy][MAX_Pox][Tmax_dx*LANE_NUM][MAX_Trc/(MAX_Poy*MAX_Pox)],
     float k_buf[Tmax_dx*LANE_NUM][MAX_Tif][MAX_KxK],
     int K, uint16_t TRow_lps, uint16_t TCol_lps, uint8_t c_stride, bool init_en, uint8_t mode, uint8_t subK_num,
     uint16_t TM_MIN, uint16_t TN_MIN, uint16_t TR_MIN_org, uint16_t TC_Range,
     uint16_t *TR_MIN_sub, uint16_t *TC_MIN_sub, bool enable, uint16_t TR_MIN_d_Poy, uint16_t TC_MIN_d_Poy, uint8_t sub_bit,
     uint32_t *ofm_offset_base){//NHWC, kernel KKNM

    if(!enable)
        return ;

    assert(MAX_Tif%MAX_Pif==0);
    assert(MAX_Tof%MAX_Pof==0);

    static uint8_t Ky_s[16], Kx_s[16], k_offset_s[16];
    static uint32_t ifm_offset_s[16];
    static uint64_t KyxTRC_MIN_s[16];
    static uint64_t Tcb_sk_num;
    static uint16_t cs_x_TColps;
    static uint32_t ifm_idx_poy_ofs[MAX_Poy][MAX_Pox];

    conv2d_offset_calc_sub( K, TCol_lps, mode, subK_num, c_stride, TR_MIN_sub, TC_MIN_sub, TR_MIN_d_Poy, TC_MIN_d_Poy,
                Ky_s, Kx_s, k_offset_s, ifm_offset_s, KyxTRC_MIN_s, &Tcb_sk_num, &cs_x_TColps, ifm_idx_poy_ofs, init_en);

    const uint32_t MAX_POF = (MAX_Tof/MAX_Pof);
    const uint32_t MAX_PIF = (MAX_Tif/MAX_Pif);

    uint16_t Pof = (TM_MIN/MAX_Pof);
    uint16_t Pif = (TN_MIN/MAX_Pif);
    if(TM_MIN%MAX_Pof)
        Pof = Pof + 1;
    if(TN_MIN%MAX_Pif)
        Pif = Pif + 1;
    assert((Pof>=1)&&(Pof<=MAX_POF));
    assert((Pif>=1)&&(Pif<=MAX_PIF));

    uint16_t pof, pif;
    uint64_t Tcb_sk_num_comb = Tcb_sk_num*Pof*Pif;
    uint64_t tcb, KyxTRC_MIN;
    uint8_t Kx, k_offset, z, ky, kx;
    uint16_t TC_MIN, tr, tc;
    uint32_t ofm_offset_base_val, ifm_offset_base;

    pof = 0; pif = 0; z = 0; tcb = 0;
    tr = 0; tc = 0; ky = 0; kx = 0;
    for(uint64_t tcb_x=0; tcb_x < Tcb_sk_num_comb; tcb_x++){

        if(tcb==0){
            ky = 0;
            Kx = Kx_s[z];
            k_offset = k_offset_s[z];
            TC_MIN = TC_MIN_sub[z];
            ofm_offset_base_val = ofm_offset_base[z];
            KyxTRC_MIN = KyxTRC_MIN_s[z];
            ifm_offset_base = ifm_offset_s[z];
        }

        bool init_flag = (ky==0)&&(kx==0)&&init_en&&(pif==0);
        uint32_t ifm_offset = ifm_offset_base + c_stride*tc + kx + TCol_lps*ky + tr*cs_x_TColps;
        uint8_t k_idx = ky*Kx + kx + k_offset;
        uint32_t ofm_idx = ((tr*TC_Range + tc) << sub_bit) + ofm_offset_base_val;

        uint32_t ifm_idx[MAX_Poy][MAX_Pox];
        for(uint16_t poy = 0; poy < MAX_Poy; poy++)
        for(uint16_t pox = 0; pox < MAX_Pox; pox++){
            ifm_idx[poy][pox] = ifm_idx_poy_ofs[poy][pox] + ifm_offset;
            assert(ifm_idx[poy][pox] < MAX_IB_HW);
        }

        for(uint16_t poy = 0; poy < MAX_Poy; poy++)
        for(uint16_t pox = 0; pox < MAX_Pox; pox++){
            MAX_Tof_PEs_Piof_Poy(ifm_buf_Poy[poy][pox], ofm_buf_Poy[poy][pox], k_buf, init_flag, ifm_idx[poy][pox], k_idx, ofm_idx, pif, pof);
        }

        tc++;
        if(tc==TC_MIN_d_Poy){
            tc=0; tr++;
            if(tr==TR_MIN_d_Poy){
                tr=0; kx++;
                if(kx==Kx){
                    kx=0; ky++;
                }
            }
        }

        tcb++;
        if(tcb==KyxTRC_MIN){
            tcb=0; z++;
            if(z==subK_num){
                z=0; pif++;
                if(pif==Pif){
                    pif=0; pof++;
                }
            }
        }

    }
}

void lc_wrapper(float *ifm, float *kernel, float ofm_buf_Poy[MAX_Poy][MAX_Pox][Tmax_dx*LANE_NUM][MAX_Trc/(MAX_Poy*MAX_Pox)],
                int IF, int IR, int IC, int OF, int OR, int OC, int pad, int K,
                uint8_t p_stride, uint8_t c_stride, uint8_t ps_sf_bit, uint8_t ps_mask, bool sq_en,
                uint16_t TR, uint16_t TC, uint16_t TM, uint16_t TN, uint8_t KxK, uint32_t IHxIW, uint16_t NTif,
                uint8_t lmode, uint16_t m, uint16_t r, uint16_t c, bool r_init_en, bool c_init_en, uint16_t TX_MIN_n1[3], uint16_t tx_n1[3],
                bool in_flag, bool proc_flag, bool lc_enable, uint8_t subK_num, uint8_t c_mode, uint8_t sr_bit, uint8_t ba_num, uint16_t *TR_Range_n,
                uint16_t *TC_Range_n){

    if(!lc_enable)
        return;

    static float ifm_buf_Poy[MAX_Poy][MAX_Pox][Tnax_dx*LANE_NUM][MAX_IB_HW];
    static float k_buf0[Tmax_dx*LANE_NUM][MAX_Tif][MAX_KxK];

    static uint16_t TRow_lps_n0[1], TCol_lps_n0[1];
    static uint16_t TM_MIN, TR_MIN, TC_MIN, TN_MIN;

    static uint16_t TR_MIN_sub[16], TC_MIN_sub[16], TR_MIN_max, TR_MIN_d_Poy, TR_Range;
    static uint16_t TC_MIN_max, TC_MIN_d_Poy, TC_Range;

    static uint32_t ofm_offset_base[16];

    TM_MIN = MIN(TM, OF-m);
    if(r_init_en){
        TR_MIN = MIN(TR, OR-r);
    }
    if(c_init_en){
        TC_MIN = MIN(TC, OC-c);
        TR_MIN_max = 0;
        TC_MIN_max = 0;

        uint16_t TR_MIN_aX = TR_MIN + ba_num;
        uint16_t TC_MIN_aX = TC_MIN + ba_num;

        for(uint8_t z=0; z<subK_num; z++){
            uint8_t x = z & ba_num;
            uint8_t y = z >> sr_bit;
            TR_MIN_sub[z] = (TR_MIN_aX-y) >> sr_bit;
            TC_MIN_sub[z] = (TC_MIN_aX-x) >> sr_bit;
            if(TR_MIN_sub[z] > TR_MIN_max)
                TR_MIN_max = TR_MIN_sub[z];
            if(TC_MIN_sub[z] > TC_MIN_max)
                TC_MIN_max = TC_MIN_sub[z];
            // ofm_offset_base[z] = y*TC_MIN + x;
        }

        uint16_t TR_MIN_t = TR_MIN_max;
        if(TR_MIN_t < MAX_Poy)
            TR_MIN_t = MAX_Poy;
        if(TR_MIN_t % MAX_Poy)
            TR_MIN_t = (TR_MIN_t + MAX_Poy-1)/MAX_Poy*MAX_Poy;
        // if(TR_MIN_t%MAX_Poy){
        //    printf("TR_MIN=%d, TR_MIN_t=%d, MAX_Poy=%d\n", TR_MIN, TR_MIN_t, MAX_Poy);
        // }
        assert(TR_MIN_t%MAX_Poy==0);
        TR_MIN_d_Poy = TR_MIN_t/MAX_Poy;
        TR_Range = TR_MIN_d_Poy << sr_bit;

        uint16_t TC_MIN_t = TC_MIN_max;
        if(TC_MIN_t < MAX_Pox)
            TC_MIN_t = MAX_Pox;
        // assert(TC_MIN_t%MAX_Pox==0);
        if(TC_MIN_t % MAX_Pox)
            TC_MIN_t = (TC_MIN_t + MAX_Pox-1)/MAX_Pox*MAX_Pox;
        TC_MIN_d_Poy = TC_MIN_t/MAX_Pox;
        TC_Range = TC_MIN_d_Poy << sr_bit;

        for(uint8_t z=0; z<subK_num; z++){
            uint8_t x = z & ba_num;
            uint8_t y = z >> sr_bit;
            ofm_offset_base[z] = y*TC_Range + x;
        }
    }

    uint16_t tn_r = 0;
    for(uint16_t tn = 0; tn < NTif; tn++)
    {
        load_ifm_k_wrapper( ifm_buf_Poy, k_buf0, ifm, kernel, m, tn_r, r, c, TM_MIN, TR_MIN, TC_MIN, K, KxK, TN, IF, IR, IC, OR, OC, IHxIW,
            p_stride, c_stride, pad, ps_sf_bit, ps_mask, sq_en, lmode, r_init_en, c_init_en, TRow_lps_n0, TCol_lps_n0, &TN_MIN, in_flag);

        conv2d_Acc_sub(ifm_buf_Poy, ofm_buf_Poy, k_buf0, K, TRow_lps_n0[0], TCol_lps_n0[0], c_stride, (tn_r==0), c_mode,
                subK_num, TM_MIN, TN_MIN, TR_MIN, TC_Range, TR_MIN_sub, TC_MIN_sub, proc_flag, TR_MIN_d_Poy, TC_MIN_d_Poy, sr_bit, ofm_offset_base);
        tn_r += TN;
    }

    tx_n1[0] = m; tx_n1[1] = r; tx_n1[2] = c;
    TX_MIN_n1[0] = TM_MIN; TX_MIN_n1[1] = TR_MIN; TX_MIN_n1[2] = TC_MIN;
    *TR_Range_n = TR_Range;
    *TC_Range_n = TC_Range;
}

void IC_codec(float *ofm, float *ifm, float *kernel, float *bias,
                int IF, int IR, int IC, int OF, int OR, int OC, int pad, int K, int S, int ltype,
                uint8_t p_stride, uint8_t c_stride, uint8_t wb_mode, uint8_t ps_sf_bit, uint8_t ps_mask, bool sq_en,
                uint16_t TR, uint16_t TC, uint16_t TM, uint16_t TN, uint32_t OHW, uint8_t KxK,
                uint32_t IHxIW, uint16_t p_stridexIR, uint16_t p_stridexIC, bool use_bias,
                uint16_t NToy, uint16_t NTox, uint16_t NTof, uint64_t NTcomb, uint16_t NTif, uint8_t lmode, uint64_t NTcomb_l){

    float ofm_buf_Poy[MAX_Poy][MAX_Pox][Tmax_dx*LANE_NUM][MAX_Trc/(MAX_Poy*MAX_Pox)];
    float b_buf[((MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM)][LANE_NUM];

    uint8_t c_mode;
    uint8_t subK_num;
    uint8_t ba_num = 0x0;
    uint8_t sr_bit = 0;
    if(K==5&&c_stride==1){//K==5&cs==1 CONVT:2
        c_mode = 2;
        subK_num = 4;
        sr_bit = 1;
        ba_num = 0x1;
    }else if(K==9&&c_stride==1){//K==9&cs==1 CONVT:3
        c_mode = 3;
        subK_num = 16;
        sr_bit = 2;
        ba_num = 0x3;
    }else{//normal:0
        // printf("c_stride=%d\n", c_stride);
        c_mode = 0;
        subK_num = 1;
        sr_bit = 0;
        ba_num = 0x0;
    }
    assert((subK_num==1)||(subK_num==4)||(subK_num==16));
    // printf("TR=%d, TC=%d, subK_num=%d\n", TR, TC, subK_num);

    if(use_bias){
        uint16_t ofm_num_dx = ((OF+LANE_NUM-1)/LANE_NUM);
        for(uint16_t x=0; x<ofm_num_dx; x++){
            memcpy(b_buf[x], bias + x*LANE_NUM, LANE_NUM*sizeof(float));
        }
    }

    uint16_t TX_MIN_n10[3];//TM_MIN, TR_MIN, TC_MIN
    uint16_t tx_n10[3];//m, r, c
    uint16_t TR_Range_n[1], TC_Range_n[1];

	uint16_t tr, tc, tm;
	uint16_t TR_MIN, TC_MIN, TM_MIN;
	uint16_t tm_r, tr_r, tc_r;
	bool in_flag, proc_flag, out_flag, lc_enable;
    in_flag = 1;
    proc_flag = 1;
    out_flag = 1;
    lc_enable = 1;

	tr = 0; tc = 0; tm = 0;
	tm_r = 0; tr_r = 0; tc_r = 0;
	bool r_init_en = 1, c_init_en = 1;
	for(uint64_t ntc = 0; ntc < NTcomb; ntc++){

        lc_wrapper(ifm, kernel, ofm_buf_Poy, IF, IR, IC, OF, OR, OC, pad, K, p_stride, c_stride, ps_sf_bit, ps_mask, sq_en,
                    TR, TC, TM, TN, KxK, IHxIW, NTif, lmode, tm_r, tr_r, tc_r, r_init_en, c_init_en,
                    TX_MIN_n10, tx_n10, in_flag, proc_flag, lc_enable, subK_num, c_mode, sr_bit, ba_num, TR_Range_n, TC_Range_n);

        codec_ofm_store_chw_tile(ofm, ofm_buf_Poy, ifm, b_buf,
            tx_n10[0], tx_n10[1], tx_n10[2], TX_MIN_n10[0], TX_MIN_n10[1], TX_MIN_n10[2], OHW, OR, OC, wb_mode, use_bias, out_flag, subK_num,
            TR_Range_n[0], TC_Range_n[0]);

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