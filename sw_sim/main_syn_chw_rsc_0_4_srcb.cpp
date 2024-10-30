
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "yolov2_reorg_rsc_0_3_srcb.h"

const uint32_t F_num = 128;
const uint32_t LNUM = 5;

int LANE_ext(int tmp_in){
	int val = (((tmp_in + LANE_NUM -1)/LANE_NUM)*LANE_NUM);
	return val;
}

int LANE_div(int tmp_in){
	int val = ((tmp_in + LANE_NUM -1)/LANE_NUM);
	return val;
}

int main( int argc, char *argv[])
{
	const int IF_NUM_set[LNUM] = {F_num, F_num, F_num, F_num, F_num};
	const int OF_NUM_set[LNUM] = {F_num, F_num, F_num, F_num,     3};
	const int KS_set[LNUM] = {5, 1, 5, 1, 9};
	const int P_set[LNUM]  = {2, 0, 2, 0, 4};
	const int S_set[LNUM]  = {2, 1, 2, 1, 4};
	const bool bias_en_set[LNUM] = {true, true, true, true, true};
	const int LT_set[LNUM] = {LT_CONVT, LT_IGDN, LT_CONVT, LT_IGDN, LT_CONVT};
	const char *LN_s[LNUM] = {"syn_0", "igdn_0", "syn_1", "igdn_1", "syn_2"};

	int IW_set[LNUM], IH_set[LNUM], OW_set[LNUM], OH_set[LNUM];
	int IFM_offset[LNUM], OFM_offset[LNUM];

	FILE *fp;
	int data_num;
	assert(argc==3);

	data_num = get_file_size(argv[2]);
	int32_t *ana_IHW_OHW_set = (int32_t *)malloc(data_num);
	read_binfile_flt32_rb((float *)ana_IHW_OHW_set, argv[2], data_num/4);
	for(int lnum = 0; lnum < LNUM; lnum++){
		IH_set[lnum] = ana_IHW_OHW_set[LNUM*2 + LNUM-lnum-1];
		IW_set[lnum] = ana_IHW_OHW_set[LNUM*3 + LNUM-lnum-1];
		OH_set[lnum] = ana_IHW_OHW_set[       + LNUM-lnum-1];
		OW_set[lnum] = ana_IHW_OHW_set[LNUM   + LNUM-lnum-1];
	}
	free(ana_IHW_OHW_set);

	int fm_max_size = 0;
	int fm_max_size_single = 0;
	int ifm_channel_num = 0, ofm_channel_num = 0;
	for(int lnum = 0; lnum < LNUM; lnum++){
		ifm_channel_num = ifm_channel_num + LANE_ext(IF_NUM_set[lnum]);
		ofm_channel_num = ofm_channel_num + LANE_ext(OF_NUM_set[lnum]);

		int ifm_offset = IW_set[lnum]*IH_set[lnum]*LANE_ext(IF_NUM_set[lnum]);
		int ofm_offset = OW_set[lnum]*OH_set[lnum]*LANE_ext(OF_NUM_set[lnum]);
		if(lnum==0){
			fm_max_size = ifm_offset + ofm_offset;
		}else{
			int tmp_fm_size = fm_max_size - ifm_offset;
			if(tmp_fm_size < ofm_offset)
				fm_max_size = ifm_offset + ofm_offset;
		}

		if(ifm_offset > fm_max_size_single)
			fm_max_size_single = ifm_offset;
		if(ofm_offset > fm_max_size_single)
			fm_max_size_single = ofm_offset;
	}
	printf("fm_max_size_calc = %d\n",fm_max_size);

	for(int lnum = 0; lnum < LNUM; lnum++){
		int ifm_offset = IW_set[lnum]*IH_set[lnum]*LANE_ext(IF_NUM_set[lnum]);
		int ofm_offset = OW_set[lnum]*OH_set[lnum]*LANE_ext(OF_NUM_set[lnum]);
		if(lnum==0){
			IFM_offset[lnum] = 0;
			OFM_offset[lnum] = fm_max_size - ofm_offset;
		}else{
			IFM_offset[lnum] = OFM_offset[lnum-1];
			if(IFM_offset[lnum]!=0){
				OFM_offset[lnum] = 0;
			}else{
				OFM_offset[lnum] = fm_max_size - ofm_offset;
			}
		}
	}

	data_num = get_file_size("syn_kernel_w_T_i16rc.bin");
	int16_t *kernel_w = (int16_t *)malloc(data_num);
	printf("read weight byte_num = %d\n", read_binfile_flt32_rb((float *)kernel_w, "syn_kernel_w_T_i16rc.bin", data_num/4));

	data_num = get_file_size("syn_bias_f32c.bin");
	float *bias = (float *)malloc(data_num);
	printf("%d\n", read_binfile_flt32_rb((float *)bias, "syn_bias_f32c.bin", data_num/4));
	printf("read bias byte_num = %d\n", data_num);

	int w_aoffset[LNUM];
	printf("%d\n", read_binfile_flt32_rb((float*)w_aoffset, "syn_kernel_w_T_i16rc_oadd.bin", LNUM));

	int bias_aoffset[LNUM];
	printf("%d\n", read_binfile_flt32_rb((float*)bias_aoffset, "syn_bias_f32c_oadd.bin", LNUM));

	float *ifm_Scale = (float *)calloc(sizeof(float), ifm_channel_num);
	read_binfile_flt32_rb((float*)ifm_Scale, "syn_i16c_ifm_scale.bin", ifm_channel_num);
	float *ofm_Scale = (float *)calloc(sizeof(float), ofm_channel_num);
	read_binfile_flt32_rb((float*)ofm_Scale, "syn_i16c_ofm_scale.bin", ofm_channel_num);
	float *kernel_Scale = (float *)calloc(sizeof(float), ofm_channel_num);
	read_binfile_flt32_rb((float*)kernel_Scale, "syn_i16c_kernel_scale.bin", ofm_channel_num);

	int ifm_sqQarray[LNUM];
	read_binfile_flt32_rb((float*)ifm_sqQarray, "syn_i16c_ifm_sqQ.bin", LNUM);
	int interQarray[LNUM];
	read_binfile_flt32_rb((float*)interQarray, "syn_i16c_interQ.bin", LNUM);

	int16_t *fm_mem = (int16_t *)malloc(sizeof(int16_t)*fm_max_size);

	data_num = IH_set[0]*IW_set[0]*F_num;
	float *tmp_in_buf = (float *)malloc(sizeof(float)*data_num);

	fp = fopen(argv[1], "rb");
	fread(tmp_in_buf, sizeof(float), data_num, fp);
	fclose(fp);
	printf("syn_input: %s, h_w_c= [%d, %d, %d]\n", argv[1], IH_set[0], IW_set[0], F_num);

	int img_c = F_num, img_h = IH_set[0], img_w = IW_set[0];
	int16_t *ifm_buf = fm_mem + IFM_offset[0];
	int Ca3_d4 =  LANE_div(img_c);
	for(int c4=0; c4<Ca3_d4; c4++)
	for(int ih=0; ih<img_h; ih++)
	for(int iw=0; iw<img_w; iw++)
	for(int ic=0; ic<LANE_NUM; ic++)
	{
		int idx_out = c4*img_h*img_w*LANE_NUM + ih*img_w*LANE_NUM + iw*LANE_NUM + ic;
		int idx_in = (c4*LANE_NUM+ic)*img_h*img_w + ih*img_w + iw;

		if((c4*LANE_NUM+ic) >= img_c)
			ifm_buf[idx_out] = 0;
		else
			ifm_buf[idx_out] = (tmp_in_buf[idx_in]/ifm_Scale[c4*LANE_NUM+ic])*pow(2.0, 14);
	}
	free(tmp_in_buf);

	float *ofm_scale_last_buf = (float *)malloc(LANE_ext(OF_NUM_set[LNUM-1])*sizeof(float));
	int ofm_scale_last_offset = 0;
	for(int lnum = 0; lnum < LNUM; lnum++){
		int ofm_num = OF_NUM_set[lnum];
		if(lnum==(LNUM-1)){
			memcpy(ofm_scale_last_buf, ofm_Scale + ofm_scale_last_offset, LANE_ext(ofm_num)*sizeof(float));
		}
		ofm_scale_last_offset += LANE_ext(ofm_num);
	}

	int16_t *kernel_w_ptr = kernel_w;
	float *bias_ptr = bias;
	int16_t *ifm_ptr, *ofm_ptr;
	double time1, time2;

	float *ifm_Scale_ptr = ifm_Scale;
	float *ofm_Scale_ptr = ofm_Scale;
	float *kernel_Scale_ptr = kernel_Scale;

	for(int lnum = 0; lnum < LNUM; lnum++){
		time1 = what_time_is_it_now();

		int ltype = LT_set[lnum];
		bool bias_en = bias_en_set[lnum];
		int pad = P_set[lnum];
		int ih = IH_set[lnum];
		int iw = IW_set[lnum];
		int oh = OH_set[lnum];
		int ow = OW_set[lnum];
		int ksize = KS_set[lnum];
		int kstride = S_set[lnum];
		int ifm_num = IF_NUM_set[lnum];
		int ofm_num = OF_NUM_set[lnum];

		uint8_t p_stride, c_stride;
		switch (ltype)
		{
			case LT_CONVT:
				p_stride = kstride;
				c_stride = 1;
				break;
			case LT_IGDN:
				p_stride = kstride;
				c_stride = kstride;
				break;
			default:
				p_stride = 1;
				c_stride = kstride;
				break;
		}

		uint8_t wb_mode;
		bool sq_en = 0;
		if(ltype==LT_GDN){
			sq_en = 1;
			wb_mode = 2;
		}else if(ltype==LT_IGDN){
			sq_en = 1;
			wb_mode = 1;
		}else{
			wb_mode = 0;
		}

		uint8_t ps_sf_bit, ps_mask;
		switch (p_stride)
		{
		case 1:
			ps_sf_bit = 0;
			ps_mask = 0x0;
			break;
		case 2:
			ps_sf_bit = 1;
			ps_mask = 0x1;
			break;
		case 4:
			ps_sf_bit = 2;
			ps_mask = 0x3;
			break;
		default:
			ps_sf_bit = 0;
			ps_mask = 0x0;
			break;
		}

		uint8_t c_mode;
		uint8_t subK_num;
		if(ksize==5&&c_stride==1){//K==5&cs==1 CONVT:2
			c_mode = 2;
			subK_num = 4;
		}else if(ksize==9&&c_stride==1){//K==9&cs==1 CONVT:3
			c_mode = 3;
			subK_num = 16;
		}else{//normal:0
			c_mode = 0;
			subK_num = 1;
		}

		uint16_t TR, TC, TM, TN;
		int max_Trc4TC = MAX_Trc;
		assert(MAX_IB_HW > ksize);
		if(c_mode==2){
			TR = 2;
		}else if(c_mode==3){
			TR = 4;
		}else{
			TR = 1;
		}
		int TRow_max = (TR-1)*c_stride + ksize;
		int max_ib_hw_left = MAX_IB_HW/TRow_max;
		max_Trc4TC = max_Trc4TC/TR;

		TC = MIN(((max_ib_hw_left-ksize)/c_stride+1), ow);//at least left K rows for calc
		TC = MIN(max_Trc4TC, TC);
		if(c_mode==2){
			assert(TC>=2);
			TC = (TC/2)*2;
		}else if(c_mode==3){
			assert(TC>=4);
			TC = (TC/4)*4;
		}

		int TCol_max = (TC-1)*c_stride + ksize;
		TR = MIN(MAX_Trc/TC, oh);
		TR = MIN(((MAX_IB_HW/TCol_max-ksize)/c_stride+1), TR);
		if(c_mode==2){
			assert(TR>=2);
			TR = (TR/2)*2;
		}else if(c_mode==3){
			assert(TR>=4);
			TR = (TR/4)*4;
		}
		TRow_max = (TR-1)/c_stride+1;

		assert((TRow_max>0)&&(TCol_max>0));
		assert((TR>0)&&(TC>0));
		assert((TR*TC)<=MAX_Trc);
		assert((TRow_max*TCol_max)<=MAX_IB_HW);

		TM = MIN(ofm_num, MAX_Tof);
		TN = MIN(ifm_num, MAX_Tif);

		uint32_t OHW = oh*ow;
		uint8_t KxK = ksize*ksize;
		uint32_t IHxIW = ih*iw;
		uint16_t p_stridexIR = p_stride*ih;
		uint16_t p_stridexIC = p_stride*iw;

		uint16_t NToy = ceil(oh*1.0f/TR);
		uint16_t NTox = ceil(ow*1.0f/TC);
		uint16_t NTof = ceil(ofm_num*1.0f/TM);
		uint64_t NTcomb = NToy*NTox*NTof;
		uint16_t NTif = ceil(ifm_num*1.0f/TN);

		uint8_t lmode;
		uint64_t NTcomb_l;
		if(NTif==1){
			lmode = 0;
			NTcomb_l = NTcomb+2;
		}else{
			lmode = 1;
			NTcomb_l = NTcomb+1;
		}

		ifm_ptr = fm_mem + IFM_offset[lnum];
		ofm_ptr = fm_mem + OFM_offset[lnum];

		int interQ = interQarray[lnum];
		int ifm_sqQ = ifm_sqQarray[lnum];
		printf("TR,TC,TM,TN, interQ=%d,%d,%d,%d\n", TR, TC, TM, TN, interQ);

		uint8_t wQ = interQ/2 + (interQ & 0x1);
		w_sr(kernel_w_ptr, (ksize*ksize*ifm_num*ofm_num + w_aoffset[lnum]), wQ);

		if(wb_mode!=0){
			uint16_t ifm_num_dx = ((ifm_num+LANE_NUM-1)/LANE_NUM);
			for(uint16_t x=0; x<ifm_num_dx; x++)
			for(int y=0; y<LANE_NUM; y++){
				ifm_Scale_ptr[x*LANE_NUM+y] = ifm_Scale_ptr[x*LANE_NUM+y]*pow(0.5, 14);
			}
		}

		uint8_t ifmQ_c = 14 - ifm_sqQ;
		uint8_t ComQ = 14 + ifmQ_c - interQ;

		uint16_t ofm_num_dx = ((ofm_num+LANE_NUM-1)/LANE_NUM);
		for(uint16_t x=0; x<ofm_num_dx; x++)
		for(int y=0; y<LANE_NUM; y++){
			ofm_Scale_ptr[x*LANE_NUM+y] = pow(2.0, 14)/ofm_Scale_ptr[x*LANE_NUM+y];
			kernel_Scale_ptr[x*LANE_NUM+y] = kernel_Scale_ptr[x*LANE_NUM+y]*pow(0.5, (int)ComQ);
		}

		IC_codec(ofm_ptr, ifm_ptr, kernel_w_ptr, bias_ptr, ifm_Scale_ptr, ofm_Scale_ptr, kernel_Scale_ptr,
					ifm_num, ih, iw, ofm_num, oh, ow, pad, ksize, kstride, ltype,
					p_stride, c_stride, wb_mode, ps_sf_bit, ps_mask, sq_en, TR, TC, TM, TN, OHW, KxK,
                 	IHxIW, p_stridexIR, p_stridexIC, bias_en, NToy, NTox, NTof, NTcomb, NTif, lmode, NTcomb_l, ifm_sqQ, interQ);

		ifm_Scale_ptr = ifm_Scale_ptr + LANE_ext(IF_NUM_set[lnum]);
		ofm_Scale_ptr = ofm_Scale_ptr + LANE_ext(OF_NUM_set[lnum]);
		kernel_Scale_ptr = kernel_Scale_ptr + LANE_ext(OF_NUM_set[lnum]);

		kernel_w_ptr = kernel_w_ptr + ksize*ksize*ifm_num*ofm_num + w_aoffset[lnum];
		if(bias_en)
			bias_ptr = bias_ptr + ofm_num + bias_aoffset[lnum];

		time2 = what_time_is_it_now();
		printf("%s: ic_h_w= [%d, %d, %d], oc_h_w= [%d, %d, %d], w_aoffset=%d, bias_aoffset=%d, lat= %3.5lf\n", LN_s[lnum],
		ifm_num, ih, iw, ofm_num, oh, ow, w_aoffset[lnum], bias_aoffset[lnum], time2 - time1);
	}

	// ofm_Scale_ptr = ofm_Scale_ptr - LANE_ext(OF_NUM_set[LNUM-1]);

	data_num = OH_set[LNUM-1]*OW_set[LNUM-1]*OF_NUM_set[LNUM-1];
	float *ofm_tmp_buf = (float *)malloc(sizeof(float)*data_num);

	int ofm_w = OW_set[LNUM-1];
	int ofm_h = OH_set[LNUM-1];
	int ofm_c = OF_NUM_set[LNUM-1];

	float tmp_pow_out = pow(0.5, 14);
	// printf("here?1-2, w_h_c = [%d, %d, %d]\n", ofm_w, ofm_h, ofm_c);
	for(int oc=0; oc<ofm_c;oc++)
	for(int oh=0; oh<ofm_h;oh++)
	for(int ow=0; ow<ofm_w;ow++){
		ofm_tmp_buf[oc*ofm_h*ofm_w + oh*ofm_w + ow] = ofm_ptr[((oc/LANE_NUM)*ofm_h*ofm_w + oh*ofm_w + ow)*LANE_NUM + (oc & (LANE_NUM-1))]
							*tmp_pow_out*ofm_scale_last_buf[(oc/LANE_NUM)*LANE_NUM + (oc & (LANE_NUM-1))];
	}
	// printf("here?2\n");
	time1 = what_time_is_it_now();
	save_image_png_chw(ofm_tmp_buf, OH_set[LNUM-1], OW_set[LNUM-1], 3, "recon_c");
	time2 = what_time_is_it_now();
	printf("save_png(s): %3.5lf\n", time2 - time1);

	printf("syn_output: %s, h_w_c= [%d, %d, %d]\n", "recon_c.png", OH_set[LNUM-1], OW_set[LNUM-1], OF_NUM_set[LNUM-1]);

	fp = fopen("syn_i16c_ifm_scale_cb.bin", "wb");
	fwrite(ifm_Scale, sizeof(float), ifm_channel_num, fp);
	fclose(fp);

	fp = fopen("syn_i16c_ofm_scale_cb.bin", "wb");
	fwrite(ofm_Scale, sizeof(float), ofm_channel_num, fp);
	fclose(fp);

	fp = fopen("syn_i16c_kernel_scale_cb.bin", "wb");
	fwrite(kernel_Scale, sizeof(float), ofm_channel_num, fp);
	fclose(fp);

	data_num = get_file_size("syn_kernel_w_T_i16rc.bin");
	fp = fopen("syn_kernel_w_T_i16rc_srcb.bin", "wb");
	fwrite(kernel_w, sizeof(int16_t), data_num/sizeof(int16_t), fp);
	fclose(fp);

	free(fm_mem);
	free(ofm_tmp_buf);
	free(ofm_scale_last_buf);

	free(kernel_w);
	free(bias);

    return 0;
}
