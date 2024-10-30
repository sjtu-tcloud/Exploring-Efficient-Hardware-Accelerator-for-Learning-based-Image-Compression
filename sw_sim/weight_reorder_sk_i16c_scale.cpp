
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
// #include "yolov2_reorg_rsc.h"

#define LT_CONV (0)
#define LT_CONVT (1)
#define LT_GDN (2)
#define LT_IGDN (3)

#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))

int LANE_ext(int tmp_in, int lane_num){
	int val = (((tmp_in + lane_num -1)/lane_num)*lane_num);
	return val;
}

int LANE_div(int tmp_in, int lane_num){
	int val = ((tmp_in + lane_num -1)/lane_num);
	return val;
}

unsigned long get_file_size(const char *filename)
{
    struct stat buf;
    if(stat(filename, &buf)<0)
    {
        return 0;
    }
	printf("%s's data size is %ld\n", filename, buf.st_size);
    return (unsigned long)buf.st_size;
}


int read_binfile_flt32_rb(float *buf, char *filename, int data_num)
{
	FILE *fp;
	if( (fp = fopen(filename, "rb")) == NULL)
		printf("cannot open bin_file %s\n", filename);
	int rd_num  = fread(buf, sizeof(unsigned char)*4, data_num, fp);
	fclose(fp);

	return rd_num;
}

void weight_reorder_tile_f32c(float *weight, float *local_wbuf, int IFxOF, int OF, int KK, int TM_MIN, int TN_MIN,
		 int tm, int tn, int ksize, int lane_num, int *add_offset, FILE *fp, int ltype)//KKNM
{
    const uint8_t LUT_sub_Ky[4]  = {3, 3, 2, 2};
    const uint8_t LUT_sub_Kx[4]  = {3, 2, 3, 2};

    const uint8_t LUT_sub5x5_r[4*3*3] = { 0, 2,  4, 10,  12,  14,  20,  22,  24, //3x3
                                          1, 3, 11, 13,  21,  23,0xFF,0xFF,0xFF, //3x2
                                          5, 7,  9, 15,  17,  19,0xFF,0xFF,0xFF, //2x3
                                          6, 8, 16, 18,0xFF,0xFF,0xFF,0xFF,0xFF  //2x2
    };
    const uint8_t LUT_sub9x9_r[16*3*3] =  {  0,  4,   8, 36, 40,  44,  72,  76,  80, //3x3
                                             3,  7, 39, 43,  75,  79,0xFF,0xFF,0xFF, //3x2
                                             2,  6, 38, 42,  74,  78,0xFF,0xFF,0xFF, //3x2
                                             1,  5, 37, 41,  73,  77,0xFF,0xFF,0xFF, //3x2
                                            27, 31, 35, 63,  67,  71,0xFF,0xFF,0xFF, //2x3
                                            30, 34, 66, 70,0xFF,0xFF,0xFF,0xFF,0xFF, //2x2
                                            29, 33, 65, 69,0xFF,0xFF,0xFF,0xFF,0xFF, //2x2
                                            28, 32, 64, 68,0xFF,0xFF,0xFF,0xFF,0xFF, //2x2
                                            18, 22, 26, 54,  58,  62,0xFF,0xFF,0xFF, //2x3
                                            21, 25, 57, 61,0xFF,0xFF,0xFF,0xFF,0xFF, //2x2
                                            20, 24, 56, 60,0xFF,0xFF,0xFF,0xFF,0xFF, //2x2
                                            19, 23, 55, 59,0xFF,0xFF,0xFF,0xFF,0xFF, //2x2
                                             9, 13, 17, 45,  49,  53,0xFF,0xFF,0xFF, //2x3
                                            12, 16, 48, 52,0xFF,0xFF,0xFF,0xFF,0xFF, //2x2
                                            11, 15, 47, 51,0xFF,0xFF,0xFF,0xFF,0xFF, //2x2
                                            10, 14, 46, 50,0xFF,0xFF,0xFF,0xFF,0xFF, //2x2
    };

    const uint8_t k_offset_sub5x5[4] = {0, 9, 15, 21};
    const uint8_t k_offset_sub9x9[16] = {0, 9, 15, 21, 27, 33, 37, 41, 45, 51, 55, 59, 63, 69, 73, 77};

	int t1,t2,t3,t4,t5;

	int ma3_d3 = (TM_MIN+lane_num-1)/lane_num;
	float tmp_in_float;

	if(ltype==LT_CONVT){
		uint8_t subK_num = 1;
		if(ksize==5)
			subK_num = 4;
		else if(ksize==9)
			subK_num = 16;
		assert((ksize==5)||(ksize==9));
		for(int z=0; z<subK_num; z++){

			uint8_t ifm_sel;
			uint8_t Ky, Kx;
			uint8_t k_offset_cur;
			if(subK_num==16){
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
			}else if(subK_num == 4){
				ifm_sel = z;
				Ky = LUT_sub_Ky[ifm_sel];
				Kx = LUT_sub_Kx[ifm_sel];
				k_offset_cur = k_offset_sub5x5[z];
			}else{
				ifm_sel = 0;
				Ky = ksize;
				Kx = ksize;
				k_offset_cur = 0;
			}

			for(t1 = 0;t1 < ma3_d3; t1++)
			for(t2 = 0;t2 < TN_MIN; t2++)
			for(t3 = 0;t3 <Ky; t3++)
			for(t4 = 0;t4 <Kx; t4++)
			for(t5 = 0;t5 <lane_num; t5++)
			{
				uint8_t sub_num;
				uint8_t kyx = t3*Kx + t4;
				if(subK_num == 16){
					sub_num = LUT_sub9x9_r[z*3*3 + kyx];
				}else if(subK_num == 4){
					sub_num = LUT_sub5x5_r[z*3*3 + kyx];
				}else{
					sub_num = kyx;
				}
				int idx_i = sub_num*IFxOF + (t2+tn)*OF + (t1*lane_num+t5+tm);//KKNM
				int idx_o = t1*TN_MIN*KK*lane_num + t2*KK*lane_num + (kyx + k_offset_cur)*lane_num + t5;
				if((t1*lane_num+t5)>=TM_MIN)
					tmp_in_float = 0;
				else
					tmp_in_float = weight[idx_i];
				local_wbuf[idx_o] = tmp_in_float;
			}
		}

	}else{//for CONV
		for(t1 = 0;t1 < ma3_d3; t1++)
		for(t2 = 0;t2 < TN_MIN; t2++)
		for(t3 = 0;t3 <ksize; t3++)
		for(t4 = 0;t4 <ksize; t4++)
		for(t5 = 0;t5 <lane_num; t5++)
		{
			// int idx_i = (t1*lane_num+t5+tm)*INumxKK + (t2+tn)*KK + t3*ksize + t4;
			int idx_i = (t3*ksize + t4)*IFxOF + (t2+tn)*OF + (t1*lane_num+t5+tm);//src is KKNM format in ic
			int idx_o = t1*TN_MIN*KK*lane_num + t2*KK*lane_num + t3*ksize*lane_num + t4*lane_num + t5;
			if((t1*lane_num+t5)>=TM_MIN)
				tmp_in_float = 0;
			else
				tmp_in_float = weight[idx_i];

			local_wbuf[idx_o] = tmp_in_float;

		}
	}

	uint16_t val_left = ma3_d3*TN_MIN*KK*lane_num - TM_MIN*TN_MIN*KK;
	*add_offset = *add_offset + val_left;
	fwrite(local_wbuf,sizeof(float), ma3_d3*TN_MIN*KK*lane_num, fp);
}

int weight_reorder_f32c(float *Weight, int IFM_NUM,int OFM_NUM,int Ksize,int TM,int TN, int lane_num, int MaxTif, int MaxTof, int *add_offset, FILE *fp, int ltype)
{
	const int KxK = Ksize*Ksize;
	const int IFxOF = IFM_NUM*OFM_NUM;

	// float local_wbuf[(Tm+3)/4*Tn*K*K*4];
	int lbuf_size = ((MaxTof+lane_num-1)/lane_num)*MaxTif*Ksize*Ksize*lane_num;
	float *local_wbuf = (float *)malloc(lbuf_size*sizeof(float));

	int tm,tn,tk;

	int offset_cur = IFM_NUM*OFM_NUM*KxK;
	printf("Layer[x]; param num=%12d ", offset_cur);
	*add_offset = 0;

	int TM_MIN,TN_MIN;
    for(tm = 0; tm < OFM_NUM; tm += TM)
    {
		TM_MIN = MIN(TM,OFM_NUM-tm);
		for(tn = 0; tn < IFM_NUM; tn += TN)
		{
			TN_MIN = MIN(TN, IFM_NUM-tn);
			weight_reorder_tile_f32c(Weight, local_wbuf, IFxOF, OFM_NUM, KxK, TM_MIN, TN_MIN, tm, tn, Ksize, lane_num, add_offset, fp, ltype);
		}
    }

	printf("add_offset = %d", *add_offset);
	printf("\n");
	free(local_wbuf);

	return 0;
}

int bias_reorder_f32c(float *in,const int *offset, const bool *bias_en, int layer_num, int lane_num, int *add_offset, FILE *fp)
{
	static float bias_buf_c[4096+128];

	int woffset = 0;
	int offset_cur = 0;
	for(int i=0;i<layer_num;i++)
	{
		if(!bias_en[i])
			continue;

		offset_cur = offset[i];
		printf("Layer %2d;param num=%12d ",i, offset_cur);

		int data_size = ((offset_cur + lane_num -1)/lane_num)*lane_num;
		for(int j=0;j<data_size;j++)
		{
			float tmp_in_float;
			if(j < offset_cur)
				tmp_in_float = in[woffset+j];
			else
				tmp_in_float = 0;

			bias_buf_c[j] = tmp_in_float;
		}

		if(offset_cur%lane_num)
		{
			add_offset[i] = lane_num - (offset_cur%lane_num);
			printf("add %d\n", add_offset[i]);
		}else{
			printf("\n");
		}

		fwrite(bias_buf_c,sizeof(float), data_size, fp);

		woffset += offset_cur;
	}

	return 0;
}

int bias_reorder_i16c(float *in, const int *offset, const bool *bias_en,int layer_num, float *ap16_range,int *maxQ_array, int lane_num, int *add_offset, FILE *fp)
{
	const float MIN_VALUE = (-1024*1024*1024);
	const float MAX_VALUE = (1024*1024*1024);

	static int16_t bias_buf_c[4096+128];

	int woffset = 0;
	int offset_cur = 0;
	for(int i=0;i<layer_num;i++)
	{
		if(!bias_en[i])
			continue;

		offset_cur = offset[i];
		printf("Layer %2d;param num=%12d ",i, offset_cur);

		float min,max;
		min = MAX_VALUE;
		max = MIN_VALUE;
		for(int j=0;j<offset_cur;j++)
		{
			float tmp_in_float = in[woffset+j];
			if(tmp_in_float<min)
				min = tmp_in_float;
			if(tmp_in_float>max)
				max = tmp_in_float;
		}
		printf("float min=%.7lf,max=%.7lf ",min,max);//find float min max

		int k;
		int maxQ = -1;
		for(k=0;k<16;k++)//find maxQ
		{
			if(min>ap16_range[2*k]&&max<ap16_range[2*k+1])
			{
				maxQ = k;
			}
			else if(k==0)
			{
				printf("beyond Q0 min=%.7lf,max=%.7lf ",min,max);
				break;
			}
		}
		printf("maxQ=%d ",maxQ);
		maxQ_array[i] = maxQ;

		double max_error,min_error,sum_error;
		sum_error = 0;
		max_error = MIN_VALUE;
		min_error = MAX_VALUE;

		int data_size = ((offset_cur + lane_num -1)/lane_num)*lane_num;
		for(int j=0;j<data_size;j++)
		{
			float tmp_in_float;
			if(j < offset_cur)
				tmp_in_float = in[woffset+j];
			else
				tmp_in_float = 0;

			int16_t tmp_fixed = (int16_t)(tmp_in_float*pow(2.0,maxQ));
			float tmp_out_float = (float)tmp_fixed*pow(2.0,-maxQ);
			double error = (tmp_out_float - tmp_in_float)*(tmp_out_float - tmp_in_float);
			error = sqrt(error);
			sum_error += error;
			if(error<min_error)
				min_error = error;
			if(error>max_error)
				max_error = error;

			bias_buf_c[j] = tmp_fixed;
		}
		printf("sum2_error = %.7lf,min_error=%.7lf,max_error=%.7lf\n",sum_error,min_error,max_error);

		if(offset_cur%lane_num)
		{
			add_offset[i] = lane_num - (offset_cur%lane_num);
			printf("add %d\n", add_offset[i]);
		}

		fwrite(bias_buf_c,sizeof(int16_t), data_size, fp);

		woffset += offset_cur;
	}

	return 0;
}

void weight_reorder_tile_i16c(float *weight, int16_t *local_wbuf, int IFxOF, int OF, int INumxKK, int KK, int TM_MIN, int TN_MIN, int tm, int tn, int ksize, int lane_num,
		 int maxQ, int *add_offset, FILE *fp, double *max_error, double *min_error, double *sum_error, int ltype, float *ifm_scale, float*kernel_scale)
{
	int t1,t2,t3,t4,t5;

	double max_error_l, min_error_l, sum_error_l;
	max_error_l = *max_error;
	min_error_l = *min_error;
	sum_error_l = *sum_error;

    const uint8_t LUT_sub_Ky[4]  = {3, 3, 2, 2};
    const uint8_t LUT_sub_Kx[4]  = {3, 2, 3, 2};

    const uint8_t LUT_sub5x5_r[4*3*3] = { 0, 2,  4, 10,  12,  14,  20,  22,  24, //3x3
                                          1, 3, 11, 13,  21,  23,0xFF,0xFF,0xFF, //3x2
                                          5, 7,  9, 15,  17,  19,0xFF,0xFF,0xFF, //2x3
                                          6, 8, 16, 18,0xFF,0xFF,0xFF,0xFF,0xFF  //2x2
    };
    const uint8_t LUT_sub9x9_r[16*3*3] =  {  0,  4,   8, 36, 40,  44,  72,  76,  80, //3x3
                                             3,  7, 39, 43,  75,  79,0xFF,0xFF,0xFF, //3x2
                                             2,  6, 38, 42,  74,  78,0xFF,0xFF,0xFF, //3x2
                                             1,  5, 37, 41,  73,  77,0xFF,0xFF,0xFF, //3x2
                                            27, 31, 35, 63,  67,  71,0xFF,0xFF,0xFF, //2x3
                                            30, 34, 66, 70,0xFF,0xFF,0xFF,0xFF,0xFF, //2x2
                                            29, 33, 65, 69,0xFF,0xFF,0xFF,0xFF,0xFF, //2x2
                                            28, 32, 64, 68,0xFF,0xFF,0xFF,0xFF,0xFF, //2x2
                                            18, 22, 26, 54,  58,  62,0xFF,0xFF,0xFF, //2x3
                                            21, 25, 57, 61,0xFF,0xFF,0xFF,0xFF,0xFF, //2x2
                                            20, 24, 56, 60,0xFF,0xFF,0xFF,0xFF,0xFF, //2x2
                                            19, 23, 55, 59,0xFF,0xFF,0xFF,0xFF,0xFF, //2x2
                                             9, 13, 17, 45,  49,  53,0xFF,0xFF,0xFF, //2x3
                                            12, 16, 48, 52,0xFF,0xFF,0xFF,0xFF,0xFF, //2x2
                                            11, 15, 47, 51,0xFF,0xFF,0xFF,0xFF,0xFF, //2x2
                                            10, 14, 46, 50,0xFF,0xFF,0xFF,0xFF,0xFF, //2x2
    };

    const uint8_t k_offset_sub5x5[4] = {0, 9, 15, 21};
    const uint8_t k_offset_sub9x9[16] = {0, 9, 15, 21, 27, 33, 37, 41, 45, 51, 55, 59, 63, 69, 73, 77};

	int ma3_d3 = (TM_MIN+lane_num-1)/lane_num;

	// for(t1 = 0;t1 < ma3_d3; t1++)
	// for(t2 = 0;t2 < TN_MIN; t2++)
	// for(t3 = 0;t3 <ksize; t3++)
	// for(t4 = 0;t4 <ksize; t4++)
	// for(t5 = 0;t5 <lane_num; t5++)
	// {
	// 	float tmp_in_float;
	// 	int idx_i = (t3*ksize + t4)*IFxOF + (t2+tn)*OF + (t1*lane_num+t5+tm);//KKNM
	// 	if((t1*lane_num+t5)>=TM_MIN)
	// 		tmp_in_float = 0;
	// 	else{
	// 		// float ifm_s = ifm_scale[t2+tn];
	// 		// assert(ifm_s>0);
	// 		// weight[idx_i] = weight[idx_i]*ifm_s;//to combine ifm_s to weight

	// 		tmp_in_float = weight[idx_i];
	// 	}
	// 	float tmp_in_f_abs = fabs(tmp_in_float);
	// 	float k_scale =  kernel_scale[t1*lane_num+t5+tm];
	// 	if(tmp_in_f_abs > k_scale)
	// 		kernel_scale[t1*lane_num+t5+tm] = tmp_in_f_abs;
	// }

	if(ltype==LT_CONVT){
		uint8_t subK_num = 1;
		if(ksize==5)
			subK_num = 4;
		else if(ksize==9)
			subK_num = 16;
		assert((ksize==5)||(ksize==9));

		for(int z=0; z<subK_num; z++){
			uint8_t ifm_sel;
			uint8_t Ky, Kx;
			uint8_t k_offset_cur;
			if(subK_num==16){
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
			}else if(subK_num == 4){
				ifm_sel = z;
				Ky = LUT_sub_Ky[ifm_sel];
				Kx = LUT_sub_Kx[ifm_sel];
				k_offset_cur = k_offset_sub5x5[z];
			}else{
				ifm_sel = 0;
				Ky = ksize;
				Kx = ksize;
				k_offset_cur = 0;
			}

			for(t1 = 0;t1 < ma3_d3; t1++)
			for(t2 = 0;t2 < TN_MIN; t2++)
			for(t3 = 0;t3 <Ky; t3++)
			for(t4 = 0;t4 <Kx; t4++)
			for(t5 = 0;t5 <lane_num; t5++)
			{
				float tmp_in_float;
				uint8_t sub_num;
				uint8_t kyx = t3*Kx + t4;
				if(subK_num == 16){
					sub_num = LUT_sub9x9_r[z*3*3 + kyx];
				}else if(subK_num == 4){
					sub_num = LUT_sub5x5_r[z*3*3 + kyx];
				}else{
					sub_num = kyx;
				}
				int idx_i = sub_num*IFxOF + (t2+tn)*OF + (t1*lane_num+t5+tm);//KKNM
				int idx_o = t1*TN_MIN*KK*lane_num + t2*KK*lane_num + (kyx + k_offset_cur)*lane_num + t5;
				if((t1*lane_num+t5)>=TM_MIN)
					tmp_in_float = 0;
				else{
					float k_scale =  kernel_scale[t1*lane_num+t5+tm];
					assert(k_scale > 0);
					if(k_scale==0){
						assert(weight[idx_i]==0);
						tmp_in_float = weight[idx_i];
					}
					else
						tmp_in_float = weight[idx_i]/k_scale;
					assert(weight[idx_i] <= k_scale);
				}
				int16_t tmp_fixed = (int16_t)(tmp_in_float*pow(2.0,maxQ));

				if((t1*lane_num+t5)<TM_MIN){
					float k_scale =  kernel_scale[t1*lane_num+t5+tm];
					float tmp_out_float = (float)tmp_fixed*pow(0.5, maxQ);
					double error = (tmp_out_float - tmp_in_float)*(tmp_out_float - tmp_in_float);
					error = sqrt(error);
					sum_error_l = sum_error_l + error;
					if(error<min_error_l)
						min_error_l = error;
					if(error>max_error_l)
						max_error_l = error;
					// assert(kernel_scale[t1*lane_num+t5]>0);
				}

				local_wbuf[idx_o] = tmp_fixed;
			}
		}

	}else{//for CONV

	// 	bool sq_en = 0;
	// 	if(ltype==LT_GDN){
	// 		sq_en = 1;
	// 	}else if(ltype==LT_IGDN){
	// 		sq_en = 1;
	// 	}

	// //step 1: find max_scale for each ofm channel with ifm_scale combined
	// 	for(t1 = 0;t1 < ma3_d3; t1++)
	// 	for(t2 = 0;t2 < TN_MIN; t2++)
	// 	for(t3 = 0;t3 <ksize; t3++)
	// 	for(t4 = 0;t4 <ksize; t4++)
	// 	for(t5 = 0;t5 <lane_num; t5++)
	// 	{
	// 		// int idx_i = (t1*lane_num+t5+tm)*INumxKK + (t2+tn)*KK + t3*ksize + t4;
	// 		int idx_i = (t3*ksize + t4)*IFxOF + (t2+tn)*OF + (t1*lane_num+t5+tm);//src is KKNM format in ic
	// 		if((t1*lane_num+t5)>=TM_MIN)
	// 			tmp_in_float = 0;
	// 		else{
	// 			// float ifm_s = ifm_scale[t2+tn];
	// 			// assert(ifm_s>0);
	// 			// if(sq_en){
	// 			// 	weight[idx_i] = weight[idx_i]*ifm_s*ifm_s;
	// 			// }
	// 			// else
	// 			// 	weight[idx_i] = weight[idx_i]*ifm_s;

	// 			tmp_in_float = weight[idx_i];
	// 		}
	// 		float tmp_in_f_abs = abs(tmp_in_float);
	// 		float k_scale =  kernel_scale[t1*lane_num+t5+tm];
	// 		if(tmp_in_f_abs > k_scale)
	// 			kernel_scale[t1*lane_num+t5+tm] = tmp_in_f_abs;
	// 	}

		for(t1 = 0;t1 < ma3_d3; t1++)
		for(t2 = 0;t2 < TN_MIN; t2++)
		for(t3 = 0;t3 <ksize; t3++)
		for(t4 = 0;t4 <ksize; t4++)
		for(t5 = 0;t5 <lane_num; t5++)
		{
			float tmp_in_float;
			// int idx_i = (t1*lane_num+t5+tm)*INumxKK + (t2+tn)*KK + t3*ksize + t4;
			int idx_i = (t3*ksize + t4)*IFxOF + (t2+tn)*OF + (t1*lane_num+t5+tm);//src is KKNM format in ic
			int idx_o = t1*TN_MIN*KK*lane_num + t2*KK*lane_num + t3*ksize*lane_num + t4*lane_num + t5;
			if((t1*lane_num+t5)>=TM_MIN)
				tmp_in_float = 0;
			else{
				float k_scale =  kernel_scale[t1*lane_num+t5+tm];
				assert(k_scale > 0);
				if(k_scale==0)
				{
					assert(weight[idx_i]==0);
					tmp_in_float = weight[idx_i];
				}else{
					tmp_in_float = weight[idx_i]/k_scale;
				}
				assert(weight[idx_i] <= k_scale);
			}

			int16_t tmp_fixed = (int16_t)(tmp_in_float*pow(2.0, maxQ));

			if((t1*lane_num+t5) < TM_MIN){
				// float k_scale =  kernel_scale[t1*lane_num+t5+tm];
				float tmp_out_float = (float)tmp_fixed*pow(0.5, maxQ);
				double error = (tmp_out_float - tmp_in_float)*(tmp_out_float - tmp_in_float);
				error = sqrt(error);
				sum_error_l = sum_error_l + error;
				if(error<min_error_l)
					min_error_l = error;
				if(error>max_error_l)
					max_error_l = error;
			}

			local_wbuf[idx_o] = tmp_fixed;
		}
	}

	uint16_t val_left = ma3_d3*TN_MIN*KK*lane_num - TM_MIN*TN_MIN*KK;
	*add_offset = *add_offset + val_left;
	fwrite(local_wbuf,sizeof(int16_t), ma3_d3*TN_MIN*KK*lane_num, fp);


	*max_error = max_error_l;
	*min_error = min_error_l;
	*sum_error = sum_error_l;
}

int weight_reorder_i16c(float *Weight, int IFM_NUM,int OFM_NUM,int Ksize,int TM,int TN, float *ifm_scale, float*kernel_scale,
							int lane_num, int MaxTif, int MaxTof, int *add_offset, FILE *fp, int ltype)
{
	const float MIN_VALUE = (-1024*1024*1024);
	const float MAX_VALUE = (1024*1024*1024);

	const int KxK = Ksize*Ksize;
	const int IFM_NUMxKxK = IFM_NUM*KxK;
	int IFxOF = IFM_NUM*OFM_NUM;
	int OF = OFM_NUM;

	// float local_wbuf[(Tm+3)/4*Tn*K*K*4];
	int lbuf_size = ((MaxTof+lane_num-1)/lane_num)*MaxTif*Ksize*Ksize*lane_num;
	int16_t *local_wbuf = (int16_t *)malloc(lbuf_size*sizeof(int16_t));

	int offset_cur = IFM_NUM*OFM_NUM*KxK;
	printf("Layer[x]; param num=%12d ", offset_cur);
	*add_offset = 0;
	int maxQ = 14;

	double max_error,min_error,sum_error;
	sum_error = 0;
	max_error = MIN_VALUE;
	min_error = MAX_VALUE;

	bool sq_en = 0;
	if(ltype==LT_GDN){
		sq_en = 1;
	}else if(ltype==LT_IGDN){
		sq_en = 1;
	}

	int ma3_d3 = (OFM_NUM+lane_num-1)/lane_num;
	for(int t1 = 0;t1 < ma3_d3; t1++)
	for(int t2 = 0;t2 < IFM_NUM; t2++)
	for(int t3 = 0;t3 <Ksize; t3++)
	for(int t4 = 0;t4 <Ksize; t4++)
	for(int t5 = 0;t5 <lane_num; t5++)
	{
		float tmp_in_float;
		int idx_i = (t3*Ksize + t4)*IFxOF + t2*OF + (t1*lane_num+t5);//KKNM
		if((t1*lane_num+t5)>=OFM_NUM)
			tmp_in_float = 0;
		else{
			// float ifm_s = ifm_scale[t2+tn];
			// assert(ifm_s>0);
			// weight[idx_i] = weight[idx_i]*ifm_s;//to combine ifm_s to weight

			float ifm_s = ifm_scale[t2];
			assert(ifm_s>0);
			// if(sq_en){
			// 	Weight[idx_i] = Weight[idx_i]*ifm_s*ifm_s;
			// }
			// else
				Weight[idx_i] = Weight[idx_i]*ifm_s;

			tmp_in_float = Weight[idx_i];
		}
		float tmp_in_f_abs = fabs(tmp_in_float);
		float k_scale =  kernel_scale[t1*lane_num+t5];
		if(tmp_in_f_abs > k_scale)
			kernel_scale[t1*lane_num+t5] = tmp_in_f_abs;
	}

    for(int tm = 0; tm < OFM_NUM; tm += TM)
    {
		int TM_MIN = MIN(TM,OFM_NUM-tm);
		for(int tn = 0; tn < IFM_NUM; tn += TN)
		{
			int TN_MIN = MIN(TN, IFM_NUM-tn);
			weight_reorder_tile_i16c(Weight, local_wbuf, IFxOF, OF, IFM_NUMxKxK, KxK, TM_MIN, TN_MIN, tm, tn, Ksize, lane_num,
										maxQ, add_offset, fp, &max_error,&min_error,&sum_error, ltype, ifm_scale, kernel_scale);
		}
    }

	printf("sum2_error = %.7lf,min_error=%.7lf,max_error=%.7lf,add_offset = %d",sum_error,min_error,max_error, *add_offset);
	printf("\n");

	// for(int i=0; i<OFM_NUM; i++){
	// 	// printf("%5.6lf ", kernel_scale[i]);
	// 	assert(kernel_scale[i]>0);
	// }

	free(local_wbuf);

	return 0;
}


void ana_reorder(int LANE_NUM, int MaxTif, int MaxTof){

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

	FILE *fp;
	int w_num = 0;
	int bias_num = 0;
	int ifm_channel_num = 0, ofm_channel_num = 0;
	for(int lnum = 0; lnum < LNUM; lnum++){
//calc sum of bias/weight total num
		ifm_channel_num = ifm_channel_num + LANE_ext(IF_NUM_set[lnum], LANE_NUM);
		ofm_channel_num = ofm_channel_num + LANE_ext(OF_NUM_set[lnum], LANE_NUM);

		if(bias_en_set[lnum])
			bias_num = bias_num + OF_NUM_set[lnum];
		w_num = w_num + IF_NUM_set[lnum]*OF_NUM_set[lnum]*KS_set[lnum]*KS_set[lnum];
	}

	fp = fopen("../bin/ana_w.bin", "rb");
	float *kernel_w = (float *)malloc(sizeof(float)*w_num);
	fread(kernel_w, sizeof(float), w_num, fp);
	fclose(fp);

	fp = fopen("../bin/ana_b.bin", "rb");
	float *bias = (float *)malloc(sizeof(float)*bias_num);
	fread(bias, sizeof(float), bias_num, fp);
	fclose(fp);

	// float *kernel_w_reorg = (float *)malloc(sizeof(float)*w_num);

    int q_offset[LNUM];
	// int weight_Qarray[LNUM];
	// memset(weight_Qarray, 0, sizeof(weight_Qarray));
	memset(q_offset, 0, sizeof(q_offset));
	FILE *fout = fopen("ana_kernel_w_i16rc.bin","wb");
	if(!fout) printf("fopen ana_kernel_w_i16rc.bin error\n");

	float *ifm_Scale = (float *)calloc(sizeof(float), ifm_channel_num);
	float *kernel_Scale = (float *)calloc(sizeof(float), ofm_channel_num);
	read_binfile_flt32_rb((float *)ifm_Scale, "ana_i16c_ifm_scale.bin", ifm_channel_num);

	memset(kernel_Scale, 0, ofm_channel_num*sizeof(float));

	int w_offset = 0;
	float *ifm_Scale_ptr = ifm_Scale;
	float *k_Scale_ptr = kernel_Scale;
	for(int lnum = 0; lnum < LNUM; lnum++){

		int ltype = LT_set[lnum];
		int ksize = KS_set[lnum];
		int ifm_num = IF_NUM_set[lnum];
		int ofm_num = OF_NUM_set[lnum];

		int TM, TN;
		TM = MIN(ofm_num, MaxTof);
		TN = MIN(ifm_num, MaxTif);

		// weight_reorg(kernel_w + w_offset, kernel_w_reorg+w_offset, ifm_num,ofm_num,ksize, TM, TN);
		// weight_reorder_f32c(kernel_w + w_offset,ifm_num,ofm_num,ksize,TM,TN, LANE_NUM, MaxTif, MaxTof, &q_offset[lnum], fout, ltype);
		weight_reorder_i16c(kernel_w + w_offset,ifm_num,ofm_num,ksize,TM,TN, ifm_Scale_ptr, k_Scale_ptr, LANE_NUM, MaxTif, MaxTof, &q_offset[lnum], fout, ltype);

		printf("ana_kernel_scale[layer %d]: %d\n", lnum, OF_NUM_set[lnum]);
		for(int i=0; i<OF_NUM_set[lnum]; i++){
			printf("%5.6lf ", k_Scale_ptr[i]);
			assert(k_Scale_ptr[i]>0);
		}
		printf("\n");

		ifm_Scale_ptr = ifm_Scale_ptr + LANE_ext(IF_NUM_set[lnum], LANE_NUM);
		k_Scale_ptr = k_Scale_ptr + LANE_ext(OF_NUM_set[lnum], LANE_NUM);
		w_offset = w_offset + ksize*ksize*ifm_num*ofm_num;

		// printf("%s: ic,oc= [%d, %d]\n", LN_s[lnum], ifm_num, ofm_num);
	}
	fclose(fout);//weight reorg quanti

	fout = fopen("ana_kernel_w_i16rc_oadd.bin","wb");
	if(!fout) printf("fopen ana_kernel_w_i16rc_oadd.bin error\n");
	fwrite(q_offset, sizeof(int), LNUM, fout);
	fclose(fout);

	FILE *fp_ioq;
	fp_ioq = fopen("ana_i16c_kernel_scale.bin", "wb");
	if(!fp_ioq) printf("ana_i16c_kernel_scale.bin\n");
	fwrite(kernel_Scale, sizeof(float), ofm_channel_num, fp_ioq);
	fclose(fp_ioq);//iofm quanti

	fout = fopen("ana_bias_f32c.bin","wb");
	if(!fout) printf("fopen bias_f32c.bin error\n");

	memset(q_offset, 0, sizeof(q_offset));

	bias_reorder_f32c(bias, OF_NUM_set, bias_en_set, LNUM, LANE_NUM, q_offset, fout);
	fclose(fout);

	fout = fopen("ana_bias_f32c_oadd.bin","wb");
	if(!fout) printf("fopen bias_f32c_oadd.bin error\n");
	fwrite(q_offset, sizeof(int), LNUM, fout);
	fclose(fout);

	free(bias);
	free(kernel_w);
}

void syn_reorder(int LANE_NUM, int MaxTif, int MaxTof){

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

	FILE *fp;

	int w_num = 0;
	int bias_num = 0;
	int ifm_channel_num = 0, ofm_channel_num = 0;
	for(int lnum = 0; lnum < LNUM; lnum++){
//calc sum of bias/weight total num
		ifm_channel_num = ifm_channel_num + LANE_ext(IF_NUM_set[lnum], LANE_NUM);
		ofm_channel_num = ofm_channel_num + LANE_ext(OF_NUM_set[lnum], LANE_NUM);
		if(bias_en_set[lnum])
			bias_num = bias_num + OF_NUM_set[lnum];
		w_num = w_num + IF_NUM_set[lnum]*OF_NUM_set[lnum]*KS_set[lnum]*KS_set[lnum];
	}

	// data_num = 5*5*128*128 + 1*1*128*128 + 5*5*128*128 + 1*1*128*128 + 9*9*128*3;
	fp = fopen("../bin/syn_w.bin", "rb");
	float *kernel_w = (float *)malloc(sizeof(float)*w_num);
	fread(kernel_w, sizeof(float), w_num, fp);
	fclose(fp);

	// data_num = 128 + 128 + 128 + 128 + 3;
	fp = fopen("../bin/syn_b.bin", "rb");
	float *bias = (float *)malloc(sizeof(float)*bias_num);
	fread(bias, sizeof(float), bias_num, fp);
	fclose(fp);

    int q_offset[LNUM];
	// int weight_Qarray[LNUM];
	// memset(weight_Qarray, 0, sizeof(weight_Qarray));
	memset(q_offset, 0, sizeof(q_offset));
	FILE *fout = fopen("syn_kernel_w_T_i16rc.bin","wb");
	if(!fout) printf("fopen syn_kernel_w_T_i16rc.bin error\n");

	float *ifm_Scale = (float *)calloc(sizeof(float), ifm_channel_num);
	float *kernel_Scale = (float *)calloc(sizeof(float), ofm_channel_num);
	read_binfile_flt32_rb((float *)ifm_Scale, "syn_i16c_ifm_scale.bin", ifm_channel_num);
	memset(kernel_Scale, 0, ofm_channel_num*sizeof(float));

	int w_offset = 0;
	float *ifm_Scale_ptr = ifm_Scale;
	float *k_Scale_ptr = kernel_Scale;
	for(int lnum = 0; lnum < LNUM; lnum++){

		int ltype = LT_set[lnum];
		int ksize = KS_set[lnum];
		int ifm_num = IF_NUM_set[lnum];
		int ofm_num = OF_NUM_set[lnum];

		int TM, TN;
		TM = MIN(ofm_num, MaxTof);
		TN = MIN(ifm_num, MaxTif);

		// weight_reorg(kernel_w + w_offset, kernel_w_reorg+w_offset, ifm_num,ofm_num,ksize, TM, TN);
		// weight_reorder_f32c(kernel_w + w_offset,ifm_num,ofm_num,ksize,TM,TN, LANE_NUM, MaxTif, MaxTof, &q_offset[lnum], fout, ltype);
		weight_reorder_i16c(kernel_w + w_offset,ifm_num,ofm_num,ksize,TM,TN, ifm_Scale_ptr, k_Scale_ptr, LANE_NUM, MaxTif, MaxTof, &q_offset[lnum], fout, ltype);

		printf("syn_kernel_scale[layer %d]: %d\n", lnum, OF_NUM_set[lnum]);
		for(int i=0; i<OF_NUM_set[lnum]; i++){
			printf("%5.6lf ", k_Scale_ptr[i]);
			assert(k_Scale_ptr[i]>0);
		}
		printf("\n");

		ifm_Scale_ptr = ifm_Scale_ptr + LANE_ext(IF_NUM_set[lnum], LANE_NUM);
		k_Scale_ptr = k_Scale_ptr + LANE_ext(OF_NUM_set[lnum], LANE_NUM);
		w_offset = w_offset + ksize*ksize*ifm_num*ofm_num;

		// printf("%s: ic,oc= [%d, %d]\n", LN_s[lnum], ifm_num, ofm_num);

	}
	fclose(fout);//weight reorg quanti

	fout = fopen("syn_kernel_w_T_i16rc_oadd.bin","wb");
	if(!fout) printf("fopen syn_kernel_w_T_i16rc_oadd.bin error\n");
	fwrite(q_offset, sizeof(int), LNUM, fout);
	fclose(fout);

	FILE *fp_ioq;
	fp_ioq = fopen("syn_i16c_kernel_scale.bin", "wb");
	if(!fp_ioq) printf("syn_i16c_kernel_scale.bin\n");
	fwrite(kernel_Scale, sizeof(float), ofm_channel_num, fp_ioq);
	fclose(fp_ioq);//iofm quanti

	fout = fopen("syn_bias_f32c.bin","wb");
	if(!fout) printf("fopen bias_f32c.bin error\n");

	memset(q_offset, 0, sizeof(q_offset));

	bias_reorder_f32c(bias, OF_NUM_set, bias_en_set, LNUM, LANE_NUM, q_offset, fout);
	fclose(fout);

	fout = fopen("syn_bias_f32c_oadd.bin","wb");
	if(!fout) printf("fopen bias_f32c_oadd.bin error\n");
	fwrite(q_offset, sizeof(int), LNUM, fout);
	fclose(fout);

	free(kernel_w);
	free(bias);

}

int main( int argc, char *argv[])
{

	if(argc<4){
		printf("1<argc<4, pls press LANE_NUM MAX_Tif MAX_Tof\n");
		return -1;
	}
	// strncpy(bfile_name, argv[1], 256);
	int LANE_NUM = atoi(argv[1]);
	int MaxTif = atoi(argv[2]);
	int MaxTof = atoi(argv[3]);

	ana_reorder(LANE_NUM, MaxTif, MaxTof);

	syn_reorder(LANE_NUM, MaxTif, MaxTof);

    return 0;
}
