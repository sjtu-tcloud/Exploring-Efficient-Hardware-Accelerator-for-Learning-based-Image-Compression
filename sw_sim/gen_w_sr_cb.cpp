
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "yolov2_reorg_rsc_0_3.h"

const uint32_t F_num = 128;
const uint32_t LNUM = 5;

void gen_ana_w_sr_cb(){
	const int IF_NUM_set[LNUM] = {    3, F_num, F_num, F_num, F_num};
	const int OF_NUM_set[LNUM] = {F_num, F_num, F_num, F_num, F_num};
	const int KS_set[LNUM] = {9, 1, 5, 1, 5};

	FILE *fp;
	int data_num;

	data_num = get_file_size("ana_kernel_w_i16rc.bin");
	int16_t *kernel_w = (int16_t *)malloc(data_num);
	read_binfile_flt32_rb((float *)kernel_w, "ana_kernel_w_i16rc.bin", data_num/4);

	int w_aoffset[LNUM];
	read_binfile_flt32_rb((float*)w_aoffset, "ana_kernel_w_i16rc_oadd.bin", LNUM);

	int interQarray[LNUM];
	read_binfile_flt32_rb((float*)interQarray, "ana_i16c_interQ.bin", LNUM);

	int16_t *kernel_w_ptr = kernel_w;
	for(int lnum = 0; lnum < LNUM; lnum++){
		int ksize = KS_set[lnum];
		int ifm_num = IF_NUM_set[lnum];
		int ofm_num = OF_NUM_set[lnum];

		int interQ = interQarray[lnum];
		uint8_t wQ = interQ/2 + (interQ & 0x1);
		w_sr(kernel_w_ptr, (ksize*ksize*ifm_num*ofm_num + w_aoffset[lnum]), wQ);

		kernel_w_ptr = kernel_w_ptr + ksize*ksize*ifm_num*ofm_num + w_aoffset[lnum];
	}

	fp = fopen("ana_kernel_w_i16rc_srcb.bin", "wb");
	fwrite(kernel_w, sizeof(int16_t), data_num/sizeof(int16_t), fp);
	fclose(fp);

	free(kernel_w);
}

void gen_syn_w_sr_cb(){
	const int IF_NUM_set[LNUM] = {F_num, F_num, F_num, F_num, F_num};
	const int OF_NUM_set[LNUM] = {F_num, F_num, F_num, F_num,     3};
	const int KS_set[LNUM] = {5, 1, 5, 1, 9};

	FILE *fp;
	int data_num;

	data_num = get_file_size("syn_kernel_w_T_i16rc.bin");
	int16_t *kernel_w = (int16_t *)malloc(data_num);
	read_binfile_flt32_rb((float *)kernel_w, "syn_kernel_w_T_i16rc.bin", data_num/4);

	int w_aoffset[LNUM];
	read_binfile_flt32_rb((float*)w_aoffset, "syn_kernel_w_T_i16rc_oadd.bin", LNUM);
	
	int interQarray[LNUM];
	read_binfile_flt32_rb((float*)interQarray, "syn_i16c_interQ.bin", LNUM);

	int16_t *kernel_w_ptr = kernel_w;
	for(int lnum = 0; lnum < LNUM; lnum++){
		int ksize = KS_set[lnum];
		int ifm_num = IF_NUM_set[lnum];
		int ofm_num = OF_NUM_set[lnum];

		int interQ = interQarray[lnum];
		uint8_t wQ = interQ/2 + (interQ & 0x1);
		w_sr(kernel_w_ptr, (ksize*ksize*ifm_num*ofm_num + w_aoffset[lnum]), wQ);

		kernel_w_ptr = kernel_w_ptr + ksize*ksize*ifm_num*ofm_num + w_aoffset[lnum];

	}

	fp = fopen("syn_kernel_w_T_i16rc_srcb.bin", "wb");
	fwrite(kernel_w, sizeof(int16_t), data_num/sizeof(int16_t), fp);
	fclose(fp);

	free(kernel_w);
}

int main( int argc, char *argv[])
{
	gen_ana_w_sr_cb();
	gen_syn_w_sr_cb();

    return 0;
}
