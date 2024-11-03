
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "yolov2_reorg.h"

unsigned long get_file_size(const char *filename)
{
    struct stat buf;
    if(stat(filename, &buf)<0)
    {
        return 0;
    }
	printf("%s's data size is %ld, 0x%x\n", filename, buf.st_size, buf.st_size);
    return (unsigned long)buf.st_size;
}

int LANE_ext(int tmp_in){
	int val = (((tmp_in + LANE_NUM -1)/LANE_NUM)*LANE_NUM);
	return val;
}

int LANE_div(int tmp_in){
	int val = ((tmp_in + LANE_NUM -1)/LANE_NUM);
	return val;
}

int syn_transform(int *ana_o_dim, char *ana_out_bname){

	const uint32_t F_num = 128;
	const uint32_t LNUM = 5;

	const int IF_NUM_set[LNUM] = {F_num, F_num, F_num, F_num, F_num};
	const int OF_NUM_set[LNUM] = {F_num, F_num, F_num, F_num,     3};
	const int KS_set[LNUM] = {5, 1, 5, 1, 9};
	const int P_set[LNUM]  = {2, 0, 2, 0, 4};
	const int S_set[LNUM]  = {2, 1, 2, 1, 4};
	const bool bias_en_set[LNUM] = {true, true, true, true, true};
	const int LT_set[LNUM] = {LT_CONVT, LT_IGDN, LT_CONVT, LT_IGDN, LT_CONVT};
	const char *LN_s[LNUM] = {"syn_0", "igdn_0", "syn_1", "igdn_1", "syn_2"};

	int32_t IW_set[LNUM], IH_set[LNUM], OW_set[LNUM], OH_set[LNUM];
	int IFM_offset[LNUM], OFM_offset[LNUM];

	FILE *fp;
	int data_num;
	int in_dim[3];//hwc

	in_dim[0] = ana_o_dim[0];//0-2, hwc
	in_dim[1] = ana_o_dim[1];
	in_dim[2] = ana_o_dim[2];
	bool IsDC2 = ana_o_dim[3];
	uint16_t max_RTof = ana_o_dim[4];

	data_num = get_file_size("ana_IHW_OHW_set.bin");
	int32_t *ana_IHW_OHW_set = (int32_t *)malloc(data_num);
	read_binfile_flt32_rb((float *)ana_IHW_OHW_set, "ana_IHW_OHW_set.bin", data_num/4);
	for(int lnum = 0; lnum < LNUM; lnum++){
		IH_set[lnum] = ana_IHW_OHW_set[LNUM*2 + LNUM-lnum-1];
		IW_set[lnum] = ana_IHW_OHW_set[LNUM*3 + LNUM-lnum-1];
		OH_set[lnum] = ana_IHW_OHW_set[       + LNUM-lnum-1];
		OW_set[lnum] = ana_IHW_OHW_set[LNUM   + LNUM-lnum-1];
	}
	free(ana_IHW_OHW_set);

	uint64_t fm_max_size = 0;
	int ifm_channel_num = 0, ofm_channel_num = 0;
	for(int lnum = 0; lnum < LNUM; lnum++){
//calc ifm/ofm width/height
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
	}
	printf("fm_max_size=0x%x\n", fm_max_size*sizeof(float));

	for(int lnum = 0; lnum < LNUM; lnum++){
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

	copy_file2mem("syn_kernel_w_T_i16rc_srcb.bin", get_file_size("syn_kernel_w_T_i16rc_srcb.bin"), WEIGHT_BASE);
	copy_file2mem("syn_bias_f32c.bin", get_file_size("syn_bias_f32c.bin"), BETA_BASE);
	copy_file2mem("syn_i16c_ifm_scale_cb.bin", get_file_size("syn_i16c_ifm_scale_cb.bin"), IFM_SCALE_BASE);
	copy_file2mem("syn_i16c_kernel_scale_cb.bin", get_file_size("syn_i16c_kernel_scale_cb.bin"), K_SCALE_BASE);
	copy_file2mem("syn_i16c_ofm_scale_cb.bin", get_file_size("syn_i16c_ofm_scale_cb.bin"), OFM_SCALE_BASE);

	int w_aoffset[LNUM];
	read_binfile_flt32_rb((float*)w_aoffset, "syn_kernel_w_T_i16rc_oadd.bin", LNUM);
	int bias_aoffset[LNUM];
	read_binfile_flt32_rb((float*)bias_aoffset, "syn_bias_f32c_oadd.bin", LNUM);

	float *ifm_Scale = (float *)calloc(sizeof(float), ifm_channel_num);
	read_binfile_flt32_rb((float*)ifm_Scale, "syn_i16c_ifm_scale.bin", ifm_channel_num);
	float *ofm_Scale = (float *)calloc(sizeof(float), ofm_channel_num);
	read_binfile_flt32_rb((float*)ofm_Scale, "syn_i16c_ofm_scale.bin", ofm_channel_num);
	int ifm_sqQarray[LNUM];
	read_binfile_flt32_rb((float*)ifm_sqQarray, "syn_i16c_ifm_sqQ.bin", LNUM);
	int interQarray[LNUM];
	read_binfile_flt32_rb((float*)interQarray, "syn_i16c_interQ.bin", LNUM);

	data_num = in_dim[0]*in_dim[1]*in_dim[2];
	float *ifm_mem = (float *)malloc(sizeof(float)*data_num);

	fp = fopen(ana_out_bname, "rb");
	fread(ifm_mem, sizeof(float), data_num, fp);//NHWC
	fclose(fp);
	printf("syn_input: %s, h_w_c= [%d, %d, %d]\n", ana_out_bname, in_dim[0], in_dim[1], in_dim[2]);

	int img_c = in_dim[2], img_h = in_dim[0], img_w = in_dim[1];
	int Ca3_d4 =  LANE_div(img_c);
	data_num = Ca3_d4*img_h*img_w*LANE_NUM;
	int16_t *ifm_buf = (int16_t *)malloc(sizeof(int16_t)*data_num);
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
			ifm_buf[idx_out] = (ifm_mem[idx_in]/ifm_Scale[c4*LANE_NUM+ic])*pow(2.0, 14);
	}
	copy_mem2dev((uint8_t *)ifm_buf, data_num*sizeof(int16_t), MEM_BASE + IFM_offset[0]*2);

	free(ifm_buf);
	free(ifm_mem);

	float *ofm_scale_last_buf = (float *)malloc(LANE_ext(OF_NUM_set[LNUM-1])*sizeof(float));
	int ofm_scale_last_offset = 0;
	for(int lnum = 0; lnum < LNUM; lnum++){
		int ofm_num = OF_NUM_set[lnum];
		if(lnum==(LNUM-1)){
			memcpy(ofm_scale_last_buf, ofm_Scale + ofm_scale_last_offset, LANE_ext(ofm_num)*sizeof(float));
		}
		ofm_scale_last_offset += LANE_ext(ofm_num);
	}

	unsigned int ifm_ptr, ofm_ptr;
	unsigned int kernel_w_ptr = 0;
	unsigned int bias_ptr = 0;
	unsigned int ifm_Scale_ptr = 0;
	unsigned int ofm_Scale_ptr = 0;
	unsigned int kernel_Scale_ptr = 0;

	double time1, time2;
	time1 = what_time_is_it_now();
	for(int lnum = 0; lnum < LNUM; lnum++){
		double time1_t, time2_t;
		time1_t = what_time_is_it_now();

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
		uint8_t IsFMreod = 0;

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
		uint8_t K_ifm = 0;
		if(ksize==5&&c_stride==1){//K==5&cs==1 CONVT:2
			c_mode = 2;
			K_ifm = 3;
		}else if(ksize==9&&c_stride==1){//K==9&cs==1 CONVT:3
			c_mode = 3;
			K_ifm = 3;
		}else{//normal:0
			c_mode = 0;
			K_ifm = ksize;
		}

		uint16_t MAX_RTof = 1;
		bool IsRTof = false;
		if(IsRTof){
			MAX_RTof = max_RTof;
			if(MAX_RTof > (MAX_KxK/(ksize*ksize)))
				MAX_RTof = (MAX_KxK/(ksize*ksize));
		}

		uint16_t TR, TC, TM, TN;
		uint16_t TR_MIN_d_Poy, TC_MIN_d_Pox;
		uint16_t Poy_LEN, Pox_LEN;

		int factor = 1;
		if(c_mode==2){
			factor = 2;
		}else if(c_mode==3){
			factor = 4;
		}else{
			factor = 1;
		}
		int TR_min = factor*MAX_Poy;//for Poy unroll
		int TC_min = factor*MAX_Pox;//for Pox unroll

		int TRow_max = (factor-1)*c_stride + K_ifm;
		int max_ib_hw_left = MAX_IB_HW/TRow_max;
		assert(max_ib_hw_left>=K_ifm);

		TC = MIN(((max_ib_hw_left-K_ifm)/c_stride+1)*MAX_Pox, ow);//at least left K rows for calc
		if(IsRTof)
			TC = MIN(MAX_Trc/MAX_RTof/TR_min, TC);
		else
			TC = MIN(MAX_Trc/TR_min, TC);
		assert(TC>=TC_min);
		TC = TC/TC_min*TC_min;

		TC_MIN_d_Pox = TC/MAX_Pox;
		Pox_LEN = (TC_MIN_d_Pox-1)*c_stride+K_ifm;

		if(IsRTof)
			TR = MIN(MAX_Trc/MAX_RTof/TC, oh);
		else
			TR = MIN(MAX_Trc/TC, oh);
		TR = MIN(((MAX_IB_HW/Pox_LEN-K_ifm)/c_stride+1)*MAX_Poy, TR);
		assert(TR>=TR_min);
		TR = (TR/TR_min)*TR_min;

		TR_MIN_d_Poy = TR/MAX_Poy;
		Poy_LEN = (TR_MIN_d_Poy-1)*c_stride+K_ifm;

		assert((MAX_Poy>0)&&(MAX_Pox>0));
		assert((TR>=MAX_Poy)&&(TC>=MAX_Pox));//for ifm_buf
		assert((Pox_LEN*Poy_LEN)<=MAX_IB_HW);//for ifm_buf
		if(IsRTof)
			assert((TR*TC*MAX_RTof)<=MAX_Trc);
		else
			assert((TR*TC)<=MAX_Trc);

		int MaxTof_t;
		if(IsRTof)
			MaxTof_t = MAX_Tof*MAX_RTof;
		else
			MaxTof_t = MAX_Tof;
		TM = MIN(ofm_num, MaxTof_t);
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

		ifm_ptr = MEM_BASE + IFM_offset[lnum]*2;
		ofm_ptr = MEM_BASE + OFM_offset[lnum]*2;

		int interQ = interQarray[lnum];
		int ifm_sqQ = ifm_sqQarray[lnum];

		IC_codec(ofm_ptr, ifm_ptr, kernel_w_ptr, bias_ptr,
					ifm_Scale_ptr,  ofm_Scale_ptr,  kernel_Scale_ptr,
					ifm_num, ih, iw, ofm_num, oh, ow, pad, ksize, kstride, ltype,
					p_stride, c_stride, wb_mode, ps_sf_bit, ps_mask, sq_en, TR, TC, TM, TN, OHW, KxK,
                 	IHxIW, p_stridexIR, p_stridexIC, bias_en,
					NToy, NTox, NTof, NTcomb, NTif, lmode, NTcomb_l, ifm_sqQ, interQ, IsFMreod);
		time2_t = what_time_is_it_now();
		printf("%s: ic_h_w= [%d, %d, %d], oc_h_w= [%d, %d, %d], lat= %3.5lf\n", LN_s[lnum],
		ifm_num, ih, iw, ofm_num, oh, ow, time2_t - time1_t);

		ifm_Scale_ptr = ifm_Scale_ptr + LANE_ext(IF_NUM_set[lnum]);
		ofm_Scale_ptr = ofm_Scale_ptr + LANE_ext(OF_NUM_set[lnum]);
		kernel_Scale_ptr = kernel_Scale_ptr + LANE_ext(OF_NUM_set[lnum]);
		kernel_w_ptr = kernel_w_ptr + ksize*ksize*ifm_num*ofm_num + w_aoffset[lnum];
		if(bias_en)
			bias_ptr = bias_ptr + ofm_num + bias_aoffset[lnum];

	}
	time2 = what_time_is_it_now();
	printf("\n\nsyn lat_sum= %3.5lf\n\n", time2 - time1);

	data_num = LANE_ext(OF_NUM_set[LNUM-1])*OH_set[LNUM-1]*OW_set[LNUM-1];
	int16_t *syn_out_i16 = (int16_t *)malloc(sizeof(int16_t)*data_num);
	copy_dev2mem((uint8_t *)syn_out_i16, data_num*sizeof(int16_t), ofm_ptr);

	data_num = OH_set[LNUM-1]*OW_set[LNUM-1]*OF_NUM_set[LNUM-1];
	float *ofm_tmp_buf = (float *)malloc(sizeof(float)*data_num);
	int ofm_w = OW_set[LNUM-1];
	int ofm_h = OH_set[LNUM-1];
	int ofm_c = OF_NUM_set[LNUM-1];
	float tmp_pow_out = pow(0.5, 14);

	for(int oc=0; oc<ofm_c;oc++)
	for(int oh=0; oh<ofm_h;oh++)
	for(int ow=0; ow<ofm_w;ow++){
		ofm_tmp_buf[oc*ofm_h*ofm_w + oh*ofm_w + ow] =
				syn_out_i16[((oc/LANE_NUM)*ofm_h*ofm_w + oh*ofm_w + ow)*LANE_NUM + (oc & (LANE_NUM-1))]*
				tmp_pow_out*ofm_scale_last_buf[oc];
	}

	time1 = what_time_is_it_now();
	save_image_png_chw(ofm_tmp_buf, OH_set[LNUM-1], OW_set[LNUM-1], 3, "recon_c");
	time2 = what_time_is_it_now();
	printf("save_png(s): %3.5lf\n", time2 - time1);

	printf("syn_output: %s, h_w_c= [%d, %d, %d]\n", "recon_c.png", OH_set[LNUM-1], OW_set[LNUM-1], OF_NUM_set[LNUM-1]);

	free(ofm_tmp_buf);
	free(syn_out_i16);
	free(ofm_scale_last_buf);

	return 0;
}

int ana_transform(int argc, char *argv[], int *ana_o_dim, char *ana_out_bname){

	const uint32_t F_num = 128;
	const uint32_t LNUM = 5;

	const int IF_NUM_set[LNUM] = {    3, F_num, F_num, F_num, F_num};
	const int OF_NUM_set[LNUM] = {F_num, F_num, F_num, F_num, F_num};
	const int KS_set[LNUM] = {9, 1, 5, 1, 5};
	const int P_set[LNUM]  = {4, 0, 2, 0, 2};
	const int S_set[LNUM]  = {4, 1, 2, 1, 2};
	const bool bias_en_set[LNUM] = {true, true, true, true, false};
	const int LT_set[LNUM] = {LT_CONV, LT_GDN, LT_CONV, LT_GDN, LT_CONV};
	const char *LN_s[LNUM] = {"ana_0", "gdn_0", "ana_1", "gdn_1", "ana_2"};

	int32_t IW_set[LNUM], IH_set[LNUM], OW_set[LNUM], OH_set[LNUM];
	int IFM_offset[LNUM], OFM_offset[LNUM];

	FILE *fp;
	int data_num;
	char img_buff[256];
	char *input_imgfn = img_buff;
	if(argc==1)
		strncpy(input_imgfn, "abigail-keenan-27293.png", 256);
	else
		strncpy(input_imgfn, argv[1], 256);
	image im = load_image_stb(input_imgfn, 3);//3 channel img
	printf("Input img:%s\n w=%d,h=%d,c=%d\n", input_imgfn, im.w, im.h, im.c);

	uint8_t FMReod = 0;
	if(argc>=3){
		FMReod = atoi(argv[2]);
	}
	bool IsDC  = FMReod & 0x1;
	bool IsDC2 = FMReod & 0x2;

	uint16_t max_RTof = 1;
	if(argc>=4){
		max_RTof = atoi(argv[3]);
	}

	unsigned int fm_max_size = 0;
	int ifm_channel_num = 0, ofm_channel_num = 0;
	for(int lnum = 0; lnum < LNUM; lnum++){
//calc sum of bias/weight total num
		ifm_channel_num = ifm_channel_num + LANE_ext(IF_NUM_set[lnum]);
		ofm_channel_num = ofm_channel_num + LANE_ext(OF_NUM_set[lnum]);
//calc ifm/ofm width/height
		if(lnum==0){
			IW_set[lnum] = im.w;
			IH_set[lnum] = im.h;
		}else{
			IW_set[lnum] = OW_set[lnum-1];
			IH_set[lnum] = OH_set[lnum-1];
		}
		OW_set[lnum] = (IW_set[lnum] - KS_set[lnum] + 2*P_set[lnum])/S_set[lnum] + 1;
		OH_set[lnum] = (IH_set[lnum] - KS_set[lnum] + 2*P_set[lnum])/S_set[lnum] + 1;

		int ifm_offset = IW_set[lnum]*IH_set[lnum]*LANE_ext(IF_NUM_set[lnum]);
		int ofm_offset = OW_set[lnum]*OH_set[lnum]*LANE_ext(OF_NUM_set[lnum]);
		if(lnum==0){
			fm_max_size = ifm_offset + ofm_offset;
		}else{
			int tmp_fm_size = fm_max_size - ifm_offset;
			if(tmp_fm_size < ofm_offset)
				fm_max_size = ifm_offset + ofm_offset;
		}
	}

	for(int lnum = 0; lnum < LNUM; lnum++){
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

	printf("fm_max_size=0x%x\n", fm_max_size*sizeof(int16_t));

	int w_aoffset[LNUM];
	read_binfile_flt32_rb((float*)w_aoffset, "ana_kernel_w_i16rc_oadd.bin", LNUM);
	int bias_aoffset[LNUM];
	read_binfile_flt32_rb((float*)bias_aoffset, "ana_bias_f32c_oadd.bin", LNUM);

	copy_file2mem("ana_kernel_w_i16rc_srcb.bin", get_file_size("ana_kernel_w_i16rc_srcb.bin"), WEIGHT_BASE);
	copy_file2mem("ana_bias_f32c.bin", get_file_size("ana_bias_f32c.bin"), BETA_BASE);
	copy_file2mem("ana_i16c_ifm_scale_cb.bin", get_file_size("ana_i16c_ifm_scale_cb.bin"), IFM_SCALE_BASE);
	copy_file2mem("ana_i16c_kernel_scale_cb.bin", get_file_size("ana_i16c_kernel_scale_cb.bin"), K_SCALE_BASE);
	copy_file2mem("ana_i16c_ofm_scale_cb.bin", get_file_size("ana_i16c_ofm_scale_cb.bin"), OFM_SCALE_BASE);

	float *ifm_Scale = (float *)calloc(sizeof(float), ifm_channel_num);
	read_binfile_flt32_rb((float*)ifm_Scale, "ana_i16c_ifm_scale.bin", ifm_channel_num);
	float *ofm_Scale = (float *)calloc(sizeof(float), ofm_channel_num);
	read_binfile_flt32_rb((float*)ofm_Scale, "ana_i16c_ofm_scale.bin", ofm_channel_num);

	int ifm_sqQarray[LNUM];
	read_binfile_flt32_rb((float*)ifm_sqQarray, "ana_i16c_ifm_sqQ.bin", LNUM);
	int interQarray[LNUM];
	read_binfile_flt32_rb((float*)interQarray, "ana_i16c_interQ.bin", LNUM);

	float *img_in_chw = im.data;
	int img_c = im.c, img_h = im.h, img_w = im.w;
	int Ca3_d4 =  LANE_div(img_c);
	float tmp_pow_ifm = pow(2.0, 14);
	data_num = Ca3_d4*img_h*img_w*LANE_NUM;
	int16_t *ifm_buf = (int16_t *)malloc(sizeof(int16_t)*data_num);
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
			ifm_buf[idx_out] = (img_in_chw[idx_in]/ifm_Scale[c4*LANE_NUM+ic])*tmp_pow_ifm;
	}

	copy_mem2dev((uint8_t *)ifm_buf, data_num*sizeof(int16_t), MEM_BASE + IFM_offset[0]*2);

	free_image(im);
	free(ifm_buf);

	float *ofm_scale_last_buf = (float *)malloc(LANE_ext(OF_NUM_set[LNUM-1])*sizeof(float));

	int ofm_scale_last_offset = 0;
	for(int lnum = 0; lnum < LNUM; lnum++){
		int ofm_num = OF_NUM_set[lnum];
		if(lnum==(LNUM-1)){
			memcpy(ofm_scale_last_buf, ofm_Scale + ofm_scale_last_offset, LANE_ext(ofm_num)*sizeof(float));
		}
		ofm_scale_last_offset += LANE_ext(ofm_num);
	}

	unsigned int ifm_ptr, ofm_ptr;
	unsigned int kernel_w_ptr = 0;
	unsigned int bias_ptr = 0;
	unsigned int ifm_Scale_ptr = 0;
	unsigned int ofm_Scale_ptr = 0;
	unsigned int kernel_Scale_ptr = 0;

	double time1, time2;
	time1 = what_time_is_it_now();
	for(int lnum = 0; lnum < LNUM; lnum++){
		double time1_t, time2_t;
		time1_t = what_time_is_it_now();

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
		uint8_t IsFMreod = 0;
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
		uint8_t K_ifm = 0;
		if(ksize==5&&c_stride==1){//K==5&cs==1 CONVT:2
			c_mode = 2;
			K_ifm = 3;
		}else if(ksize==9&&c_stride==1){//K==9&cs==1 CONVT:3
			c_mode = 3;
			K_ifm = 3;
		}else{//normal:0
			c_mode = 0;
			K_ifm = ksize;
		}

		uint16_t MAX_RTof = 1;
		bool IsRTof = false;
		if(IsRTof){
			MAX_RTof = max_RTof;
			if(MAX_RTof > (MAX_KxK/(ksize*ksize)))
				MAX_RTof = (MAX_KxK/(ksize*ksize));
		}

		uint16_t TR, TC, TM, TN;
		uint16_t TR_MIN_d_Poy, TC_MIN_d_Pox;
		uint16_t Poy_LEN, Pox_LEN;

		int factor = 1;
		if(c_mode==2){
			factor = 2;
		}else if(c_mode==3){
			factor = 4;
		}else{
			factor = 1;
		}
		int TR_min = factor*MAX_Poy;//for Poy unroll
		int TC_min = factor*MAX_Pox;//for Pox unroll

		int TRow_max = (factor-1)*c_stride + K_ifm;
		int max_ib_hw_left = MAX_IB_HW/TRow_max;
		assert(max_ib_hw_left>=K_ifm);

		TC = MIN(((max_ib_hw_left-K_ifm)/c_stride+1)*MAX_Pox, ow);//at least left K rows for calc
		if(IsRTof)
			TC = MIN(MAX_Trc/MAX_RTof/TR_min, TC);
		else
			TC = MIN(MAX_Trc/TR_min, TC);
		assert(TC>=TC_min);
		TC = TC/TC_min*TC_min;

		TC_MIN_d_Pox = TC/MAX_Pox;
		Pox_LEN = (TC_MIN_d_Pox-1)*c_stride+K_ifm;

		if(IsRTof)
			TR = MIN(MAX_Trc/MAX_RTof/TC, oh);
		else
			TR = MIN(MAX_Trc/TC, oh);
		TR = MIN(((MAX_IB_HW/Pox_LEN-K_ifm)/c_stride+1)*MAX_Poy, TR);
		assert(TR>=TR_min);
		TR = (TR/TR_min)*TR_min;

		TR_MIN_d_Poy = TR/MAX_Poy;
		Poy_LEN = (TR_MIN_d_Poy-1)*c_stride+K_ifm;

		assert((MAX_Poy>0)&&(MAX_Pox>0));
		assert((TR>=MAX_Poy)&&(TC>=MAX_Pox));//for ifm_buf
		assert((Pox_LEN*Poy_LEN)<=MAX_IB_HW);//for ifm_buf
		if(IsRTof)
			assert((TR*TC*MAX_RTof)<=MAX_Trc);
		else
			assert((TR*TC)<=MAX_Trc);

		int MaxTof_t;
		if(IsRTof)
			MaxTof_t = MAX_Tof*MAX_RTof;
		else
			MaxTof_t = MAX_Tof;
		TM = MIN(ofm_num, MaxTof_t);
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

		ifm_ptr = MEM_BASE + IFM_offset[lnum]*2;
		ofm_ptr = MEM_BASE + OFM_offset[lnum]*2;

		int interQ = interQarray[lnum];
		int ifm_sqQ = ifm_sqQarray[lnum];

		IC_codec(ofm_ptr, ifm_ptr, kernel_w_ptr, bias_ptr,
					ifm_Scale_ptr,  ofm_Scale_ptr,  kernel_Scale_ptr,
					ifm_num, ih, iw, ofm_num, oh, ow, pad, ksize, kstride, ltype,
					p_stride, c_stride, wb_mode, ps_sf_bit, ps_mask, sq_en, TR, TC, TM, TN, OHW, KxK,
                 	IHxIW, p_stridexIR, p_stridexIC, bias_en,
					NToy, NTox, NTof, NTcomb, NTif, lmode, NTcomb_l, ifm_sqQ, interQ, IsFMreod);

		ifm_Scale_ptr = ifm_Scale_ptr + LANE_ext(IF_NUM_set[lnum]);
		ofm_Scale_ptr = ofm_Scale_ptr + LANE_ext(OF_NUM_set[lnum]);
		kernel_Scale_ptr = kernel_Scale_ptr + LANE_ext(OF_NUM_set[lnum]);

		kernel_w_ptr = kernel_w_ptr + ksize*ksize*ifm_num*ofm_num + w_aoffset[lnum];
		if(bias_en)
			bias_ptr = bias_ptr + ofm_num + bias_aoffset[lnum];

		time2_t = what_time_is_it_now();
		printf("%s: ic_h_w= [%d, %d, %d], oc_h_w= [%d, %d, %d], lat= %3.5lf\n", LN_s[lnum],
		ifm_num, ih, iw, ofm_num, oh, ow, time2_t - time1_t);
	}
	time2 = what_time_is_it_now();
	printf("\nana lat_sum= %3.5lf\n", time2 - time1);

//save transformed img
	float tmp_pow_out = pow(0.5, 14);
	data_num = OH_set[LNUM-1]*OW_set[LNUM-1]*LANE_ext(F_num);
	int32_t *ana_out = (int32_t *)malloc(sizeof(float)*data_num);
	float *ana_out_f32 = (float *)malloc(sizeof(float)*data_num);
	copy_dev2mem((uint8_t *)ana_out, data_num*sizeof(int16_t), ofm_ptr);

	int16_t *tmp_i16_ptr = (int16_t *)ana_out;
	int ofm_w = OW_set[LNUM-1];
	int ofm_h = OH_set[LNUM-1];
	int ofm_c = OF_NUM_set[LNUM-1];
	for(int oc=0; oc<ofm_c;oc++)
	for(int oh=0; oh<ofm_h;oh++)
	for(int ow=0; ow<ofm_w;ow++){
		ana_out_f32[oc*ofm_h*ofm_w + oh*ofm_w + ow] =
				tmp_i16_ptr[((oc/LANE_NUM)*ofm_h*ofm_w + oh*ofm_w + ow)*LANE_NUM + (oc & (LANE_NUM-1))]
			*tmp_pow_out*ofm_scale_last_buf[oc];
	}

	data_num = OH_set[LNUM-1]*OW_set[LNUM-1]*F_num;
	printf("ana_output: %s, h_w_c= [%d, %d, %d]\n", ana_out_bname, OH_set[LNUM-1], OW_set[LNUM-1], F_num);
	fp = fopen(ana_out_bname, "wb");
	fwrite(ana_out_f32, sizeof(float), data_num, fp);
	fclose(fp);

	fp = fopen("ana_IHW_OHW_set.bin", "wb");
	fwrite(IH_set, sizeof(int32_t), LNUM, fp);
	fwrite(IW_set, sizeof(int32_t), LNUM, fp);
	fwrite(OH_set, sizeof(int32_t), LNUM, fp);
	fwrite(OW_set, sizeof(int32_t), LNUM, fp);
	fclose(fp);

	free(ana_out_f32);
	free(ana_out);

	ana_o_dim[0] = OH_set[LNUM-1];
	ana_o_dim[1] = OW_set[LNUM-1];
	ana_o_dim[2] = F_num;
	ana_o_dim[3] = IsDC2;
	ana_o_dim[4] = max_RTof;

	return 0;
}

int main( int argc, char *argv[])
{
	int ana_o_dim[5];
	char *ana_out_bname = "ana_q_out_chw.bin";

	ana_transform( argc, argv, ana_o_dim, ana_out_bname);

	syn_transform( ana_o_dim, ana_out_bname);

    return 0;
}



//
//#include <stdio.h>
//#include <stdlib.h>
//#include <iostream>
//#include <math.h>
//#include <fcntl.h>
//#include <string.h>
//#include <time.h>
//#include <sys/time.h>
//#include "yolov2_reorg.h"
//
////int read_binfile_flt32_rb(float *buf, char *filename, int data_num)
////{
////	FILE *fp;
////	if( (fp = fopen(filename, "rb")) == NULL)
////		printf("cannot open bin_file %s\n", filename);
////	int rd_num  = fread(buf, sizeof(unsigned char)*4, data_num, fp);
////	fclose(fp);
////
////	return rd_num;
////}
//
//unsigned long get_file_size(const char *filename)
//{
//    struct stat buf;
//    if(stat(filename, &buf)<0)
//    {
//        return 0;
//    }
//	printf("%s's data size is %ld\n", filename, buf.st_size);
//    return (unsigned long)buf.st_size;
//}
//
//int syn_transform(int ana_o_dim[3], char *ana_out_bname){
//
//	const uint32_t F_num = 128;
//	const uint32_t LNUM = 5;
//
//	const int IF_NUM_set[LNUM] = {F_num, F_num, F_num, F_num, F_num};
//	const int OF_NUM_set[LNUM] = {F_num, F_num, F_num, F_num,     3};
//	const int KS_set[LNUM] = {5, 1, 5, 1, 9};
//	const int P_set[LNUM]  = {2, 0, 2, 0, 4};
//	const int S_set[LNUM]  = {2, 1, 2, 1, 4};
//	const bool bias_en_set[LNUM] = {true, true, true, true, true};
//	const int LT_set[LNUM] = {LT_CONVT, LT_IGDN, LT_CONVT, LT_IGDN, LT_CONVT};
//	const char *LN_s[LNUM] = {"syn_0", "igdn_0", "syn_1", "igdn_1", "syn_2"};
//
//	int IW_set[LNUM], IH_set[LNUM], OW_set[LNUM], OH_set[LNUM];
//	int IFM_offset[LNUM], OFM_offset[LNUM];
//
//	FILE *fp;
//	int data_num;
////	char bfile_name[256];
//	int in_dim[3];//hwc
//
//	in_dim[0] = ana_o_dim[0];//0-2, hwc
//	in_dim[1] = ana_o_dim[1];
//	in_dim[2] = ana_o_dim[2];
//
//
//	int w_num = 0;
//	int bias_num = 0;
//	int pad_space_size = 0;
//	int fm_max_size = 0;
//	for(int lnum = 0; lnum < LNUM; lnum++){
////calc sum of bias/weight total num
//		if(bias_en_set[lnum])
//			bias_num = bias_num + OF_NUM_set[lnum];
//
//		w_num = w_num + IF_NUM_set[lnum]*OF_NUM_set[lnum]*KS_set[lnum]*KS_set[lnum];
//
////calc ifm/ofm width/height
//		if(lnum==0){
//			IW_set[lnum] = in_dim[1];
//			IH_set[lnum] = in_dim[0];
//		}else{
//			IW_set[lnum] = OW_set[lnum-1];
//			IH_set[lnum] = OH_set[lnum-1];
//		}
//
//		int ltype = LT_set[lnum];
//		if(ltype==LT_CONVT){
//			OW_set[lnum] = IW_set[lnum]*S_set[lnum] - KS_set[lnum] + 2*P_set[lnum] +1;
//			OH_set[lnum] = IH_set[lnum]*S_set[lnum] - KS_set[lnum] + 2*P_set[lnum] +1;
//		}else{
//			OW_set[lnum] = (IW_set[lnum] - KS_set[lnum] + 2*P_set[lnum])/S_set[lnum] + 1;
//			OH_set[lnum] = (IH_set[lnum] - KS_set[lnum] + 2*P_set[lnum])/S_set[lnum] + 1;
//		}
//
//		int ifm_offset = IW_set[lnum]*IH_set[lnum]*IF_NUM_set[lnum];
//		int ofm_offset = OW_set[lnum]*OH_set[lnum]*OF_NUM_set[lnum];
//		if(lnum==0){
//			fm_max_size = ifm_offset + ofm_offset;
//		}else{
//			int tmp_fm_size = fm_max_size - ifm_offset;
//			if(tmp_fm_size < ofm_offset)
//				fm_max_size = ifm_offset + ofm_offset;
//		}
//	}
//	printf("fm_max_size_calc = %d\n",fm_max_size);
//
//	for(int lnum = 0; lnum < LNUM; lnum++){
//		int ifm_offset = IW_set[lnum]*IH_set[lnum]*IF_NUM_set[lnum];
//		int ofm_offset = OW_set[lnum]*OH_set[lnum]*OF_NUM_set[lnum];
//		if(lnum==0){
//			IFM_offset[lnum] = 0;
//			OFM_offset[lnum] = fm_max_size - ofm_offset;
//		}else{
//			IFM_offset[lnum] = OFM_offset[lnum-1];
//			if(IFM_offset[lnum]!=0){
//				OFM_offset[lnum] = 0;
//			}else{
//				OFM_offset[lnum] = fm_max_size - ofm_offset;
//			}
//		}
//	}
//
//	copy_file2mem("syn_kernel_w_T_reorg.bin", get_file_size("syn_kernel_w_T_reorg.bin"), WEIGHT_BASE);
//	copy_file2mem("syn_bias.bin", get_file_size("syn_bias.bin"), BETA_BASE);
//
////	// data_num = 5*5*128*128 + 1*1*128*128 + 5*5*128*128 + 1*1*128*128 + 9*9*128*3;
////	fp = fopen("syn_kernel_w_T_reorg.bin", "rb");
////	float *kernel_w = (float *)malloc(sizeof(float)*w_num);
////	fread(kernel_w, sizeof(float), w_num, fp);
////	fclose(fp);
////	// data_num = 128 + 128 + 128 + 128 + 3;
////	fp = fopen("syn_bias.bin", "rb");
////	float *bias = (float *)malloc(sizeof(float)*bias_num);
////	fread(bias, sizeof(float), bias_num, fp);
////	fclose(fp);
//
////	float *fm_mem = (float *)malloc(sizeof(float)*fm_max_size);
//	// float *img_in_hwc_pad = (float *)malloc(sizeof(float)*pad_space_size);//NHWC
//
//	data_num = in_dim[0]*in_dim[1]*in_dim[2];
//	float *ifm_mem = (float *)malloc(sizeof(float)*data_num);
//
//	fp = fopen(ana_out_bname, "rb");
//	fread(ifm_mem, sizeof(float), data_num, fp);//NHWC
//	fclose(fp);
//	printf("syn_input: %s, h_w_c= [%d, %d, %d]\n", ana_out_bname, in_dim[0], in_dim[1], in_dim[2]);
//
//	i32tof32_1d(ifm_mem, (int32_t *)ifm_mem, data_num);
//
//	float *syn_in_median = (float *)malloc(sizeof(float)*in_dim[2]);
//	read_binfile_flt32_rb(syn_in_median, "syn_in_median", in_dim[2]);
//	syn_add_channel_median_chw(ifm_mem, syn_in_median, in_dim[0], in_dim[1], in_dim[2]);
//
//	copy_mem2dev((uint8_t *)ifm_mem, data_num*sizeof(float), MEM_BASE + IFM_offset[0]*4);
//
//	free(syn_in_median);
//	free(ifm_mem);
//
////	float *kernel_w_ptr = kernel_w;
////	float *bias_ptr = bias;
////	float *ifm_ptr, *ofm_ptr;
//	unsigned int ifm_ptr, ofm_ptr;
//	unsigned int kernel_w_ptr = 0;
//	unsigned int bias_ptr = 0;
//	double time1, time2;
//	for(int lnum = 0; lnum < LNUM; lnum++){
//		time1 = what_time_is_it_now();
//
//		int ltype = LT_set[lnum];
//		bool bias_en = bias_en_set[lnum];
//		int pad = P_set[lnum];
//		int ih = IH_set[lnum];
//		int iw = IW_set[lnum];
//		int oh = OH_set[lnum];
//		int ow = OW_set[lnum];
//		int ksize = KS_set[lnum];
//		int kstride = S_set[lnum];
//		int ifm_num = IF_NUM_set[lnum];
//		int ofm_num = OF_NUM_set[lnum];
//
//		// int h_pad, w_pad;
//		// if(ltype==LT_CONVT){
//		// 	h_pad = ih*kstride + pad*2;
//		// 	w_pad = iw*kstride + pad*2;
//		// }else{
//		// 	h_pad = ih + pad*2;
//		// 	w_pad = iw + pad*2;
//		// }
//
//		uint8_t p_stride, c_stride;
//		switch (ltype)
//		{
//			case LT_CONVT:
//				p_stride = kstride;
//				c_stride = 1;
//				break;
//			case LT_IGDN:
//				p_stride = kstride;
//				c_stride = kstride;
//				break;
//			default:
//				p_stride = 1;
//				c_stride = kstride;
//				break;
//		}
//
//		uint8_t wb_mode;
//		bool sq_en = 0;
//		if(ltype==LT_GDN){
//			sq_en = 1;
//			wb_mode = 2;
//		}else if(ltype==LT_IGDN){
//			sq_en = 1;
//			wb_mode = 1;
//		}else{
//			wb_mode = 0;
//		}
//
//		uint8_t ps_sf_bit, ps_mask;
//		switch (p_stride)
//		{
//		case 1:
//			ps_sf_bit = 0;
//			ps_mask = 0x0;
//			break;
//		case 2:
//			ps_sf_bit = 1;
//			ps_mask = 0x1;
//			break;
//		case 4:
//			ps_sf_bit = 2;
//			ps_mask = 0x3;
//			break;
//		default:
//			ps_sf_bit = 0;
//			ps_mask = 0x0;
//			break;
//		}
//
//		uint16_t TR, TC, TM, TN;
//		// TC = MIN(((MAX_IB_W-K)/kstride_c+1), MAX_Tc);
//		// TC = MIN(OC, TC);
//		// TR = MIN(((MAX_IB_H-K)/kstride_c+1), MAX_Tr);
//		// TR = MIN(OR, TR);
//
////		TC = MIN(((MAX_IB_HW-ksize)/c_stride+1), ow);
////		TC = MIN(MAX_Trc, TC);
////		int TCol_max_tmp = (TC-1)*c_stride + ksize;
////		TR = MIN(((MAX_IB_HW/TCol_max_tmp-ksize)/c_stride+1), oh);
////		TR = MIN(MAX_Trc/TC, TR);
//        TC = MIN(((MAX_IB_HW/ksize-ksize)/c_stride+1), ow);//at least left K rows for calc
//        TC = MIN(MAX_Trc, TC);
//        int TCol_max = (TC-1)*c_stride + ksize;
//        TR = MIN(MAX_Trc/TC, oh);
//        TR = MIN(((MAX_IB_HW/TCol_max-ksize)/c_stride+1), TR);
//
//		TM = MIN(ofm_num, MAX_Tof);
//		TN = MIN(ifm_num, MAX_Tif);
//		// printf("TR,TC,TM,TN=%d,%d,%d,%d\n", TR, TC, TM, TN);
////		assert((TR*TC)<=MAX_Trc);
//
//		uint32_t OHW = oh*ow;
//		uint8_t KxK = ksize*ksize;
//		uint32_t IHxIW = ih*iw;
//		uint16_t p_stridexIR = p_stride*ih;
//		uint16_t p_stridexIC = p_stride*iw;
//
//		uint16_t NToy = ceil(oh*1.0f/TR);
//		uint16_t NTox = ceil(ow*1.0f/TC);
//		uint16_t NTof = ceil(ofm_num*1.0f/TM);
//		uint64_t NTcomb = NToy*NTox*NTof;
//		uint16_t NTif = ceil(ifm_num*1.0f/TN);
//
//		uint8_t lmode;
//		uint64_t NTcomb_l;
//		if(NTif==1){
//			// printf("NTif=%d\n", NTif);
//			lmode = 0;
//			NTcomb_l = NTcomb+2;
//		}else{
//			lmode = 1;
//			NTcomb_l = NTcomb+1;
//		}
//
//		ifm_ptr = MEM_BASE + IFM_offset[lnum]*4;
//		ofm_ptr = MEM_BASE + OFM_offset[lnum]*4;
//
//		// printf("%s: ic_h_w= [%d, %d, %d], oc_h_w= [%d, %d, %d], k=%d, s=%d, h_pad=%d, w_pad=%d, pad=%d\n", LN_s[lnum],
//		// ifm_num, ih, iw, ofm_num, oh, ow, ksize, kstride, h_pad, w_pad, pad);
//		// IC_codec(ofm_ptr, ifm_ptr, kernel_w_ptr, bias_ptr, ifm_num, ih, iw, ofm_num, oh, ow, pad, ksize, kstride, ltype, bias_en);
//		IC_codec(ofm_ptr, ifm_ptr, kernel_w_ptr, bias_ptr, ifm_num, ih, iw, ofm_num, oh, ow, pad, ksize, kstride, ltype,
//					p_stride, c_stride, wb_mode, ps_sf_bit, ps_mask, sq_en, TR, TC, TM, TN, OHW, KxK,
//                 	IHxIW, p_stridexIR, p_stridexIC, bias_en,
//					NToy, NTox, NTof, NTcomb, NTif, lmode, NTcomb_l);
//
//		kernel_w_ptr = kernel_w_ptr + ksize*ksize*ifm_num*ofm_num;
//		if(bias_en)
//			bias_ptr = bias_ptr + ofm_num;
//
//		time2 = what_time_is_it_now();
//		printf("%s: ic_h_w= [%d, %d, %d], oc_h_w= [%d, %d, %d], lat= %3.5lf\n", LN_s[lnum],
//		ifm_num, ih, iw, ofm_num, oh, ow, time2 - time1);
//	}
//
//	data_num = OH_set[LNUM-1]*OW_set[LNUM-1]*3;
//	float *syn_out_f32 = (float *)malloc(sizeof(float)*data_num);
//	copy_dev2mem((uint8_t *)syn_out_f32, data_num*4, ofm_ptr);
//
//	time1 = what_time_is_it_now();
//	save_image_png_chw(syn_out_f32, OH_set[LNUM-1], OW_set[LNUM-1], 3, "recon_c");
//	time2 = what_time_is_it_now();
//	printf("save_png(s): %3.5lf\n", time2 - time1);
//
//	printf("syn_output: %s, h_w_c= [%d, %d, %d]\n", "recon_c.png", OH_set[LNUM-1], OW_set[LNUM-1], OF_NUM_set[LNUM-1]);
//
////	free(fm_mem);
//	// free(img_in_hwc_pad);
////	free(kernel_w);
////	free(bias);
//
//	return 0;
//}
//
//int ana_transform(int argc, char *argv[], int ana_o_dim[3], char *ana_out_bname){
//
//	const uint32_t F_num = 128;
//	const uint32_t LNUM = 5;
//
//	const int IF_NUM_set[LNUM] = {    3, F_num, F_num, F_num, F_num};
//	const int OF_NUM_set[LNUM] = {F_num, F_num, F_num, F_num, F_num};
//	const int KS_set[LNUM] = {9, 1, 5, 1, 5};
//	const int P_set[LNUM]  = {4, 0, 2, 0, 2};
//	const int S_set[LNUM]  = {4, 1, 2, 1, 2};
//	const bool bias_en_set[LNUM] = {true, true, true, true, false};
//	const int LT_set[LNUM] = {LT_CONV, LT_GDN, LT_CONV, LT_GDN, LT_CONV};
//	const char *LN_s[LNUM] = {"ana_0", "gdn_0", "ana_1", "gdn_1", "ana_2"};
//
//	int IW_set[LNUM], IH_set[LNUM], OW_set[LNUM], OH_set[LNUM];
//	int IFM_offset[LNUM], OFM_offset[LNUM];
//
//	FILE *fp;
//	int data_num;
//	char img_buff[256];
//	char *input_imgfn = img_buff;
//	if(argc==1)
//		strncpy(input_imgfn, "abigail-keenan-27293.png", 256);
//	else
//		strncpy(input_imgfn, argv[1], 256);
//	image im = load_image_stb(input_imgfn, 3);//3 channel img
//	printf("Input img:%s\n w=%d,h=%d,c=%d\n", input_imgfn, im.w, im.h, im.c);
//
//	int w_num = 0;
//	int bias_num = 0;
//	int fm_max_size = 0;
//	for(int lnum = 0; lnum < LNUM; lnum++){
////calc sum of bias/weight total num
//		if(bias_en_set[lnum])
//			bias_num = bias_num + OF_NUM_set[lnum];
//
//		w_num = w_num + IF_NUM_set[lnum]*OF_NUM_set[lnum]*KS_set[lnum]*KS_set[lnum];
//
////calc ifm/ofm width/height
//		if(lnum==0){
//			IW_set[lnum] = im.w;
//			IH_set[lnum] = im.h;
//		}else{
//			IW_set[lnum] = OW_set[lnum-1];
//			IH_set[lnum] = OH_set[lnum-1];
//		}
//		OW_set[lnum] = (IW_set[lnum] - KS_set[lnum] + 2*P_set[lnum])/S_set[lnum] + 1;
//		OH_set[lnum] = (IH_set[lnum] - KS_set[lnum] + 2*P_set[lnum])/S_set[lnum] + 1;
//
//		int ifm_offset = IW_set[lnum]*IH_set[lnum]*IF_NUM_set[lnum];
//		int ofm_offset = OW_set[lnum]*OH_set[lnum]*OF_NUM_set[lnum];
//		if(lnum==0){
//			fm_max_size = ifm_offset + ofm_offset;
//		}else{
//			int tmp_fm_size = fm_max_size - ifm_offset;
//			if(tmp_fm_size < ofm_offset)
//				fm_max_size = ifm_offset + ofm_offset;
//		}
//	}
//
//	for(int lnum = 0; lnum < LNUM; lnum++){
//		int ifm_offset = IW_set[lnum]*IH_set[lnum]*IF_NUM_set[lnum];
//		int ofm_offset = OW_set[lnum]*OH_set[lnum]*OF_NUM_set[lnum];
//		if(lnum==0){
//			IFM_offset[lnum] = 0;
//			OFM_offset[lnum] = fm_max_size - ofm_offset;
//		}else{
//			IFM_offset[lnum] = OFM_offset[lnum-1];
//			if(IFM_offset[lnum]!=0){
//				OFM_offset[lnum] = 0;
//			}else{
//				OFM_offset[lnum] = fm_max_size - ofm_offset;
//			}
//		}
//	}
//
//	copy_file2mem("ana_kernel_w_reorg.bin", get_file_size("ana_kernel_w_reorg.bin"), WEIGHT_BASE);
//	copy_file2mem("ana_bias.bin", get_file_size("ana_bias.bin"), BETA_BASE);
//
//	float *syn_in_median = (float *)malloc(sizeof(float)*F_num);
//	read_binfile_flt32_rb(syn_in_median, "syn_in_median", F_num);
//
////	float *fm_mem = (float *)malloc(sizeof(float)*fm_max_size);
//	float *img_in_chw = im.data;
////	memcpy(fm_mem, img_in_chw, im.c*im.h*im.w*sizeof(float));
//	copy_mem2dev((uint8_t *)img_in_chw, IW_set[0]*IH_set[0]*IF_NUM_set[0]*sizeof(float), MEM_BASE + IFM_offset[0]*4);
//	free_image(im);
//
////	fp = fopen("ana_kernel_w_reorg.bin", "rb");
////	float *kernel_w = (float *)malloc(sizeof(float)*w_num);
////	fread(kernel_w, sizeof(float), w_num, fp);
////	fclose(fp);
////
////	fp = fopen("ana_bias.bin", "rb");
////	float *bias = (float *)malloc(sizeof(float)*bias_num);
////	fread(bias, sizeof(float), bias_num, fp);
////	fclose(fp);
//
////	float *kernel_w_ptr = kernel_w;
////	float *bias_ptr = bias;
////	float *ifm_ptr, *ofm_ptr;
//	unsigned int ifm_ptr, ofm_ptr;
//	unsigned int kernel_w_ptr = 0;
//	unsigned int bias_ptr = 0;
//
//	double time1, time2;
//	for(int lnum = 0; lnum < LNUM; lnum++){
//		time1 = what_time_is_it_now();
//
//		int ltype = LT_set[lnum];
//		bool bias_en = bias_en_set[lnum];
//		int pad = P_set[lnum];
//		int ih = IH_set[lnum];
//		int iw = IW_set[lnum];
//		int oh = OH_set[lnum];
//		int ow = OW_set[lnum];
//		// int h_pad = ih + pad*2;
//		// int w_pad = iw + pad*2;
//		int ksize = KS_set[lnum];
//		int kstride = S_set[lnum];
//		int ifm_num = IF_NUM_set[lnum];
//		int ofm_num = OF_NUM_set[lnum];
//
//		uint8_t p_stride, c_stride;
//		switch (ltype)
//		{
//			case LT_CONVT:
//				p_stride = kstride;
//				c_stride = 1;
//				break;
//			case LT_IGDN:
//				p_stride = kstride;
//				c_stride = kstride;
//				break;
//			default:
//				p_stride = 1;
//				c_stride = kstride;
//				break;
//		}
//
//		uint8_t wb_mode;
//		bool sq_en = 0;
//		if(ltype==LT_GDN){
//			sq_en = 1;
//			wb_mode = 2;
//		}else if(ltype==LT_IGDN){
//			sq_en = 1;
//			wb_mode = 1;
//		}else{
//			wb_mode = 0;
//		}
//
//		uint8_t ps_sf_bit, ps_mask;
//		switch (p_stride)
//		{
//		case 1:
//			ps_sf_bit = 0;
//			ps_mask = 0x0;
//			break;
//		case 2:
//			ps_sf_bit = 1;
//			ps_mask = 0x1;
//			break;
//		case 4:
//			ps_sf_bit = 2;
//			ps_mask = 0x3;
//			break;
//		default:
//			ps_sf_bit = 0;
//			ps_mask = 0x0;
//			break;
//		}
//
//		uint16_t TR, TC, TM, TN;
//		// TC = MIN(((MAX_IB_W-K)/kstride_c+1), MAX_Tc);
//		// TC = MIN(OC, TC);
//		// TR = MIN(((MAX_IB_H-K)/kstride_c+1), MAX_Tr);
//		// TR = MIN(OR, TR);
//
//        TC = MIN(((MAX_IB_HW/ksize-ksize)/c_stride+1), ow);//at least left K rows for calc
//        TC = MIN(MAX_Trc, TC);
//        int TCol_max = (TC-1)*c_stride + ksize;
//        TR = MIN(MAX_Trc/TC, oh);
//        TR = MIN(((MAX_IB_HW/TCol_max-ksize)/c_stride+1), TR);
////        int TRow_max = (TR-1)/c_stride+1;
//
//		TM = MIN(ofm_num, MAX_Tof);
//		TN = MIN(ifm_num, MAX_Tif);
//		// printf("TR,TC,TM,TN=%d,%d,%d,%d\n", TR, TC, TM, TN);
////		assert((TR*TC)<=MAX_Trc);
//
//		uint32_t OHW = oh*ow;
//		uint8_t KxK = ksize*ksize;
//		uint32_t IHxIW = ih*iw;
//		uint16_t p_stridexIR = p_stride*ih;
//		uint16_t p_stridexIC = p_stride*iw;
//
//		uint16_t NToy = ceil(oh*1.0f/TR);
//		uint16_t NTox = ceil(ow*1.0f/TC);
//		uint16_t NTof = ceil(ofm_num*1.0f/TM);
//		uint64_t NTcomb = NToy*NTox*NTof;
//		uint16_t NTif = ceil(ifm_num*1.0f/TN);
//
//		uint8_t lmode;
//		uint64_t NTcomb_l;
//		if(NTif==1){
//			// printf("NTif=%d\n", NTif);
//			lmode = 0;
//			NTcomb_l = NTcomb+2;
//		}else{
//			lmode = 1;
//			NTcomb_l = NTcomb+1;
//		}
//
//		ifm_ptr = MEM_BASE + IFM_offset[lnum]*4;
//		ofm_ptr = MEM_BASE + OFM_offset[lnum]*4;
//
//		IC_codec(ofm_ptr, ifm_ptr, kernel_w_ptr, bias_ptr, ifm_num, ih, iw, ofm_num, oh, ow, pad, ksize, kstride, ltype,
//					p_stride, c_stride, wb_mode, ps_sf_bit, ps_mask, sq_en, TR, TC, TM, TN, OHW, KxK,
//                 	IHxIW, p_stridexIR, p_stridexIC, bias_en, NToy, NTox, NTof, NTcomb, NTif, lmode, NTcomb_l);
//
//		kernel_w_ptr = kernel_w_ptr + ksize*ksize*ifm_num*ofm_num;
//		if(bias_en)
//			bias_ptr = bias_ptr + ofm_num;
//
//		time2 = what_time_is_it_now();
//		printf("%s: ic_h_w= [%d, %d, %d], oc_h_w= [%d, %d, %d], lat= %3.5lf\n", LN_s[lnum],
//		ifm_num, ih, iw, ofm_num, oh, ow, time2 - time1);
//	}
//
////save transformed img
//
//	data_num = OH_set[LNUM-1]*OW_set[LNUM-1]*F_num;
//	int32_t *ana_out = (int32_t *)malloc(sizeof(float)*data_num);
//
//	float *ana_out_f32 = (float *)malloc(sizeof(float)*data_num);
//	copy_dev2mem((uint8_t *)ana_out_f32, data_num*4, ofm_ptr);
//
//	ana_a05_sub_median_chw(ana_out, ana_out_f32, syn_in_median, OH_set[LNUM-1], OW_set[LNUM-1], F_num);
//	free(syn_in_median);
//
//	printf("ana_output: %s, h_w_c= [%d, %d, %d]\n", ana_out_bname, OH_set[LNUM-1], OW_set[LNUM-1], F_num);
//	fp = fopen(ana_out_bname, "wb");
//	fwrite(ana_out, sizeof(int32_t), data_num, fp);
//	fclose(fp);
//
////	free(fm_mem);
////	free(kernel_w);
////	free(bias);
//	free(ana_out_f32);
//	free(ana_out);
//
//	ana_o_dim[0] = OH_set[LNUM-1];
//	ana_o_dim[1] = OW_set[LNUM-1];
//	ana_o_dim[2] = F_num;
//
//	return 0;
//}
//
//int main( int argc, char *argv[])
//{
//	int ana_o_dim[3];
//	char *ana_out_bname = "ana_q_out_chw.bin";
//
//	ana_transform( argc, argv, ana_o_dim, ana_out_bname);
//
//	syn_transform( ana_o_dim, ana_out_bname);
//
//    return 0;
//}
//
