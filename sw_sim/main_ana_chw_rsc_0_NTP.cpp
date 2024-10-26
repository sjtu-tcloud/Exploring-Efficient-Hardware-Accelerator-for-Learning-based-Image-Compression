
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "yolov2_reorg_rsc_0_NTP.h"

const uint32_t F_num = 128;
const uint32_t LNUM = 5;

int main( int argc, char *argv[])
{
	const int IF_NUM_set[LNUM] = {    3, F_num, F_num, F_num, F_num};
	const int OF_NUM_set[LNUM] = {F_num, F_num, F_num, F_num, F_num};
	const int KS_set[LNUM] = {9, 1, 5, 1, 5};
	const int P_set[LNUM]  = {4, 0, 2, 0, 2};
	const int S_set[LNUM]  = {4, 1, 2, 1, 2};
	const bool bias_en_set[LNUM] = {true, true, true, true, false};
	const int LT_set[LNUM] = {LT_CONV, LT_GDN, LT_CONV, LT_GDN, LT_CONV};
	const char *LN_s[LNUM] = {"ana_0", "gdn_0", "ana_1", "gdn_1", "ana_2"};

	int IW_set[LNUM], IH_set[LNUM], OW_set[LNUM], OH_set[LNUM];
	int IFM_offset[LNUM], OFM_offset[LNUM];

	FILE *fp;
	int data_num;
	char img_buff[256];
	char *input_imgfn = img_buff;
	if(argc==1)
		strncpy(input_imgfn, "../clic2020_train_data/abigail-keenan-27293.png", 256);
	else
		strncpy(input_imgfn, argv[1], 256);
	image im = load_image_stb(input_imgfn, 3);//3 channel img
	printf("Input img:%s\n w=%d,h=%d,c=%d\n", input_imgfn, im.w, im.h, im.c);

	int fm_max_size = 0;
	for(int lnum = 0; lnum < LNUM; lnum++){
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

	data_num = get_file_size("ana_kernel_w_f32rc.bin");
	float *kernel_w = (float *)malloc(data_num);
	printf("read weight byte_num = %d\n", read_binfile_flt32_rb((float *)kernel_w, "ana_kernel_w_f32rc.bin", data_num/4));

	data_num = get_file_size("ana_bias_f32c.bin");
	float *bias = (float *)malloc(data_num);
	printf("%d\n", read_binfile_flt32_rb((float *)bias, "ana_bias_f32c.bin", data_num/4));
	printf("read bias byte_num = %d\n", data_num);

	int w_aoffset[LNUM];
	printf("%d\n", read_binfile_flt32_rb((float*)w_aoffset, "ana_kernel_w_f32rc_oadd.bin", LNUM));

	int bias_aoffset[LNUM];
	printf("%d\n", read_binfile_flt32_rb((float*)bias_aoffset, "ana_bias_f32c_oadd.bin", LNUM));

	float *fm_mem = (float *)malloc(sizeof(float)*fm_max_size);
	float *img_in_chw = im.data;

	int img_c = im.c, img_h = im.h, img_w = im.w;
	float *ifm_buf = fm_mem + IFM_offset[0];
	int Ca3_d4 =  LANE_div(img_c);
	for(int c4=0; c4<Ca3_d4; c4++)
	for(int ih=0; ih<img_h; ih++)
	for(int iw=0; iw<img_w; iw++)
	for(int ic=0; ic<LANE_NUM; ic++)
	{
		int idx_out = (c4*img_h*img_w + ih*img_w + iw)*LANE_NUM + ic;
		int idx_in = (c4*LANE_NUM+ic)*img_h*img_w + ih*img_w + iw;

		if((c4*LANE_NUM+ic) >= img_c)
			ifm_buf[idx_out] = 0;
		else
			ifm_buf[idx_out] = img_in_chw[idx_in];
	}
	free_image(im);

	float *kernel_w_ptr = kernel_w;
	float *bias_ptr = bias;
	float *ifm_ptr, *ofm_ptr;

	double time1, time2;
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
		int factor = 1;
		if(c_mode==2){
			factor = 2;
		}else if(c_mode==3){
			factor = 4;
		}else{
			factor = 1;
		}
		int TR_min = factor*MAX_Poy;//for Poy unroll
		TR = TR_min;

		int TRow_max = (TR-1)*c_stride + ksize;
		int max_ib_hw_left = MAX_IB_HW/TRow_max;
		max_Trc4TC = max_Trc4TC/TR;

		TC = MIN(((max_ib_hw_left-ksize)/c_stride+1), ow);//at least left K rows for calc
		TC = MIN(max_Trc4TC, TC);

		factor = factor*MAX_Pox;
		assert(TC>=factor);
		TC = TC/factor*factor;
		assert(TC%factor==0);//for Pox unroll

		int TCol_max = (TC-1)*c_stride + ksize;
		TR = MIN(MAX_Trc/TC, oh);
		TR = MIN(((MAX_IB_HW/TCol_max-ksize)/c_stride+1), TR);
		TRow_max = (TR-1)/c_stride+1;
		assert(TR>=TR_min);
		if(TR%TR_min)
		{
			TR = (TR/TR_min)*TR_min;
		}
		assert(TR%TR_min==0);//for Poy unroll

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

		IC_codec(ofm_ptr, ifm_ptr, kernel_w_ptr, bias_ptr, ifm_num, ih, iw, ofm_num, oh, ow, pad, ksize, kstride, ltype,
					p_stride, c_stride, wb_mode, ps_sf_bit, ps_mask, sq_en, TR, TC, TM, TN, OHW, KxK,
                 	IHxIW, p_stridexIR, p_stridexIC, bias_en, NToy, NTox, NTof, NTcomb, NTif, lmode, NTcomb_l);

		kernel_w_ptr = kernel_w_ptr + ksize*ksize*ifm_num*ofm_num + w_aoffset[lnum];
		if(bias_en)
			bias_ptr = bias_ptr + ofm_num + bias_aoffset[lnum];

		time2 = what_time_is_it_now();
		printf("%s: ic_h_w= [%d, %d, %d], oc_h_w= [%d, %d, %d], w_aoffset=%d, bias_aoffset=%d, lat= %3.5lf\n", LN_s[lnum],
		ifm_num, ih, iw, ofm_num, oh, ow, w_aoffset[lnum], bias_aoffset[lnum], time2 - time1);
	}

	data_num = OH_set[LNUM-1]*OW_set[LNUM-1]*OF_NUM_set[LNUM-1];
	float *ofm_tmp_buf = (float *)malloc(sizeof(float)*data_num);
	int ofm_w = OW_set[LNUM-1];
	int ofm_h = OH_set[LNUM-1];
	int ofm_c = OF_NUM_set[LNUM-1];
	for(int oc=0; oc<ofm_c;oc++)
	for(int oh=0; oh<ofm_h;oh++)
	for(int ow=0; ow<ofm_w;ow++){
		ofm_tmp_buf[oc*ofm_h*ofm_w + oh*ofm_w + ow] = ofm_ptr[((oc/LANE_NUM)*ofm_h*ofm_w + oh*ofm_w + ow)*LANE_NUM + (oc & (LANE_NUM-1))];
	}

//save transformed img
	data_num = F_num;
	float *syn_in_median = (float *)malloc(sizeof(float)*data_num);
	read_binfile_flt32_rb(syn_in_median, "syn_in_median", data_num);

	data_num = OH_set[LNUM-1]*OW_set[LNUM-1]*F_num;
	int32_t *ana_out = (int32_t *)malloc(sizeof(float)*data_num);
	// ana_a05_sub_median_chw(ana_out, ofm_ptr, syn_in_median, OH_set[LNUM-1], OW_set[LNUM-1], F_num);
	ana_a05_sub_median_chw(ana_out, ofm_tmp_buf, syn_in_median, OH_set[LNUM-1], OW_set[LNUM-1], F_num);
	free(syn_in_median);

	printf("ana_output: %s, h_w_c= [%d, %d, %d]\n", "ana_q_out_chw.bin", OH_set[LNUM-1], OW_set[LNUM-1], F_num);
	fp = fopen("ana_q_out_chw.bin", "wb");
	fwrite(ana_out, sizeof(int32_t), data_num, fp);
	fclose(fp);

	free(ana_out);
	free(fm_mem);
	free(ofm_tmp_buf);

	free(kernel_w);
	free(bias);

    return 0;
}
