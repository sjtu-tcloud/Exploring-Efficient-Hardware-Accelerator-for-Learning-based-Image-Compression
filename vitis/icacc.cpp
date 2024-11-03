
#include "acc_new_reorg.h"

void copy_mem2dev(uint8_t *orig,uint32_t byte_num, unsigned long in_buffer)
{
	int fd = open("/dev/mem", O_RDWR);
	unsigned char *virtual_addr;
	uint32_t RequestByteNum;// must page
	if(byte_num%(HPAGESIZE)==0)
		RequestByteNum = byte_num;
	else
	{
		RequestByteNum = ceil(byte_num/(HPAGESIZE*1.0))*(HPAGESIZE);
	}
	virtual_addr = (unsigned char *)mmap(NULL, RequestByteNum, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)in_buffer);
	if(virtual_addr == MAP_FAILED)
	{
		perror("Virtual_addr_in mappong for absolute memory access failed!\n");
		return;
	}
	memcpy(virtual_addr,orig,byte_num);

	munmap((void *)virtual_addr, byte_num);
	close(fd);
}

void copy_dev2mem(uint8_t *dst,uint32_t byte_num, unsigned long in_buffer)
{
	int fd = open("/dev/mem", O_RDWR);
	unsigned char *virtual_addr;
	uint32_t RequestByteNum;// must page
	if(byte_num%(HPAGESIZE)==0)
		RequestByteNum = byte_num;
	else
	{
		RequestByteNum = ceil(byte_num/(HPAGESIZE*1.0))*(HPAGESIZE);
	}
		virtual_addr = (unsigned char *)mmap(NULL, RequestByteNum, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)in_buffer);
	if(virtual_addr == MAP_FAILED)
	{
		perror("Virtual_addr_in mappong for absolute memory access failed!\n");
		return;
	}
	printf("copy start-----byte_num=%d\n",byte_num);
	memcpy((uint8_t *)dst,virtual_addr,byte_num);
	printf("copy ok!\n");

	munmap((void *)virtual_addr, byte_num);
	close(fd);
}

int copy_file2mem(char *bin_file,uint32_t byte_num,unsigned long in_buffer)
{
	unsigned char *buffer = (unsigned char *)malloc(HPAGESIZE);
	if(buffer==NULL){
		printf("cannot malloc buffer %d byte\n", HPAGESIZE);
		return -1;
	}
//	printf("Total Byte Num = %d\n Address 0x%X\n", byte_num, in_buffer);
	FILE *fp;
	if( (fp = fopen(bin_file, "rb")) == NULL)fprintf(stderr,"CANNOT OPEN bin_file\n");
	int rd_num;
	unsigned long offset = 0;
	while(rd_num = fread(buffer, sizeof(unsigned char), HPAGESIZE, fp))
	{
		if(rd_num < HPAGESIZE)
			rd_num = HPAGESIZE;
		copy_mem2dev(buffer,rd_num, in_buffer+offset);
//		printf("rd_num=%d, offset=%d\n", rd_num, offset);
		offset += rd_num;
	}
//	printf("copy_file2mem offset=%d\n",offset);
	fclose(fp);

	free(buffer);


	return 0;
}

int copy_mem2file(char *bin_file,uint32_t byte_num,unsigned long in_buffer)
{
	void *buffer = malloc(HPAGESIZE);
	if(buffer==NULL){
		printf("cannot malloc buffer %d byte\n", HPAGESIZE);
		return -1;
	}

	FILE *fp;
	if( (fp = fopen(bin_file, "wb")) == NULL)fprintf(stderr,"CANNOT OPEN bin_file\n");

	int x = byte_num;
	int addbyte;
	unsigned long offset = 0;
	while(addbyte=((x<HPAGESIZE)?x:(HPAGESIZE)))
	{
		copy_dev2mem((uint8_t *)buffer,addbyte, in_buffer+offset);
		fwrite(buffer , sizeof(unsigned char), addbyte, fp);
		x -= addbyte;
		offset += addbyte;
	}
	printf("copy_mem2file offset=%d\n",offset);


	fclose(fp);

	free(buffer);

	return 0;
}

double what_time_is_it_now()
{
    struct timeval time;
    if (gettimeofday(&time,NULL)){
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

int IC_codec(unsigned int  ofm_addr, unsigned int ifm_addr, unsigned int weight_offset, unsigned int bias_offset,
				unsigned int ifm_scale, unsigned int ofm_scale, unsigned int kernel_scale,
                int IF, int IR, int IC, int OF, int OR, int OC, int pad, int K, int S, int ltype,
                uint8_t p_stride, uint8_t c_stride, uint8_t wb_mode, uint8_t ps_sf_bit, uint8_t ps_mask, bool sq_en,
                uint16_t TR, uint16_t TC, uint16_t TM, uint16_t TN, uint32_t OHW, uint8_t KxK,
                uint32_t IHxIW, uint16_t p_stridexIR, uint16_t p_stridexIC, bool use_bias,
				uint16_t NToy, uint16_t NTox, uint16_t NTof, uint64_t NTcomb, uint16_t NTif, uint8_t lmode, uint64_t NTcomb_l,
				uint8_t ifm_sqQ, uint8_t interQ, uint8_t IsFMreod)//, uint16_t RTof)
{
	unsigned int ap_idle;
	unsigned int ap_done;

	unsigned long int PhysicalAddress = ICCODEC_BASEADDR;
	int map_len = 0x200;
	int fd = open("/dev/mem", O_RDWR);

	unsigned char *xbase_address;
	xbase_address = (unsigned char *)mmap(NULL, map_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)PhysicalAddress);
	if(xbase_address == MAP_FAILED)
	{
		perror("1:Init Mapping memory for absolute memory access failed.\n");
		return -1;
	}

	while(1)
	{
		ap_idle = ((ReadReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_AP_CTRL) >> 2) && 0x1);
		if(ap_idle)
			break;
	}

//	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_OFM_V_DATA, ofm_addr);
//	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_IFM_V_DATA,  ifm_addr);
//	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_IFM_EXTRA_V_DATA,  ifm_addr);
//	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_KERNEL_V_DATA, WEIGHT_BASE + weight_offset*2);

	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_OFM0_V_DATA, ofm_addr);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_OFM1_V_DATA, ofm_addr);
//	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_OFM2_V_DATA, ofm_addr);
//	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_OFM3_V_DATA, ofm_addr);
//
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_IFM0_V_DATA,  ifm_addr);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_IFM1_V_DATA,  ifm_addr);
//	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_IFM2_V_DATA,  ifm_addr);
//	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_IFM3_V_DATA,  ifm_addr);
//
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_IFM_EXTRA0_V_DATA,  ifm_addr);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_IFM_EXTRA1_V_DATA,  ifm_addr);
//	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_IFM_EXTRA2_V_DATA,  ifm_addr);
//	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_IFM_EXTRA3_V_DATA,  ifm_addr);

	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_KERNEL0_V_DATA, WEIGHT_BASE + weight_offset*2);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_KERNEL1_V_DATA, WEIGHT_BASE + weight_offset*2);
//	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_KERNEL2_V_DATA, WEIGHT_BASE + weight_offset*2);
//	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_KERNEL3_V_DATA, WEIGHT_BASE + weight_offset*2);

	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_BIAS_V_DATA, BETA_BASE + bias_offset*4);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_IFM_SCALE_V_DATA, IFM_SCALE_BASE + ifm_scale*4);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_OFM_SCALE_V_DATA, OFM_SCALE_BASE + ofm_scale*4);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_KERNEL_SCALE_V_DATA, K_SCALE_BASE + kernel_scale*4);

	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_IF_R_DATA,  IF);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_IR_DATA,  IR);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_IC_DATA,  IC);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_OF_R_DATA,  OF);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_OR_R_DATA,  OR);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_OC_DATA,  OC);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_PAD_DATA,  pad);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_K_DATA,  K);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_S_DATA,  S);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_LTYPE_DATA,  ltype);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_P_STRIDE_DATA,  p_stride);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_C_STRIDE_DATA,  c_stride);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_WB_MODE_DATA,  wb_mode);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_PS_SF_BIT_DATA,  ps_sf_bit);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_PS_MASK_DATA,  ps_mask);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_SQ_EN_DATA,  sq_en);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_TR_DATA,  TR);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_TC_DATA,  TC);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_TM_DATA,  TM);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_TN_DATA,  TN);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_OHW_DATA,  OHW);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_KXK_DATA,  KxK);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_IHXIW_DATA,  IHxIW);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_P_STRIDEXIR_DATA,  p_stridexIR);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_P_STRIDEXIC_DATA,  p_stridexIC);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_USE_BIAS_DATA,  use_bias);

	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_NTOY_DATA,  NToy);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_NTOX_DATA,  NTox);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_NTOF_DATA,  NTof);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_NTCOMB_DATA,  NTcomb);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_NTIF_DATA,  NTif);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_LMODE_DATA,  lmode);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_NTCOMB_L_DATA,  NTcomb_l);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_IFM_SQQ_DATA,  ifm_sqQ);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_INTERQ_DATA,  interQ);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_FMREOD_DATA,  IsFMreod);
//	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_RTOF_DATA,  RTof);

	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_GIE, 0x0);
	WriteReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_AP_CTRL, 0x1);//Start
	while(1)
	{
		ap_done = ((ReadReg(xbase_address, XIC_CODEC_CTRL_BUS_ADDR_AP_CTRL) >> 1) && 0x1);
		if(ap_done)
			break;
	}

	munmap((void *)xbase_address, map_len);
	close(fd);

	return 0;
}
