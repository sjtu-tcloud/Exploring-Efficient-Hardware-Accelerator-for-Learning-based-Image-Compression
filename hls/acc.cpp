
#include "acc_new_reorg.h"

float u32tof32(uint32_t data_u32){
	union {
		uint32_t tmp_u32;
		float tmp_f32;
	}tmp_32b;

	tmp_32b.tmp_u32 = data_u32;

	return tmp_32b.tmp_f32;
}

float post_process(float tmp_in, float bias, float ifm_ex, uint8_t mode, bool bias_en){

    float tmp_out;
    if(bias_en){
        tmp_in = tmp_in + bias;
    }

    if(mode==0)
        tmp_out = tmp_in;
    else if(mode==1)
		tmp_out = ifm_ex*tmp_in;
	else
		tmp_out = ifm_ex/tmp_in;

    return tmp_out;
}


const uint32_t OFM_BLmax = 256;

void wb_ofm_lb(DT_IO *ofm, int32_t ofm_buf[Tm_fm_w_d2*LANE_NUM][MAX_Poy*MAX_Pox][MAX_Trc/(MAX_Poy*MAX_Pox)], float b_buf[((MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM)][LANE_NUM],
        uint8_t mode, bool bias_en, uint8_t subK_num, uint16_t TR_Range, uint16_t TC_Range, uint16_t m, uint16_t TR_MIN, uint16_t TC_MIN,
		float ifm_scale[(MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM][LANE_NUM], float ofm_scale[(MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM][LANE_NUM],
		float kernel_scale[(MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM][LANE_NUM], uint16_t tm_offset, uint32_t ifm_ex_buf[OFM_BLmax][LANE_NUM/2],
		uint32_t ofm_offset, uint32_t TBL_MIN, uint16_t tm_r, uint16_t tr_r, uint16_t tc_r,
		uint16_t *tm_s, uint16_t *tr_s, uint16_t *tc_s, bool tmrc_ud_en, uint8_t tmlps, uint32_t RTof_offset, bool enable){

	if(!enable)
		return ;

	DT_IO *OFM_base = ofm + ofm_offset;

	uint16_t tm, tr, tc;
	if(tmrc_ud_en){
		tm = tm_r; tr = tr_r; tc = tc_r;
	}else{
		tm = *tm_s; tr = *tr_s; tc = *tc_s;
	}

	for(int tbl_min=0; tbl_min < TBL_MIN; tbl_min++){
DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=OFM_BLmax)
#pragma HLS PIPELINE II=1

		uint16_t poy = tr/TR_Range;
		uint16_t pox = tc/TC_Range;
		uint32_t ofm_idx = (tr%TR_Range)*TC_Range + (tc%TC_Range) + tmlps*RTof_offset;

		int16_t tmp_ifm_ex[LANE_NUM];
#pragma HLS ARRAY_PARTITION variable=tmp_ifm_ex complete dim=1

		if(mode!=0){
            for(uint32_t x=0; x<LANE_NUM/2; x++){
#pragma HLS UNROLL
            	ap_uint<32> tmp_i32;
            	tmp_i32(31, 0) = ifm_ex_buf[tbl_min][x];
				union {
					int16_t tmp_i16;
					uint16_t tmp_apu16;
				} cvt16b0, cvt16b1;
				cvt16b0.tmp_apu16 = tmp_i32(15, 0);
				cvt16b1.tmp_apu16 = tmp_i32(31,16);
				tmp_ifm_ex[x*2  ] = cvt16b0.tmp_i16;
				tmp_ifm_ex[x*2+1] = cvt16b1.tmp_i16;
            }
		}else{
			for(uint32_t x=0; x<LANE_NUM; x++){
#pragma HLS UNROLL
				tmp_ifm_ex[x] = 0;
			}
		}

		uint16_t tm_abs = tm + tm_offset;
		DT_IO tmp_out = 0;
		for(uint32_t x=0; x<LANE_NUM; x++){
			union {
				int16_t tmp_i16;
				uint16_t tmp_apu16;
			} cvt16b;
			cvt16b.tmp_i16 = post_process(ofm_buf[tm*LANE_NUM+x][poy*MAX_Pox+pox][ofm_idx]*kernel_scale[m/LANE_NUM + tm_abs][x], b_buf[m/LANE_NUM + tm_abs][x],
											tmp_ifm_ex[x]*ifm_scale[m/LANE_NUM + tm_abs][x], mode, bias_en)
											*ofm_scale[m/LANE_NUM + tm_abs][x];
			tmp_out.range(16*x+15, 16*x) = cvt16b.tmp_apu16;
		}
		OFM_base[tbl_min] = tmp_out;

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

	*tm_s = tm; *tr_s = tr; *tc_s = tc;
}

void ld_ifm_ex(uint16_t *t1_t, uint16_t *t2_t, uint32_t *tbl_t,
		uint32_t ifm_ex_buf[OFM_BLmax][LANE_NUM/2], DT_IO *ifm_extra, bool IsAllCont, bool IsColCont,
		int base_offset, uint32_t OHW, uint16_t OW, uint16_t T2_MIN, int burstlen, uint8_t mode, uint32_t blmax_tc,
		uint32_t *ofm_offset_t, uint32_t *TBL_MIN_t, uint16_t *tm_t, uint16_t *tr_t, uint16_t *tc_t, bool *tmrc_ud_en_t,
		bool init_en, bool enable){

	if(!enable)
		return ;

	uint16_t tr,tm,tc;
	uint16_t t1, t2;
	uint32_t tbl;

	if(init_en){
		t1 = 0; t2 = 0; tbl = 0;
	}else{
		t1 = *t1_t; t2 = *t2_t; tbl = *tbl_t;
		tbl++;
		if(tbl==blmax_tc){
			tbl = 0;
			t2++;
			if(t2==T2_MIN){
				t2=0;
				t1++;
			}
		}
	}
	*t1_t = t1;
	*t2_t = t2;
	*tbl_t = tbl;


	uint32_t tbl_r = (tbl << 8);
	bool tmrc_ud_en = (tbl==0);
	if(tmrc_ud_en){
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
	}

	uint32_t ofm_offset0 = base_offset + tbl_r;
	uint32_t ofm_offset;
	if(IsAllCont){
		ofm_offset = ofm_offset0;
	}else if(IsColCont){
		ofm_offset = ofm_offset0 + t1*OHW;
	}else{
		ofm_offset = ofm_offset0 + t1*OHW + t2*OW;
	}

	uint32_t TBL_MIN = MIN(OFM_BLmax, burstlen-tbl_r);

    if(mode!=0){
    	DT_IO *IFM_ex_base = ifm_extra + ofm_offset;
		for(int tbl_min=0; tbl_min < TBL_MIN; tbl_min++){
DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=OFM_BLmax)
#pragma HLS PIPELINE II=1
				DT_IO tmp_ifm_ex_io;
				tmp_ifm_ex_io = IFM_ex_base[tbl_min];
	            for(uint32_t x=0; x<LANE_NUM/2; x++){
#pragma HLS UNROLL
	            	uint32_t tmp_ui32 = tmp_ifm_ex_io.range(32*x+31, 32*x);
					ifm_ex_buf[tbl_min][x] = tmp_ui32;
	            }
		}
    }

    *ofm_offset_t = ofm_offset;
    *TBL_MIN_t = TBL_MIN;
    *tm_t = tm;
    *tr_t = tr;
    *tc_t = tc;
    *tmrc_ud_en_t = tmrc_ud_en;
}

template<int id>
void mmcpy_o_wr_wrapper(DT_IO *ofm, int32_t ofm_buf[Tm_fm_w_d2*LANE_NUM][MAX_Poy*MAX_Pox][MAX_Trc/(MAX_Poy*MAX_Pox)], DT_IO *ifm_extra, float b_buf[((MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM)][LANE_NUM],
                            bool IsAllCont, bool IsColCont, uint32_t Tcomb_TC, int base_offset, uint32_t OHW, uint16_t OW, uint8_t mode, bool bias_en,
                            uint8_t subK_num, uint16_t TR_Range, uint16_t TC_Range, uint16_t m, uint16_t TR_MIN, uint16_t TC_MIN, uint16_t T2_MIN, int burstlen,
    float ifm_scale[(MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM][LANE_NUM], float ofm_scale[(MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM][LANE_NUM],
    float kernel_scale[(MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM][LANE_NUM], uint16_t tm_offset, uint32_t blmax_tc, bool en_flag, uint8_t tmlps, uint32_t RTof_offset)
{
#pragma HLS INLINE off

    if(!en_flag)
        return;

    uint32_t ifm_ex_buf0[OFM_BLmax][LANE_NUM/2];
#pragma HLS ARRAY_PARTITION variable=ifm_ex_buf0 complete dim=2

    uint32_t ifm_ex_buf1[OFM_BLmax][LANE_NUM/2];
#pragma HLS ARRAY_PARTITION variable=ifm_ex_buf1 complete dim=2

    uint32_t ofm_offset0, ofm_offset1;
    uint32_t TBL_MIN0, TBL_MIN1;
    uint16_t tm0, tr0, tc0, tm1, tr1, tc1;
    bool tmrc_ud_en0, tmrc_ud_en1;
    uint16_t tm_s, tr_s, tc_s;

    bool pp = 1;

	uint16_t t1, t2;
	uint32_t tbl;

	for(uint32_t tcb = 0; tcb < Tcomb_TC+1; tcb++){
		if(pp){
			ld_ifm_ex(&t1, &t2, &tbl, ifm_ex_buf0, ifm_extra, IsAllCont, IsColCont, base_offset, OHW, OW, T2_MIN,
					burstlen, mode, blmax_tc, &ofm_offset0, &TBL_MIN0, &tm0, &tr0, &tc0, &tmrc_ud_en0, (tcb==0), tcb < Tcomb_TC);

			wb_ofm_lb(ofm, ofm_buf, b_buf, mode, bias_en, subK_num, TR_Range, TC_Range, m, TR_MIN, TC_MIN,
					 ifm_scale, ofm_scale, kernel_scale, tm_offset, ifm_ex_buf1,
					 ofm_offset1, TBL_MIN1, tm1, tr1, tc1, &tm_s, &tr_s, &tc_s, tmrc_ud_en1, tmlps, RTof_offset, tcb>0);
			pp = 0;
		}else{
			ld_ifm_ex(&t1, &t2, &tbl, ifm_ex_buf1, ifm_extra, IsAllCont, IsColCont, base_offset, OHW, OW, T2_MIN,
					burstlen, mode, blmax_tc, &ofm_offset1, &TBL_MIN1, &tm1, &tr1, &tc1, &tmrc_ud_en1, (tcb==0), tcb < Tcomb_TC);

			wb_ofm_lb(ofm, ofm_buf, b_buf, mode, bias_en, subK_num, TR_Range, TC_Range, m, TR_MIN, TC_MIN,
					 ifm_scale, ofm_scale, kernel_scale, tm_offset, ifm_ex_buf0,
					 ofm_offset0, TBL_MIN0, tm0, tr0, tc0, &tm_s, &tr_s, &tc_s, tmrc_ud_en0, tmlps, RTof_offset, tcb>0);
			pp = 1;
		}
	}

}

void codec_ofm_store_chw_tile(DT_IO *ofm0, DT_IO *ofm1,
		int32_t ofm_buf[Np_wr][Tm_fm_w_d2*LANE_NUM][MAX_Poy*MAX_Pox][MAX_Trc/(MAX_Poy*MAX_Pox)],
		DT_IO *ifm_extra0, DT_IO *ifm_extra1,
	float b_buf[Np_wr][((MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM)][LANE_NUM],
    uint16_t m, uint16_t r, uint16_t c, uint16_t TM_MIN, uint16_t TR_MIN, uint16_t TC_MIN, uint32_t OHW,
    uint16_t OH, uint16_t OW, uint8_t mode, bool bias_en, bool enable, uint8_t subK_num, uint16_t TR_Range, uint16_t TC_Range,
    float ifm_scale[Np_wr][(MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM][LANE_NUM], float ofm_scale[Np_wr][(MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM][LANE_NUM],
    float kernel_scale[Np_wr][(MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM][LANE_NUM], uint32_t RTof_offset){

    if(!enable)
        return;

    int base_offset = (m/LANE_NUM)*OHW + r*OW + c;

	bool IsColCont = (OW==TC_MIN);//equal FMCont
	bool IsRowCont = (OH==TR_MIN);
	bool IsAllCont = IsColCont && IsRowCont;

	uint16_t TM_MINax_dx = (TM_MIN + LANE_NUM -1)/LANE_NUM;
    assert(Tmax_dx%Np_wr==0);

	uint16_t TC_MIN_sel, OW_sel;
	uint32_t OHW_sel;

	TC_MIN_sel = TC_MIN;
	OW_sel = OW;
	OHW_sel = OHW;

	uint16_t TM_MIN_sp[Np_wr];
#pragma HLS ARRAY_PARTITION variable=TM_MIN_sp complete
	int burstlen[Np_wr];
#pragma HLS ARRAY_PARTITION variable=burstlen complete
	uint32_t Tcomb_TC[Np_wr];
#pragma HLS ARRAY_PARTITION variable=Tcomb_TC complete
	uint32_t ofm_offset0[Np_wr];
#pragma HLS ARRAY_PARTITION variable=ofm_offset0 complete
	uint16_t T2_MIN[Np_wr];
#pragma HLS ARRAY_PARTITION variable=T2_MIN complete
	uint16_t tm_offset[Np_wr];
#pragma HLS ARRAY_PARTITION variable=tm_offset complete
	uint32_t blmax_tc[Np_wr];
#pragma HLS ARRAY_PARTITION variable=blmax_tc complete
	bool wb_en[2];
#pragma HLS ARRAY_PARTITION variable=wb_en complete

	uint16_t tm_tmp = 0;
    uint8_t TMlps = (TM_MINax_dx + Tmax_dx -1)/Tmax_dx;
    for(uint8_t tmlps=0; tmlps < TMlps; tmlps++){
#pragma HLS LOOP_FLATTEN off

 	uint16_t TMlp_max = Tm_fm_w_d2;
    uint8_t TM_MINax_dx_t;
    if(TM_MINax_dx > Tmax_dx){
        TM_MINax_dx_t = Tmax_dx;
        TM_MINax_dx = TM_MINax_dx - Tmax_dx;
    }else
        TM_MINax_dx_t = TM_MINax_dx;

 	if(TM_MINax_dx_t < TMlp_max)
 		TMlp_max = TM_MINax_dx_t;
 	uint16_t TMlp_min = TM_MINax_dx_t%Tm_fm_w_d2;
 	if(TMlp_min==0)
 		TMlp_min = Tm_fm_w_d2;

	bool flag = 1;
	for(int idx = 0; idx < Np_wr; idx++){
#pragma HLS PIPELINE II=1
		uint16_t TM_lp_tmp = (idx+1)*Tm_fm_w_d2;
		uint16_t TM_MIN_sp_tmp = 0;
		if(flag){
			if(TM_lp_tmp < TM_MINax_dx)
				TM_MIN_sp_tmp = TMlp_max;
			else
			{
				TM_MIN_sp_tmp = TMlp_min;
				flag = 0;
			}
		}
		TM_MIN_sp[idx] = TM_MIN_sp_tmp;
		tm_offset[idx] = tm_tmp;
		tm_tmp = tm_tmp + TM_MIN_sp_tmp;

		uint16_t T1_MIN, T2_MIN_t;
		uint8_t TM_MIN_t = TM_MIN_sp[idx];
		if(IsAllCont){
			burstlen[idx] = TM_MIN_t*TR_MIN*TC_MIN_sel;
			T1_MIN = 1;
			T2_MIN_t = 1;
		}else if(IsColCont){
			burstlen[idx] = TR_MIN*TC_MIN_sel;
			T1_MIN = TM_MIN_t;
			T2_MIN_t = 1;
		}else{
			burstlen[idx] = TC_MIN_sel;
			T1_MIN = TM_MIN_t;
			T2_MIN_t = TR_MIN;
		}

		T2_MIN[idx] = T2_MIN_t;
		uint8_t add_1b;
		if(burstlen[idx] & 0xFF)
			add_1b = 1;
		else
			add_1b = 0;

		uint32_t blmax_tc_t = (burstlen[idx] >> 8) + add_1b;
		blmax_tc[idx] =  blmax_tc_t;
		Tcomb_TC[idx]= T1_MIN*T2_MIN_t*blmax_tc_t;

		ofm_offset0[idx] = base_offset + tm_offset[idx]*OHW_sel;
		wb_en[idx] = TM_MIN_sp[idx] > 0;

	}

    mmcpy_o_wr_wrapper<0>(ofm0, ofm_buf[0], ifm_extra0, b_buf[0], IsAllCont, IsColCont, Tcomb_TC[0], ofm_offset0[0], OHW_sel, OW_sel, mode, bias_en,
                        subK_num, TR_Range, TC_Range, m, TR_MIN, TC_MIN_sel, T2_MIN[0], burstlen[0], ifm_scale[0], ofm_scale[0], kernel_scale[0],
                        tm_offset[0], blmax_tc[0], wb_en[0], tmlps, RTof_offset);

    mmcpy_o_wr_wrapper<1>(ofm1, ofm_buf[1], ifm_extra1, b_buf[1], IsAllCont, IsColCont, Tcomb_TC[1], ofm_offset0[1], OHW_sel, OW_sel, mode, bias_en,
                        subK_num, TR_Range, TC_Range, m, TR_MIN, TC_MIN_sel, T2_MIN[1], burstlen[1], ifm_scale[1], ofm_scale[1], kernel_scale[1],
                        tm_offset[1], blmax_tc[1], wb_en[1], tmlps, RTof_offset);
    }
}

template<int id>
void mmcpy_core_w_wraper(int32_t k_buf[Tm_fm_d4*LANE_NUM/2][MAX_Tif/MAX_Pif][MAX_Pif][MAX_KxK], DT_IO *kernel, uint32_t Woffset, uint32_t burstlen,
        uint8_t TN_MIN, uint8_t KxK, uint16_t t3_offset, bool en_flag){

#pragma HLS INLINE off

	if(!en_flag)
        return ;

	const uint32_t TmxTnxKxK = MAX_Tif*Tmax_dx*MAX_K*MAX_K;

	DT_IO *weight_addr = kernel + Woffset;

    uint8_t t1,t2,t3;
	t1 = 0; t2 = 0; t3 = 0;
	for(uint16_t t=0; t<burstlen; t++){
DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=TmxTnxKxK)
#pragma HLS PIPELINE II=1
		DT_IO tmp_io;
		tmp_io = weight_addr[t];
		for(uint32_t x=0; x<LANE_NUM/2; x++){
			union {
				int32_t tmp_i32;
				uint32_t tmp_u32;
			} cvt32b;
			cvt32b.tmp_u32 = tmp_io.range(32*x+31, 32*x);
			k_buf[t1*(LANE_NUM/2)+x][t2/MAX_Pif][t2%MAX_Pif][t3+t3_offset] = cvt32b.tmp_i32;
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

void load_kernel_reorg(int32_t k_buf[Np_rd_k][Tm_fm_d4*LANE_NUM/2][MAX_Tif/MAX_Pif][MAX_Pif][MAX_KxK],
							DT_IO *kernel0, DT_IO *kernel1, uint8_t m, uint8_t n,
                            uint8_t TM_MIN, uint8_t TN_MIN, uint8_t K, uint8_t KxK){

	static int Woffset;
	if(m==0&&n==0)
		Woffset = 0;

	uint16_t TM_MINax_dx = (TM_MIN + LANE_NUM -1)/LANE_NUM;

	for(uint8_t t2 = 0;t2 < MAX_Tif/MAX_Pif; t2++)
	for(uint8_t t3 = 0;t3 <KxK; t3++){
DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=MAX_KxK)
#pragma HLS PIPELINE II=1
    for(uint8_t idx = 0; idx < Np_rd_k; idx++)
    for(uint8_t t1 = 0;t1 < Tm_fm_d4*LANE_NUM/2; t1++)
	for(uint8_t t4 = 0;t4 < MAX_Pif; t4++){
		k_buf[idx][t1][t2][t4][t3] = 0;
	}}

    assert(Tmax_dx%Np_rd_k==0);

	uint16_t TMlp_max = Tm_fm_d4;
	if(TM_MINax_dx < TMlp_max)
		TMlp_max = TM_MINax_dx;
	uint16_t TMlp_min = TM_MINax_dx%Tm_fm_d4;
	if(TMlp_min==0)
		TMlp_min = Tm_fm_d4;

	uint16_t TM_MIN_sp[Np_rd_k];
#pragma HLS ARRAY_PARTITION variable=TM_MIN_sp complete dim=1
	uint32_t burstlen_sp[Np_rd_k];
#pragma HLS ARRAY_PARTITION variable=burstlen_sp complete dim=1
	uint32_t Woffset_sp[Np_rd_k];
#pragma HLS ARRAY_PARTITION variable=Woffset_sp complete dim=1
	uint16_t TN_MINxKK = TN_MIN*KxK;

	uint8_t TMlps = (TM_MINax_dx + Tmax_dx -1)/Tmax_dx;
	uint16_t t3_offset = 0;
	for(uint8_t tmlps=0; tmlps < TMlps; tmlps++){

		uint16_t TMlp_max = Tm_fm_d4;
		uint8_t TM_MINax_dx_t;
		if(TM_MINax_dx > Tmax_dx){
			TM_MINax_dx_t = Tmax_dx;
			TM_MINax_dx = TM_MINax_dx - Tmax_dx;
		}else
			TM_MINax_dx_t = TM_MINax_dx;

		if(TM_MINax_dx_t < TMlp_max)
			TMlp_max = TM_MINax_dx_t;
		uint16_t TMlp_min = TM_MINax_dx_t%Tm_fm_d4;
		if(TMlp_min==0)
			TMlp_min = Tm_fm_d4;

		bool flag = 1;
		for(int idx = 0; idx < Np_rd_k; idx++){
	#pragma HLS PIPELINE II=1
			uint16_t TM_lp_tmp = (idx+1)*Tm_fm_d4;
			uint16_t tmp_sp = 0;
			if(flag){
				if(TM_lp_tmp < TM_MINax_dx){
					tmp_sp = TMlp_max;
				}
				else
				{
					flag = 0;
					tmp_sp = TMlp_min;
				}
			}
			TM_MIN_sp[idx] = tmp_sp;
			burstlen_sp[idx] = tmp_sp*TN_MINxKK;
			Woffset_sp[idx] = Woffset;
			Woffset = Woffset + burstlen_sp[idx];
		}

		MP_w_rd:{
			mmcpy_core_w_wraper<0>( k_buf[0], kernel0, Woffset_sp[0], burstlen_sp[0], TN_MIN, KxK, t3_offset, TM_MIN_sp[0] > 0);
			mmcpy_core_w_wraper<1>( k_buf[1], kernel1, Woffset_sp[1], burstlen_sp[1], TN_MIN, KxK, t3_offset, TM_MIN_sp[1] > 0);
		}

        t3_offset += KxK;
	}
}

void row_init_cfg(uint16_t *TRow_lps, uint16_t IR, uint8_t ps_sf_bit, uint8_t ps_mask,
                    uint8_t c_stride, uint8_t pad, uint8_t K, uint16_t r, uint16_t TR_MIN,
                    uint16_t *px_t, uint16_t *px_b, uint16_t *row_len, int16_t *row_t, int16_t *row_b, uint16_t *px_tb, bool init_en){
#pragma HLS INLINE off
    if(!init_en)
        return;

    static int16_t Roffset;

    uint16_t TRow = (TR_MIN - 1)*c_stride + K;
    uint16_t TRow_lps_tmp = TRow >> ps_sf_bit;
    Roffset = (r*c_stride - pad) >> ps_sf_bit;

    uint16_t px_t_tmp, px_b_tmp;
    if(Roffset < 0)
        px_t_tmp = -Roffset;
    else
        px_t_tmp = 0;

    if((Roffset + TRow_lps_tmp -1) < IR)
        px_b_tmp = TRow_lps_tmp -1;
    else
        px_b_tmp = IR-1-Roffset;

    int16_t row_t_tmp, row_b_tmp;
    row_t_tmp = px_t_tmp+Roffset;
    row_b_tmp = px_b_tmp+Roffset;
    uint16_t row_len_tmp = row_b_tmp - row_t_tmp + 1;
    *TRow_lps = TRow_lps_tmp;
    *row_len = row_len_tmp;
    *px_tb = TRow_lps_tmp - row_len_tmp;
    *px_t = px_t_tmp;
    *px_b = px_b_tmp;
    *row_t = row_t_tmp;
    *row_b = row_b_tmp;
}

void col_init_cfg(uint16_t *TCol_lps, uint16_t IC, uint8_t ps_sf_bit, uint8_t ps_mask,
    uint8_t c_stride, uint8_t pad, uint8_t K, uint16_t c, uint16_t TC_MIN,
    uint16_t *px_l, uint16_t *px_r, int16_t *col_l, int16_t *col_r, uint16_t *col_len, uint16_t *px_lr, bool init_en){
#pragma HLS INLINE off
    if(!init_en)
        return;

    static int16_t Coffset;

    uint16_t TCol = (TC_MIN - 1)*c_stride + K;
    uint16_t TCol_lps_tmp = TCol >> ps_sf_bit;
    Coffset = (c*c_stride - pad) >> ps_sf_bit;

    uint16_t px_l_tmp, px_r_tmp;
    int16_t col_l_tmp, col_r_tmp;

    if(Coffset < 0)
        px_l_tmp = -Coffset;
    else
        px_l_tmp = 0;

    uint16_t br_x = Coffset + TCol_lps_tmp -1;
    if(br_x < IC)
        px_r_tmp = TCol_lps_tmp -1;
    else
        px_r_tmp = IC-1-Coffset;  //IC-1=Coffset+i

    col_l_tmp = px_l_tmp+Coffset;
    col_r_tmp = px_r_tmp+Coffset;

    uint16_t col_len_tmp = col_r_tmp - col_l_tmp + 1;
    *px_lr = TCol_lps_tmp - col_len_tmp;
    *px_l = px_l_tmp;
    *px_r = px_r_tmp;
    *col_l = col_l_tmp;
    *col_r = col_r_tmp;
    *col_len = col_len_tmp;
    *TCol_lps = TCol_lps_tmp;
}

void clean_ifm_bank(int32_t ifm_buf[MAX_IB_HW][MAX_Poy][MAX_Pox][LANE_NUM/2], uint16_t TRow, uint16_t TCol, bool cl_en){

#pragma HLS INLINE off

    if(!cl_en)
        return;

	for(uint16_t t2 = 0;t2 < TRow; t2++){
DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=MAX_IB_HW)
	for(uint16_t t3 = 0;t3 < TCol; t3++){
DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=MAX_IB_HW)
#pragma HLS PIPELINE II=1
		uint32_t t23_idx = t2*TCol + t3;
		for(int y = 0; y< MAX_Poy; y++)
		for(int z = 0; z< MAX_Pox; z++)
		for(int x=0; x<LANE_NUM/2; x++){
			ifm_buf[t23_idx][y][z][x] = 0;
		}
	}}
}

template<int id>
void mmcpy_ifm_rd_wrapper(int32_t ifm_buf[Tn_fm_d4][MAX_IB_HW][MAX_Poy][MAX_Pox][LANE_NUM/2], DT_IO *ifm,
		uint32_t IHxIW, uint16_t px_l, uint16_t px_t, uint16_t IC,
        bool square_en, uint16_t col_len, uint16_t row_len, bool IsAllCont, bool IsColCont, uint32_t burstlen,
        uint16_t T1_MIN, uint16_t T2_MIN, uint16_t TCol_lps, uint32_t offset_base, uint8_t ifm_sqQ_cb,
		uint16_t ifm_idx_poy_ofs[MAX_Poy], uint16_t ifm_idx_pox_ofs[MAX_Pox], uint16_t Poy_LEN, uint16_t Pox_LEN, bool en_flag){

#pragma HLS INLINE off

	if(!en_flag)
        return ;

    uint16_t t1, t2, t3;
    t1 = 0; t2 = 0; t3 = 0;
    for( uint16_t t1m = 0;t1m < T1_MIN; t1m++){
DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=MAX_Tif)
    for( uint16_t t2m = 0;t2m < T2_MIN; t2m++){
DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=MAX_IB_HW)

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

            DT_IO *ifm_addr = ifm + offset_ex;
            for(uint32_t tbl=0; tbl<burstlen; tbl++){
DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=MAX_IB_HW)
#pragma HLS PIPELINE II=1
                int16_t i = t3 + px_l;
                int16_t j = t2 + px_t;

                int16_t row_idx_re0[MAX_Poy], col_idx_re0[MAX_Pox];
#pragma HLS ARRAY_PARTITION variable=row_idx_re0 complete dim=0
#pragma HLS ARRAY_PARTITION variable=col_idx_re0 complete dim=0
                bool row_en_px[MAX_Poy], col_en_px[MAX_Pox];
#pragma HLS ARRAY_PARTITION variable=row_en_px complete dim=0
#pragma HLS ARRAY_PARTITION variable=col_en_px complete dim=0
                bool px_en[MAX_Poy][MAX_Pox];
#pragma HLS ARRAY_PARTITION variable=px_en complete dim=0
                uint16_t ifm_idx[MAX_Poy][MAX_Pox];
#pragma HLS ARRAY_PARTITION variable=ifm_idx complete dim=0

                for(int y = 0; y < MAX_Poy; y++){
                    row_idx_re0[y] = j - ifm_idx_poy_ofs[y];
                    row_en_px[y] = (row_idx_re0[y] >= 0) && (row_idx_re0[y] < Poy_LEN);
                }

                for(int z = 0; z < MAX_Pox; z++){
                    col_idx_re0[z] = i - ifm_idx_pox_ofs[z];
                    col_en_px[z] = (col_idx_re0[z] >= 0) && (col_idx_re0[z] < Pox_LEN);
                }

                for(int y = 0; y < MAX_Poy; y++)
                for(int z = 0; z < MAX_Pox; z++){
                    px_en[y][z] = row_en_px[y] && col_en_px[z];
                    ifm_idx[y][z] = col_idx_re0[z] + Pox_LEN*row_idx_re0[y];
                }

                DT_IO tmp_io;
                tmp_io = ifm_addr[tbl];

                for(uint32_t x=0; x<LANE_NUM/2; x++){
                    int16_t tmp_in0, tmp_in1;
                    int16_t tmp_out0, tmp_out1;
        			union {
        				int16_t tmp_i16;
        				uint16_t tmp_u16;
        			} cvt16b0, cvt16b1;

        			cvt16b0.tmp_u16 = tmp_io.range(16*(x*2)+15, 16*(x*2));
        			cvt16b1.tmp_u16 = tmp_io.range(16*(x*2+1)+15, 16*(x*2+1));
                    tmp_in0 = cvt16b0.tmp_i16;
                    tmp_in1 = cvt16b1.tmp_i16;

                    if(square_en && (tmp_in0<0))
                        tmp_out0 = -tmp_in0;
                    else
                        tmp_out0 = tmp_in0;

                    if(square_en && (tmp_in1<0))
                        tmp_out1 = -tmp_in1;
                    else
                        tmp_out1 = tmp_in1;

                    ap_int<32> tmp_i32;
                    tmp_i32.range(15, 0) = (int16_t)(tmp_out0 >> ifm_sqQ_cb);
                    tmp_i32.range(31,16) = (int16_t)(tmp_out1 >> ifm_sqQ_cb);

                    for(int y = 0; y < MAX_Poy; y++)
                    for(int z = 0; z < MAX_Pox; z++){
                        if(px_en[y][z]){
                            ifm_buf[t1][ifm_idx[y][z]][y][z][x] = tmp_i32.range(31, 0);
                        }
                    }

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

void load_ifm_pad_square(int32_t ifm_buf[Np_rd][Tn_fm_d4][MAX_IB_HW][MAX_Poy][MAX_Pox][LANE_NUM/2], DT_IO *ifm0, DT_IO *ifm1,
		uint16_t IR, uint16_t IC, uint8_t K, uint32_t IHxIW, uint8_t p_stride,
        uint8_t c_stride, uint8_t pad, uint8_t ps_sf_bit, uint8_t ps_mask, bool square_en, uint16_t TN_MIN,
        uint16_t TR_MIN, uint16_t TC_MIN, uint16_t n, uint16_t r, uint16_t c, uint8_t lmode,
		bool r_init_en, bool c_init_en, uint16_t Pox_LEN_n[1], uint8_t ifm_sqQ_cb, uint16_t IF,
		uint16_t TR_MIN_d_Poy, uint16_t TC_MIN_d_Pox, uint8_t K_ifm){

//to find the top-left real px_loc
    static uint16_t px_l, px_t, px_r, px_b;
    static int16_t col_l, col_r, row_t, row_b;
    static uint16_t col_len, row_len;
    static uint8_t cl_status = 0;

    static uint16_t px_lr, px_tb;

    static uint16_t TRow_lps, TCol_lps;

    row_init_cfg( &TRow_lps, IR, ps_sf_bit, ps_mask, c_stride, pad, K, r, TR_MIN, &px_t, &px_b, &row_len, &row_t, &row_b, &px_tb, r_init_en);
    col_init_cfg( &TCol_lps, IC, ps_sf_bit, ps_mask, c_stride, pad, K, c, TC_MIN, &px_l, &px_r, &col_l, &col_r, &col_len, &px_lr, c_init_en);

    bool cl_flag = 0;
    bool cl_en = (px_tb!=0) || (px_lr!=0);
    if(c_init_en&&(!n)){
        cl_status = 1;
        cl_flag = cl_en;
    }else{
        if(cl_status==1){
            cl_status = 0;
            cl_flag = cl_en;
        }
    }

    static uint16_t ifm_idx_poy_ofs[Np_rd][MAX_Poy];
#pragma HLS ARRAY_PARTITION variable=ifm_idx_poy_ofs complete dim=1
#pragma HLS ARRAY_PARTITION variable=ifm_idx_poy_ofs complete dim=2
    static uint16_t ifm_idx_pox_ofs[Np_rd][MAX_Pox];
#pragma HLS ARRAY_PARTITION variable=ifm_idx_pox_ofs complete dim=1
#pragma HLS ARRAY_PARTITION variable=ifm_idx_pox_ofs complete dim=2

    static uint16_t cs_x_TRMdPoy, cs_x_TCMdPox;
    static uint16_t Poy_LEN, Pox_LEN;
    if(c_init_en){
        cs_x_TRMdPoy = c_stride*TR_MIN_d_Poy;
        cs_x_TCMdPox = c_stride*TC_MIN_d_Pox;
        Poy_LEN = (TR_MIN_d_Poy-1)*c_stride+K_ifm;
        Pox_LEN = (TC_MIN_d_Pox-1)*c_stride+K_ifm;

        for(uint16_t poy = 0; poy < MAX_Poy; poy++){
//#pragma HLS PIPELINE II=1
        	uint16_t tmp_u16 = poy*cs_x_TRMdPoy;
        	for(int x=0; x<Np_rd; x++){
#pragma HLS UNROLL
        		ifm_idx_poy_ofs[x][poy] = tmp_u16;
        	}
        }

        for(uint16_t pox = 0; pox < MAX_Pox; pox++){
#pragma HLS PIPELINE II=1
        	uint16_t tmp_u16 = pox*cs_x_TCMdPox;
        	for(int x=0; x<Np_rd; x++){
#pragma HLS UNROLL
        		ifm_idx_pox_ofs[x][pox] = tmp_u16;
        	}
        }
    }
    Pox_LEN_n[0] = Pox_LEN;

    for(int idx = 0; idx < Np_rd; idx++)
	for(int k = 0; k < Tn_fm_d4; k++){
#pragma HLS UNROLL
        clean_ifm_bank(ifm_buf[idx][k], Poy_LEN, Pox_LEN, cl_flag);
	}

    assert((col_len <= IC)&&(col_len > 0));
    assert((row_len <= IR)&&(row_len > 0));

	bool IsColCont = (col_len == IC);
	bool IsRowCont = (row_len == IR);
	bool IsAllCont = IsRowCont && IsColCont;

    uint16_t TN_MINax_dx = (TN_MIN + LANE_NUM -1)/LANE_NUM;
	uint16_t nax_dx = n/LANE_NUM;

	uint16_t col_l_sel, col_len_sel, IC_sel;
	uint32_t IHxIW_sel;

	col_l_sel = col_l;
	col_len_sel = col_len;
	IC_sel = IC;
	IHxIW_sel = IHxIW;

	uint16_t TNlp_max = Tn_fm_d4;
	if(TN_MINax_dx < TNlp_max)
		TNlp_max = TN_MINax_dx;
	uint16_t TNlp_min = TN_MINax_dx%Tn_fm_d4;
	if(TNlp_min==0)
		TNlp_min = Tn_fm_d4;

	uint16_t TN_MIN_sp[Np_rd];
#pragma HLS ARRAY_PARTITION variable=TN_MIN_sp complete dim=1
	uint32_t burstlen[Np_rd];
#pragma HLS ARRAY_PARTITION variable=burstlen complete dim=1
	uint32_t offset_base[Np_rd];
#pragma HLS ARRAY_PARTITION variable=offset_base complete dim=1
	uint16_t T1_MIN[Np_rd], T2_MIN[Np_rd];
#pragma HLS ARRAY_PARTITION variable=T1_MIN complete dim=1
#pragma HLS ARRAY_PARTITION variable=T2_MIN complete dim=1

	uint16_t n_tmp = nax_dx;
	bool flag = 1;
	for(int idx = 0; idx < Np_rd; idx++){
#pragma HLS PIPELINE II=1
		uint16_t TN_lp_tmp = (idx+1)*Tn_fm_d4;
		uint16_t tmp_sp = 0;
		if(flag){
			if(TN_lp_tmp < TN_MINax_dx){
				tmp_sp = TNlp_max;
			}
			else
			{
				flag = 0;
				tmp_sp = TNlp_min;
			}
		}
		TN_MIN_sp[idx] = tmp_sp;
		uint16_t n_base_t = n_tmp;
		n_tmp = n_tmp + tmp_sp;

		uint16_t TN_MIN_t = TN_MIN_sp[idx];
		if(IsAllCont){
			burstlen[idx] = TN_MIN_t*row_len*col_len_sel;
			offset_base[idx] = n_base_t*IHxIW_sel;
			T1_MIN[idx] = 1;
			T2_MIN[idx] = 1;
		}else if(IsColCont){
			burstlen[idx] = row_len*col_len_sel;
			offset_base[idx] = n_base_t*IHxIW_sel + row_t*IC_sel;
			T1_MIN[idx] = TN_MIN_t;
			T2_MIN[idx] = 1;
		}else{
			burstlen[idx] = col_len_sel;
			offset_base[idx] = n_base_t*IHxIW_sel + row_t*IC_sel + col_l_sel;
			T1_MIN[idx] = TN_MIN_t;
			T2_MIN[idx] = row_len;
		}
	}

    mmcpy_ifm_rd_wrapper<0>( ifm_buf[0], ifm0, IHxIW_sel, px_l, px_t, IC_sel, square_en, col_len_sel, row_len,
    		IsAllCont, IsColCont, burstlen[0], T1_MIN[0], T2_MIN[0], TCol_lps, offset_base[0], ifm_sqQ_cb,
			ifm_idx_poy_ofs[0], ifm_idx_pox_ofs[0], Poy_LEN, Pox_LEN, TN_MIN_sp[0] > 0);

    mmcpy_ifm_rd_wrapper<1>( ifm_buf[1], ifm1, IHxIW_sel, px_l, px_t, IC_sel, square_en, col_len_sel, row_len,
    		IsAllCont, IsColCont, burstlen[1], T1_MIN[1], T2_MIN[1], TCol_lps, offset_base[1], ifm_sqQ_cb,
			ifm_idx_poy_ofs[1], ifm_idx_pox_ofs[1], Poy_LEN, Pox_LEN, TN_MIN_sp[1] > 0);
}

void load_ifm_k_wrapper(int32_t ifm_buf[Np_rd][Tn_fm_d4][MAX_IB_HW][MAX_Poy][MAX_Pox][LANE_NUM/2],
		int32_t k_buf[Np_rd_k][Tm_fm_d4*LANE_NUM/2][MAX_Tif/MAX_Pif][MAX_Pif][MAX_KxK],
		DT_IO *ifm0, DT_IO *ifm1, DT_IO *kernel0, DT_IO *kernel1,
        uint8_t m, uint8_t n, uint16_t r, uint16_t c, uint8_t TM_MIN, uint16_t TR_MIN, uint16_t TC_MIN, uint8_t K, uint8_t KxK, uint16_t TN, uint16_t IF, uint16_t IR, uint16_t IC,
        uint16_t OR, uint16_t OC, uint32_t IHxIW, uint8_t p_stride, uint8_t c_stride, uint8_t pad,
        uint8_t ps_sf_bit, uint8_t ps_mask, bool sq_en, uint8_t lmode, bool r_init_en, bool c_init_en,
		uint16_t TCol_lps_n[1],  uint16_t *TN_MIN_n, bool enable, uint8_t ifm_sqQ_cb, uint16_t TR_MIN_d_Poy, uint16_t TC_MIN_d_Pox, uint8_t K_ifm){

#pragma HLS INLINE off

    if(!enable)
        return;

    uint16_t TN_MIN = MIN(TN, IF-n);

    load_ifm_pad_square( ifm_buf, ifm0, ifm1, IR, IC, K, IHxIW, p_stride, c_stride, pad, ps_sf_bit, ps_mask, sq_en,
                TN_MIN, TR_MIN, TC_MIN, n, r, c, lmode, r_init_en, c_init_en, TCol_lps_n, ifm_sqQ_cb, IF, TR_MIN_d_Poy, TC_MIN_d_Pox, K_ifm);
    load_kernel_reorg(k_buf, kernel0, kernel1, m, n, TM_MIN, TN_MIN, K, KxK);

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

void MAX_Tof_PEs_Piof(int32_t ifm_buf[Np_rd][Tn_fm_d4][MAX_IB_HW][MAX_Poy][MAX_Pox][LANE_NUM/2],
		int32_t ofm_buf[Np_wr][Tm_fm_w_d2*LANE_NUM][MAX_Poy*MAX_Pox][MAX_Trc/(MAX_Poy*MAX_Pox)],
		int32_t k_buf[Np_rd_k][Tm_fm_d4*LANE_NUM/2][MAX_Tif/MAX_Pif][MAX_Pif][MAX_KxK],
		bool init_flag, uint32_t ifm_idx[MAX_Poy][MAX_Pox], uint8_t k_idx, uint32_t ofm_idx,
		uint8_t pif, uint8_t pof){

#pragma HLS INLINE

#pragma HLS DEPENDENCE variable=ofm_buf inter false

	uint8_t pofxMAX_Pof = pof*MAX_Pof;
	uint8_t pifxMAX_Pif = pif*MAX_Pif;

	int16_t w_16i[MAX_Pof][MAX_Pif];
#pragma HLS ARRAY_PARTITION variable=w_16i complete dim=0

    for(int pm=0; pm< (MAX_Pof/2); pm++){
        uint8_t tm = pofxMAX_Pof + pm*2;
        for(int pn = 0;pn < MAX_Pif; pn++)
        {
			ap_int<32> tmp_k0 = k_buf[tm/(Tm_fm_d4*LANE_NUM)][(tm%(Tm_fm_d4*LANE_NUM))/2][pif][pn][k_idx];
			w_16i[pm*2  ][pn] = tmp_k0.range(15, 0);
			w_16i[pm*2+1][pn] = tmp_k0.range(31,16);
    }}

	for(uint16_t poy = 0; poy < MAX_Poy; poy++)
	for(uint16_t pox = 0; pox < MAX_Pox; pox++){

		int16_t tmp_ifm_16i[MAX_Pif];
#pragma HLS ARRAY_PARTITION variable=tmp_ifm_16i complete dim=0

        for(int pn = 0;pn < (MAX_Pif/2); pn++)
        {
        	uint8_t tn = pifxMAX_Pif + pn*2;
			ap_int<32> tmp_ifm = ifm_buf[(tn/LANE_NUM)/Tn_fm_d4][(tn/LANE_NUM)%Tn_fm_d4][ifm_idx[poy][pox]][poy][pox][(tn%LANE_NUM)/2];
			tmp_ifm_16i[pn*2  ] = tmp_ifm.range(15, 0);
			tmp_ifm_16i[pn*2+1] = tmp_ifm.range(31,16);
        }

		for(int pm=0; pm< MAX_Pof; pm++){

			uint8_t tm = pofxMAX_Pof + pm;
			int32_t partial_sum;
	        if(init_flag){
	            partial_sum = 0;
	        }else{
	        	partial_sum = ofm_buf[tm/(LANE_NUM*Tm_fm_w_d2)][tm%(LANE_NUM*Tm_fm_w_d2)][poy*MAX_Pox+pox][ofm_idx];
	        }

			int32_t partial_add[MAX_Pif];
	#pragma HLS ARRAY_PARTITION variable=partial_add complete dim=0

			for(int pn = 0;pn < MAX_Pif; pn++)
			{
				partial_add[pn] = tmp_ifm_16i[pn]*w_16i[pm][pn];
			}

			ofm_buf[tm/(LANE_NUM*Tm_fm_w_d2)][tm%(LANE_NUM*Tm_fm_w_d2)][poy*MAX_Pox+pox][ofm_idx] =
					partial_sum + adder_tree<int32_t, (MAX_Pif)>(partial_add);
		}
	}
}

void conv2d_offset_calc_sub(int K, int TCol_lps, uint8_t mode, uint8_t subK_num, uint8_t c_stride,
	 uint16_t *TC_MIN_sub, uint16_t TR_MIN_d_Poy, uint16_t TC_MIN_d_Pox,
     uint8_t Ky_s[16], uint8_t Kx_s[16], uint8_t k_offset_s[16], uint32_t ifm_offset_s[16],
	 uint32_t *KyxTRC_MIN_s, uint64_t *Tcb_sk_num, uint16_t *cs_x_TColps, bool enable){

#pragma HLS INLINE off

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
DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=16)
#pragma HLS PIPELINE II=1

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

        KyxTRC_MIN_s[z] = Ky*Kx*TR_MIN_d_Poy*TC_MIN_d_Pox;
        Tcb_sk_num_tmp = Tcb_sk_num_tmp + KyxTRC_MIN_s[z];
    }

    uint16_t cs_x_TColps_t = c_stride*TCol_lps;
    *cs_x_TColps = cs_x_TColps_t;
    *Tcb_sk_num = Tcb_sk_num_tmp;
}

void conv2d_Acc_sub(int32_t ifm_buf[Np_rd][Tn_fm_d4][MAX_IB_HW][MAX_Poy][MAX_Pox][LANE_NUM/2],
		int32_t ofm_buf[Np_wr][Tm_fm_w_d2*LANE_NUM][MAX_Poy*MAX_Pox][MAX_Trc/(MAX_Poy*MAX_Pox)],
		int32_t k_buf[Np_rd_k][Tm_fm_d4*LANE_NUM/2][MAX_Tif/MAX_Pif][MAX_Pif][MAX_KxK],
     int K, uint16_t TCol_lps, uint8_t c_stride, bool init_en, uint8_t mode, uint8_t subK_num,
	 uint16_t TM_MIN, uint16_t TN_MIN, uint16_t *TC_MIN_sub, uint16_t TR_MIN_d_Poy, uint16_t TC_MIN_d_Pox,
	      uint16_t TC_Range, uint8_t sub_bit, uint32_t *ofm_offset_base, uint16_t RTof, uint32_t RTof_offset, bool enable){

    if(!enable)
        return ;

    static uint8_t Ky_s[16], Kx_s[16], k_offset_s[16];
    static uint32_t ifm_offset_s[16];
    static uint32_t KyxTRC_MIN_s[16];
    static uint64_t Tcb_sk_num;
    static uint16_t cs_x_TColps;

    static uint8_t c_stride_t;
    static uint16_t TCol_lps_t;
    if(init_en){
    	c_stride_t = c_stride;
    	TCol_lps_t = TCol_lps;
    }

    conv2d_offset_calc_sub( K, TCol_lps_t, mode, subK_num, c_stride, TC_MIN_sub, TR_MIN_d_Poy, TC_MIN_d_Pox,
                Ky_s, Kx_s, k_offset_s, ifm_offset_s, KyxTRC_MIN_s, &Tcb_sk_num, &cs_x_TColps, init_en);

    uint8_t k_offset, Kx, ky, kx;
    uint16_t TR_MIN, TC_MIN, tr, tc;
    uint32_t KyxTRC_MIN;
    uint8_t z;
    uint32_t tcb;
    uint32_t ofm_offset_base_val, ifm_offset_base;

	assert(MAX_Tif%MAX_Pif==0);
	assert(MAX_Tof%MAX_Pof==0);

	const uint32_t MAX_POF = (MAX_Tof/MAX_Pof);
	const uint32_t MAX_PIF = (MAX_Tif/MAX_Pif);

    uint8_t TM_lps = (TM_MIN+MAX_Tof-1)/MAX_Tof;
    uint8_t TM_MIN_t;
    if(TM_lps>1)
        TM_MIN_t = TM_MIN/RTof;
    else
        TM_MIN_t = TM_MIN;
    uint16_t Pof = (TM_MIN_t/MAX_Pof);
    if(TM_MIN_t%MAX_Pof)
        Pof = Pof + 1;

	uint16_t Pif = (TN_MIN/MAX_Pif);
	if(TN_MIN%MAX_Pif)
		Pif = Pif + 1;
	assert((Pof>=1)&&(Pof<=MAX_POF));
	assert((Pif>=1)&&(Pif<=MAX_PIF));

	uint8_t KxK = K*K;

	uint32_t ifm_idx;
	uint8_t k_idx;
	uint32_t ofm_idx;

	uint64_t Tcb_sk_num_comb = Tcb_sk_num*Pof*Pif;
    uint64_t Tcb_sk_num_comb_t = Tcb_sk_num_comb*TM_lps;

	uint16_t pof, pif;
    uint64_t tcb_xc = 0;
    uint8_t tmlps = 0;

    for(uint64_t tcb_x = 0; tcb_x < Tcb_sk_num_comb_t; tcb_x++){
#pragma HLS PIPELINE II=1

        if(tcb_xc==0){
            pof = 0; pif = 0; z = 0; tcb = 0;
            tr = 0; tc = 0; ky = 0; kx = 0;
        }

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
		uint32_t ifm_offset = ifm_offset_base + c_stride_t*tc + kx + TCol_lps_t*ky + tr*cs_x_TColps;
		uint8_t k_idx = ky*Kx + kx + k_offset + tmlps*KxK;
		uint32_t ofm_idx = ((tr*TC_Range + tc) << sub_bit) + ofm_offset_base_val + tmlps*RTof_offset;

		uint32_t ifm_idx[MAX_Poy][MAX_Pox];
#pragma HLS ARRAY_PARTITION variable=ifm_idx complete dim=0
		for(uint16_t poy = 0; poy < MAX_Poy; poy++)
		for(uint16_t pox = 0; pox < MAX_Pox; pox++){
			ifm_idx[poy][pox] = ifm_offset;
		}

        MAX_Tof_PEs_Piof(ifm_buf, ofm_buf, k_buf, init_flag, ifm_idx, k_idx, ofm_idx, pif, pof);

        tcb_xc++;
        if(tcb_xc==Tcb_sk_num_comb){
            tcb_xc=0; tmlps++;
        }

		tc++;
		if(tc==TC_MIN_d_Pox){
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

void lc_wrapper(DT_IO *ifm0, DT_IO *ifm1, DT_IO *kernel0, DT_IO *kernel1,
		int32_t ofm_buf[Np_wr][Tm_fm_w_d2*LANE_NUM][MAX_Poy*MAX_Pox][MAX_Trc/(MAX_Poy*MAX_Pox)],
                int IF, int IR, int IC, int OF, int OR, int OC, int pad, int K,
                uint8_t p_stride, uint8_t c_stride, uint8_t ps_sf_bit, uint8_t ps_mask, bool sq_en,
                uint16_t TR, uint16_t TC, uint16_t TM, uint16_t TN, uint8_t KxK, uint32_t IHxIW, uint16_t NTif,
                uint8_t lmode, uint16_t m, uint16_t r, uint16_t c, bool r_init_en, bool c_init_en, uint16_t TX_MIN_n1[3], uint16_t tx_n1[3], bool pp,
                bool in_flag, bool proc_flag, bool lc_enable, uint8_t subK_num, uint8_t c_mode,
				uint8_t sr_bit, uint8_t ba_num, uint16_t *TR_Range_n, uint16_t *TC_Range_n, uint8_t ifm_sqQ_cb, uint8_t K_ifm, uint16_t RTof, uint32_t RTof_offset){

    if(!lc_enable)
        return;

    assert(LANE_NUM%2==0);

    static int32_t ifm_buf0[Np_rd][Tn_fm_d4][MAX_IB_HW][MAX_Poy][MAX_Pox][LANE_NUM/2];
#pragma HLS ARRAY_PARTITION variable=ifm_buf0 complete dim=1
#pragma HLS ARRAY_PARTITION variable=ifm_buf0 complete dim=2
#pragma HLS ARRAY_PARTITION variable=ifm_buf0 complete dim=4
#pragma HLS ARRAY_PARTITION variable=ifm_buf0 complete dim=5
#pragma HLS ARRAY_PARTITION variable=ifm_buf0 complete dim=6
    static int32_t ifm_buf1[Np_rd][Tn_fm_d4][MAX_IB_HW][MAX_Poy][MAX_Pox][LANE_NUM/2];
#pragma HLS ARRAY_PARTITION variable=ifm_buf1 complete dim=1
#pragma HLS ARRAY_PARTITION variable=ifm_buf1 complete dim=2
#pragma HLS ARRAY_PARTITION variable=ifm_buf1 complete dim=4
#pragma HLS ARRAY_PARTITION variable=ifm_buf1 complete dim=5
#pragma HLS ARRAY_PARTITION variable=ifm_buf1 complete dim=6
    static int32_t k_buf0[Np_rd_k][Tm_fm_d4*LANE_NUM/2][MAX_Tif/MAX_Pif][MAX_Pif][MAX_KxK];
#pragma HLS ARRAY_PARTITION variable=k_buf0 complete dim=1
#pragma HLS ARRAY_PARTITION variable=k_buf0 complete dim=2
#pragma HLS ARRAY_PARTITION variable=k_buf0 complete dim=4
    static int32_t k_buf1[Np_rd_k][Tm_fm_d4*LANE_NUM/2][MAX_Tif/MAX_Pif][MAX_Pif][MAX_KxK];
#pragma HLS ARRAY_PARTITION variable=k_buf1 complete dim=1
#pragma HLS ARRAY_PARTITION variable=k_buf1 complete dim=2
#pragma HLS ARRAY_PARTITION variable=k_buf1 complete dim=4

	static uint16_t tx_n[2][3];
#pragma HLS ARRAY_PARTITION variable=tx_n complete dim=1
#pragma HLS ARRAY_PARTITION variable=tx_n complete dim=2
	static uint16_t TX_MIN_n[2][4], TX_Range_n[2], TZ_Range_n[2];
#pragma HLS ARRAY_PARTITION variable=TX_MIN_n complete dim=1
#pragma HLS ARRAY_PARTITION variable=TX_MIN_n complete dim=2
#pragma HLS ARRAY_PARTITION variable=TX_Range_n complete dim=1
#pragma HLS ARRAY_PARTITION variable=TZ_Range_n complete dim=1
    static uint16_t TCol_lps_n0[1], TCol_lps_n1[1];
    static uint16_t TR_MIN_d_Poy_n[2], TC_MIN_d_Pox_n[2];
#pragma HLS ARRAY_PARTITION variable=TR_MIN_d_Poy_n complete dim=1
#pragma HLS ARRAY_PARTITION variable=TC_MIN_d_Pox_n complete dim=1

    static uint16_t TM_MIN, TR_MIN, TC_MIN, TN_MIN;

    static uint16_t TR_MIN_sub[16], TC_MIN_sub[16], TR_MIN_max, TR_MIN_d_Poy, TR_Range;
    static uint16_t TC_MIN_max, TC_MIN_d_Pox, TC_Range;

    static uint32_t ofm_offset_base[16];
    static uint32_t ofm_offset_base_n[2][16];
#pragma HLS ARRAY_PARTITION variable=ofm_offset_base_n complete dim=1
    static uint16_t TC_MIN_sub_n[2][16];
#pragma HLS ARRAY_PARTITION variable=TC_MIN_sub_n complete dim=1

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
DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=16)
#pragma HLS PIPELINE II=1
            uint8_t x = z & ba_num;
            uint8_t y = z >> sr_bit;
            TR_MIN_sub[z] = (TR_MIN_aX-y) >> sr_bit;
            TC_MIN_sub[z] = (TC_MIN_aX-x) >> sr_bit;
            if(TR_MIN_sub[z] > TR_MIN_max)
                TR_MIN_max = TR_MIN_sub[z];
            if(TC_MIN_sub[z] > TC_MIN_max)
                TC_MIN_max = TC_MIN_sub[z];
        }

        uint16_t TR_MIN_t = TR_MIN_max;
        if(TR_MIN_t < MAX_Poy)
            TR_MIN_t = MAX_Poy;
        else if(TR_MIN_t % MAX_Poy)
            TR_MIN_t = (TR_MIN_t + MAX_Poy-1)/MAX_Poy*MAX_Poy;

        TR_MIN_d_Poy = TR_MIN_t/MAX_Poy;
        TR_Range = TR_MIN_d_Poy << sr_bit;

        uint16_t TC_MIN_t = TC_MIN_max;
        if(TC_MIN_t < MAX_Pox)
            TC_MIN_t = MAX_Pox;
        else if(TC_MIN_t % MAX_Pox)
            TC_MIN_t = (TC_MIN_t + MAX_Pox-1)/MAX_Pox*MAX_Pox;
        TC_MIN_d_Pox = TC_MIN_t/MAX_Pox;
        TC_Range = TC_MIN_d_Pox << sr_bit;

        for(uint8_t z=0; z<subK_num; z++){
DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=16)
#pragma HLS PIPELINE II=1
            uint8_t x = z & ba_num;
            uint8_t y = z >> sr_bit;
            ofm_offset_base[z] = y*TC_Range + x;
        }
    }

	if(lmode==0){
		if(pp){
            load_ifm_k_wrapper( ifm_buf0, k_buf0, ifm0, ifm1, kernel0, kernel1, m, 0, r, c, TM_MIN, TR_MIN, TC_MIN, K, KxK, TN, IF, IR, IC, OR, OC, IHxIW,
                p_stride, c_stride, pad, ps_sf_bit, ps_mask, sq_en, lmode, r_init_en, c_init_en, TCol_lps_n0, &TN_MIN, in_flag,
				ifm_sqQ_cb, TR_MIN_d_Poy, TC_MIN_d_Pox, K_ifm);

            conv2d_Acc_sub(ifm_buf1, ofm_buf, k_buf1, K, TCol_lps_n1[0], c_stride, 1, c_mode, subK_num,
            		TX_MIN_n[1][0], TX_MIN_n[1][3], TC_MIN_sub_n[1], TR_MIN_d_Poy_n[1], TC_MIN_d_Pox_n[1], TZ_Range_n[1], sr_bit,
            							ofm_offset_base_n[1], RTof, RTof_offset, proc_flag);

			tx_n[0][0] = m; tx_n[0][1] = r; tx_n[0][2] = c;
			TR_MIN_d_Poy_n[0] = TR_MIN_d_Poy; TX_Range_n[0] = TR_Range;
			TC_MIN_d_Pox_n[0] = TC_MIN_d_Pox; TZ_Range_n[0] = TC_Range;
			TX_MIN_n[0][0] = TM_MIN; TX_MIN_n[0][1] = TR_MIN; TX_MIN_n[0][2] = TC_MIN; TX_MIN_n[0][3] = TN_MIN;

            for(uint8_t z=0; z<subK_num; z++){
DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=16)
#pragma HLS PIPELINE II=1
				ofm_offset_base_n[0][z] = ofm_offset_base[z];
                TC_MIN_sub_n[0][z] = TC_MIN_sub[z];
            }

			tx_n1[0] = tx_n[1][0]; tx_n1[1] = tx_n[1][1]; tx_n1[2] = tx_n[1][2];
			TX_MIN_n1[0] = TX_MIN_n[1][0]; TX_MIN_n1[1] = TX_MIN_n[1][1]; TX_MIN_n1[2] = TX_MIN_n[1][2];
			*TR_Range_n = TX_Range_n[1];
			*TC_Range_n = TZ_Range_n[1];
		}else{
            load_ifm_k_wrapper( ifm_buf1, k_buf1, ifm0, ifm1, kernel0, kernel1, m, 0, r, c, TM_MIN, TR_MIN, TC_MIN, K, KxK, TN, IF, IR, IC, OR, OC, IHxIW,
                p_stride, c_stride, pad, ps_sf_bit, ps_mask, sq_en, lmode, r_init_en, c_init_en, TCol_lps_n1, &TN_MIN, in_flag,
				ifm_sqQ_cb, TR_MIN_d_Poy, TC_MIN_d_Pox, K_ifm);

            conv2d_Acc_sub(ifm_buf0, ofm_buf, k_buf0, K, TCol_lps_n0[0], c_stride, 1, c_mode, subK_num,
            		TX_MIN_n[0][0], TX_MIN_n[0][3], TC_MIN_sub_n[0], TR_MIN_d_Poy_n[0], TC_MIN_d_Pox_n[0], TZ_Range_n[0], sr_bit,
            							ofm_offset_base_n[0], RTof, RTof_offset, proc_flag);

			tx_n[1][0] = m; tx_n[1][1] = r; tx_n[1][2] = c;
			TR_MIN_d_Poy_n[1] = TR_MIN_d_Poy; TX_Range_n[1] = TR_Range;
			TC_MIN_d_Pox_n[1] = TC_MIN_d_Pox; TZ_Range_n[1] = TC_Range;
			TX_MIN_n[1][0] = TM_MIN; TX_MIN_n[1][1] = TR_MIN; TX_MIN_n[1][2] = TC_MIN; TX_MIN_n[1][3] = TN_MIN;

            for(uint8_t z=0; z<subK_num; z++){
DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=16)
#pragma HLS PIPELINE II=1
				ofm_offset_base_n[1][z] = ofm_offset_base[z];
                TC_MIN_sub_n[1][z] = TC_MIN_sub[z];
            }

			tx_n1[0] = tx_n[0][0]; tx_n1[1] = tx_n[0][1]; tx_n1[2] = tx_n[0][2];
			TX_MIN_n1[0] = TX_MIN_n[0][0]; TX_MIN_n1[1] = TX_MIN_n[0][1]; TX_MIN_n1[2] = TX_MIN_n[0][2];
			*TR_Range_n = TX_Range_n[0];
			*TC_Range_n = TZ_Range_n[0];
		}
	}else{
            bool pp_tn = 1;
            uint16_t tn_n[2];
            uint16_t tn_r = 0;
            for(uint16_t tn = 0; tn < NTif+1; tn++)
            {
                bool in_flag = tn < NTif;
                bool proc_flag = tn > 0;
                if(pp_tn){
                    load_ifm_k_wrapper( ifm_buf0, k_buf0, ifm0, ifm1, kernel0, kernel1, m, tn_r, r, c, TM_MIN, TR_MIN, TC_MIN, K, KxK, TN, IF, IR, IC, OR, OC, IHxIW,
                        p_stride, c_stride, pad, ps_sf_bit, ps_mask, sq_en, lmode, r_init_en, c_init_en, TCol_lps_n0, &TN_MIN, in_flag,
						ifm_sqQ_cb, TR_MIN_d_Poy, TC_MIN_d_Pox, K_ifm);

                    conv2d_Acc_sub(ifm_buf1, ofm_buf, k_buf1, K, TCol_lps_n1[0], c_stride, (tn_n[1]==0), c_mode, subK_num,
                    		TX_MIN_n[1][0], TX_MIN_n[1][3], TC_MIN_sub_n[1], TR_MIN_d_Poy_n[1], TC_MIN_d_Pox_n[1], TZ_Range_n[1], sr_bit,
                    		ofm_offset_base_n[1], RTof, RTof_offset, proc_flag);

                    tn_n[0] = tn_r;
                    tx_n[0][0] = m; tx_n[0][1] = r; tx_n[0][2] = c;
                    TR_MIN_d_Poy_n[0] = TR_MIN_d_Poy; TC_MIN_d_Pox_n[0] = TC_MIN_d_Pox; TZ_Range_n[0] = TC_Range;
                    TX_MIN_n[0][0] = TM_MIN; TX_MIN_n[0][1] = TR_MIN; TX_MIN_n[0][2] = TC_MIN; TX_MIN_n[0][3] = TN_MIN;

                    for(uint8_t z=0; z<subK_num; z++){
DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=16)
#pragma HLS PIPELINE II=1
						ofm_offset_base_n[0][z] = ofm_offset_base[z];
                        TC_MIN_sub_n[0][z] = TC_MIN_sub[z];
                    }

                    tn_r += TN;
                    pp_tn = 0;
                }else{
                    load_ifm_k_wrapper( ifm_buf1, k_buf1, ifm0, ifm1, kernel0, kernel1, m, tn_r, r, c, TM_MIN, TR_MIN, TC_MIN, K, KxK, TN, IF, IR, IC, OR, OC, IHxIW,
                        p_stride, c_stride, pad, ps_sf_bit, ps_mask, sq_en, lmode, r_init_en, c_init_en, TCol_lps_n1, &TN_MIN, in_flag,
						ifm_sqQ_cb, TR_MIN_d_Poy, TC_MIN_d_Pox, K_ifm);

                    conv2d_Acc_sub(ifm_buf0, ofm_buf, k_buf0, K, TCol_lps_n0[0], c_stride, (tn_n[0]==0), c_mode, subK_num,
                    		TX_MIN_n[0][0], TX_MIN_n[0][3], TC_MIN_sub_n[0], TR_MIN_d_Poy_n[0], TC_MIN_d_Pox_n[0], TZ_Range_n[0], sr_bit,
							ofm_offset_base_n[0], RTof, RTof_offset, proc_flag);

                    tn_n[1] = tn_r;
                    tx_n[1][0] = m; tx_n[1][1] = r; tx_n[1][2] = c;
                    TR_MIN_d_Poy_n[1] = TR_MIN_d_Poy; TC_MIN_d_Pox_n[1] = TC_MIN_d_Pox; TZ_Range_n[1] = TC_Range;
                    TX_MIN_n[1][0] = TM_MIN; TX_MIN_n[1][1] = TR_MIN; TX_MIN_n[1][2] = TC_MIN; TX_MIN_n[1][3] = TN_MIN;

                    for(uint8_t z=0; z<subK_num; z++){
DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=16)
#pragma HLS PIPELINE II=1
						ofm_offset_base_n[1][z] = ofm_offset_base[z];
                        TC_MIN_sub_n[1][z] = TC_MIN_sub[z];
                    }

                    tn_r += TN;
                    pp_tn = 1;
                }
            }

            tx_n1[0] = m; tx_n1[1] = r; tx_n1[2] = c;
			TX_MIN_n1[0] = TM_MIN; TX_MIN_n1[1] = TR_MIN; TX_MIN_n1[2] = TC_MIN;
			*TR_Range_n = TR_Range;
			*TC_Range_n = TC_Range;
	}
}

void aligned_bias_scale_mmcpy(float dst_buf[Np_wr][Betanx_dx][LANE_NUM], DT_IO *src, int data_num, bool enable){

    if(!enable)
        return ;

    for(int x=0; x<data_num; x++){
DO_PRAGMA(HLS LOOP_TRIPCOUNT min=1 max=Betanx_dx)
    for(int z=0; z<2; z++){
#pragma HLS PIPELINE II=1
    	DT_IO tmp_128b = src[x*2 + z];
        for(int y=0; y<LANE_NUM/2; y++)
        {
        	uint32_t tmp_u32 = tmp_128b.range(y*32+31, y*32);
            for(int idx = 0; idx < Np_wr; idx++){
                dst_buf[idx][x][z*(LANE_NUM/2) + y] = u32tof32(tmp_u32);
            }
        }
    }}

}

void IC_codec(DT_IO *ofm0, DT_IO *ofm1, DT_IO *ifm0, DT_IO *ifm1,
				DT_IO *ifm_extra0, DT_IO *ifm_extra1, DT_IO *kernel0, DT_IO *kernel1,
				DT_IO *bias, DT_IO *ifm_scale, DT_IO *ofm_scale, DT_IO *kernel_scale,
                int IF, int IR, int IC, int OF, int OR, int OC, int pad, int K, int S, int ltype,
                uint8_t p_stride, uint8_t c_stride, uint8_t wb_mode, uint8_t ps_sf_bit, uint8_t ps_mask, bool sq_en,
                uint16_t TR, uint16_t TC, uint16_t TM, uint16_t TN, uint32_t OHW, uint8_t KxK,
                uint32_t IHxIW, uint16_t p_stridexIR, uint16_t p_stridexIC, bool use_bias,
                uint16_t NToy, uint16_t NTox, uint16_t NTof, uint64_t NTcomb, uint16_t NTif, uint8_t lmode, uint64_t NTcomb_l,
                uint8_t ifm_sqQ, uint8_t interQ, uint8_t FMreod, uint16_t RTof){

#pragma HLS INTERFACE m_axi depth=134217728 port=ifm0 offset=slave bundle=DATA_BUS0 num_read_outstanding=1 num_write_outstanding=1 max_read_burst_length=256 max_write_burst_length=256
#pragma HLS INTERFACE m_axi depth=134217728 port=ifm1 offset=slave bundle=DATA_BUS1 num_read_outstanding=1 num_write_outstanding=1 max_read_burst_length=256 max_write_burst_length=256
#pragma HLS INTERFACE m_axi depth=1048576 port=kernel0 offset=slave bundle=DATA_BUS2 num_read_outstanding=1 num_write_outstanding=1 max_read_burst_length=256 max_write_burst_length=256
#pragma HLS INTERFACE m_axi depth=1048576 port=kernel1 offset=slave bundle=DATA_BUS3 num_read_outstanding=1 num_write_outstanding=1 max_read_burst_length=256 max_write_burst_length=256

#pragma HLS INTERFACE m_axi depth=134217728 port=ifm_extra0 offset=slave bundle=DATA_BUS4 num_read_outstanding=1 num_write_outstanding=1 max_read_burst_length=256 max_write_burst_length=256
#pragma HLS INTERFACE m_axi depth=134217728 port=ifm_extra1 offset=slave bundle=DATA_BUS5 num_read_outstanding=1 num_write_outstanding=1 max_read_burst_length=256 max_write_burst_length=256
#pragma HLS INTERFACE m_axi depth=134217728 port=ofm0 offset=slave bundle=DATA_BUS0 num_read_outstanding=1 num_write_outstanding=1 max_read_burst_length=256 max_write_burst_length=256
#pragma HLS INTERFACE m_axi depth=134217728 port=ofm1 offset=slave bundle=DATA_BUS1 num_read_outstanding=1 num_write_outstanding=1 max_read_burst_length=256 max_write_burst_length=256

#pragma HLS INTERFACE m_axi depth=1280 port=bias      offset=slave bundle=DATA_BUS0 num_read_outstanding=1 num_write_outstanding=1 max_read_burst_length=256 max_write_burst_length=256
#pragma HLS INTERFACE m_axi depth=1280 port=ifm_scale offset=slave bundle=DATA_BUS1 num_read_outstanding=1 num_write_outstanding=1 max_read_burst_length=256 max_write_burst_length=256
#pragma HLS INTERFACE m_axi depth=1280 port=ofm_scale offset=slave bundle=DATA_BUS2 num_read_outstanding=1 num_write_outstanding=1 max_read_burst_length=256 max_write_burst_length=256
#pragma HLS INTERFACE m_axi depth=1280 port=kernel_scale offset=slave bundle=DATA_BUS3 num_read_outstanding=1 num_write_outstanding=1 max_read_burst_length=256 max_write_burst_length=256

#pragma HLS INTERFACE s_axilite register port=return bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=IF bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=IR bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=IC bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=OF bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=OR bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=OC bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=pad bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=K bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=S bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=ltype bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=p_stride bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=c_stride bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=wb_mode bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=ps_sf_bit bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=ps_mask bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=sq_en bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=TR bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=TC bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=TM bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=TN bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=OHW bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=KxK bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=IHxIW bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=p_stridexIR bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=p_stridexIC bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=use_bias bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=NToy bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=NTox bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=NTof bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=NTcomb bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=NTif bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=lmode bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=NTcomb_l bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=ifm_sqQ bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=interQ bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=FMreod bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=RTof bundle=CTRL_BUS

#pragma HLS INTERFACE s_axilite register port=ifm0 bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=ifm1 bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=ifm_extra0 bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=ifm_extra1 bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=kernel0 bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=kernel1 bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=ofm0 bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=ofm1 bundle=CTRL_BUS

#pragma HLS INTERFACE s_axilite register port=bias bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=ifm_scale bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=ofm_scale bundle=CTRL_BUS
#pragma HLS INTERFACE s_axilite register port=kernel_scale bundle=CTRL_BUS

    int32_t ofm_buf0[Np_wr][Tm_fm_w_d2*LANE_NUM][MAX_Poy*MAX_Pox][MAX_Trc/(MAX_Poy*MAX_Pox)];
#pragma HLS ARRAY_PARTITION variable=ofm_buf0 complete dim=1
#pragma HLS ARRAY_PARTITION variable=ofm_buf0 complete dim=2
#pragma HLS ARRAY_PARTITION variable=ofm_buf0 complete dim=3
    int32_t ofm_buf1[Np_wr][Tm_fm_w_d2*LANE_NUM][MAX_Poy*MAX_Pox][MAX_Trc/(MAX_Poy*MAX_Pox)];
#pragma HLS ARRAY_PARTITION variable=ofm_buf1 complete dim=1
#pragma HLS ARRAY_PARTITION variable=ofm_buf1 complete dim=2
#pragma HLS ARRAY_PARTITION variable=ofm_buf1 complete dim=3

	float b_buf[Np_wr][Betanx_dx][LANE_NUM];
#pragma HLS ARRAY_PARTITION variable=b_buf complete dim=1
#pragma HLS ARRAY_PARTITION variable=b_buf complete dim=3
	float ofm_scale_l[Np_wr][Betanx_dx][LANE_NUM];
#pragma HLS ARRAY_PARTITION variable=ofm_scale_l complete dim=1
#pragma HLS ARRAY_PARTITION variable=ofm_scale_l complete dim=3
	float kernel_scale_l[Np_wr][Betanx_dx][LANE_NUM];
#pragma HLS ARRAY_PARTITION variable=kernel_scale_l complete dim=1
#pragma HLS ARRAY_PARTITION variable=kernel_scale_l complete dim=3
	float ifm_scale_l[Np_wr][Betanx_dx][LANE_NUM];
#pragma HLS ARRAY_PARTITION variable=ifm_scale_l complete dim=1
#pragma HLS ARRAY_PARTITION variable=ifm_scale_l complete dim=3

	uint32_t RTof_offset = ((MAX_Trc/(MAX_Poy*MAX_Pox))/RTof);

    uint8_t c_mode;
    uint8_t subK_num;
    uint8_t ba_num = 0x0;
    uint8_t sr_bit = 0;
    uint8_t K_ifm = 0;
    if(K==5&&c_stride==1){//K==5&cs==1 CONVT:2
        c_mode = 2;
        subK_num = 4;
        sr_bit = 1;
        ba_num = 0x1;
        K_ifm = 3;
    }else if(K==9&&c_stride==1){//K==9&cs==1 CONVT:3
        c_mode = 3;
        subK_num = 16;
        sr_bit = 2;
        ba_num = 0x3;
        K_ifm = 3;
    }else{//normal:0
        c_mode = 0;
        subK_num = 1;
        sr_bit = 0;
        ba_num = 0x0;
        K_ifm = K;
    }

    uint8_t iQ = interQ/2;
    uint8_t ifm_sqQ_cb = ifm_sqQ + iQ;

    uint16_t ofm_num_dx = ((OF+LANE_NUM-1)/LANE_NUM);
    uint16_t ifm_num_dx = ((IF+LANE_NUM-1)/LANE_NUM);

    aligned_bias_scale_mmcpy(b_buf, bias, ofm_num_dx, use_bias);
    aligned_bias_scale_mmcpy(ifm_scale_l, ifm_scale, ifm_num_dx, wb_mode!=0);
    aligned_bias_scale_mmcpy(ofm_scale_l, ofm_scale, ofm_num_dx, 1);
    aligned_bias_scale_mmcpy(kernel_scale_l, kernel_scale, ofm_num_dx, 1);

    uint16_t TX_MIN_n10[3], TX_MIN_n11[3];//TM_MIN, TR_MIN, TC_MIN
#pragma HLS ARRAY_PARTITION variable=TX_MIN_n10 complete dim=1
#pragma HLS ARRAY_PARTITION variable=TX_MIN_n11 complete dim=1
    uint16_t tx_n10[3], tx_n11[3];//m, r, c
#pragma HLS ARRAY_PARTITION variable=tx_n10 complete dim=1
#pragma HLS ARRAY_PARTITION variable=tx_n11 complete dim=1
    uint16_t TR_Range_n10[1], TR_Range_n11[1], TC_Range_n10[1], TC_Range_n11[1];

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
            lc_enable = (ntc < NTcomb + 1);
		}else{
			in_flag = 1;
			proc_flag = 1;
			out_flag = ntc > 0;
			lc_enable = ntc < NTcomb;
		}

		if(pp){
                lc_wrapper(ifm0, ifm1, kernel0, kernel1, ofm_buf0, IF, IR, IC, OF, OR, OC, pad, K, p_stride, c_stride, ps_sf_bit, ps_mask, sq_en,
                            TR, TC, TM, TN, KxK, IHxIW, NTif, lmode, tm_r, tr_r, tc_r, r_init_en, c_init_en,
                            TX_MIN_n10, tx_n10, pp, in_flag, proc_flag, lc_enable, subK_num, c_mode, sr_bit, ba_num, TR_Range_n10, TC_Range_n10,
							ifm_sqQ_cb, K_ifm, RTof, RTof_offset);

                codec_ofm_store_chw_tile(ofm0, ofm1, ofm_buf1, ifm_extra0, ifm_extra1,
                		b_buf, tx_n11[0], tx_n11[1], tx_n11[2], TX_MIN_n11[0], TX_MIN_n11[1], TX_MIN_n11[2],
                    OHW, OR, OC, wb_mode, use_bias, out_flag, subK_num, TR_Range_n11[0], TC_Range_n11[0], ifm_scale_l, ofm_scale_l, kernel_scale_l, RTof_offset);

			pp = 0;
		}else{
                lc_wrapper(ifm0, ifm1, kernel0, kernel1, ofm_buf1, IF, IR, IC, OF, OR, OC, pad, K, p_stride, c_stride, ps_sf_bit, ps_mask, sq_en,
                            TR, TC, TM, TN, KxK, IHxIW, NTif, lmode, tm_r, tr_r, tc_r, r_init_en, c_init_en,
                            TX_MIN_n11, tx_n11, pp, in_flag, proc_flag, lc_enable, subK_num, c_mode, sr_bit, ba_num, TR_Range_n11, TC_Range_n11,
							ifm_sqQ_cb, K_ifm, RTof, RTof_offset);

                codec_ofm_store_chw_tile(ofm0, ofm1, ofm_buf0, ifm_extra0, ifm_extra1,
                		b_buf, tx_n10[0], tx_n10[1], tx_n10[2], TX_MIN_n10[0], TX_MIN_n10[1], TX_MIN_n10[2],
                    OHW, OR, OC, wb_mode, use_bias, out_flag, subK_num, TR_Range_n10[0], TC_Range_n10[0], ifm_scale_l, ofm_scale_l, kernel_scale_l, RTof_offset);

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



