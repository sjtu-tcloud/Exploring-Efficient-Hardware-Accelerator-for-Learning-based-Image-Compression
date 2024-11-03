
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/fb.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <sys/time.h>
#include <assert.h>
#include <math.h>
#include <sys/stat.h>

#define XIC_CODEC_CTRL_BUS_ADDR_AP_CTRL             0x000
#define XIC_CODEC_CTRL_BUS_ADDR_GIE                 0x004
#define XIC_CODEC_CTRL_BUS_ADDR_IER                 0x008
#define XIC_CODEC_CTRL_BUS_ADDR_ISR                 0x00c
#define XIC_CODEC_CTRL_BUS_ADDR_OFM0_V_DATA         0x010
#define XIC_CODEC_CTRL_BUS_BITS_OFM0_V_DATA         32
#define XIC_CODEC_CTRL_BUS_ADDR_OFM1_V_DATA         0x018
#define XIC_CODEC_CTRL_BUS_BITS_OFM1_V_DATA         32
#define XIC_CODEC_CTRL_BUS_ADDR_IFM0_V_DATA         0x020
#define XIC_CODEC_CTRL_BUS_BITS_IFM0_V_DATA         32
#define XIC_CODEC_CTRL_BUS_ADDR_IFM1_V_DATA         0x028
#define XIC_CODEC_CTRL_BUS_BITS_IFM1_V_DATA         32
#define XIC_CODEC_CTRL_BUS_ADDR_IFM_EXTRA0_V_DATA   0x030
#define XIC_CODEC_CTRL_BUS_BITS_IFM_EXTRA0_V_DATA   32
#define XIC_CODEC_CTRL_BUS_ADDR_IFM_EXTRA1_V_DATA   0x038
#define XIC_CODEC_CTRL_BUS_BITS_IFM_EXTRA1_V_DATA   32
#define XIC_CODEC_CTRL_BUS_ADDR_KERNEL0_V_DATA      0x040
#define XIC_CODEC_CTRL_BUS_BITS_KERNEL0_V_DATA      32
#define XIC_CODEC_CTRL_BUS_ADDR_KERNEL1_V_DATA      0x048
#define XIC_CODEC_CTRL_BUS_BITS_KERNEL1_V_DATA      32
#define XIC_CODEC_CTRL_BUS_ADDR_BIAS_V_DATA         0x050
#define XIC_CODEC_CTRL_BUS_BITS_BIAS_V_DATA         32
#define XIC_CODEC_CTRL_BUS_ADDR_IFM_SCALE_V_DATA    0x058
#define XIC_CODEC_CTRL_BUS_BITS_IFM_SCALE_V_DATA    32
#define XIC_CODEC_CTRL_BUS_ADDR_OFM_SCALE_V_DATA    0x060
#define XIC_CODEC_CTRL_BUS_BITS_OFM_SCALE_V_DATA    32
#define XIC_CODEC_CTRL_BUS_ADDR_KERNEL_SCALE_V_DATA 0x068
#define XIC_CODEC_CTRL_BUS_BITS_KERNEL_SCALE_V_DATA 32
#define XIC_CODEC_CTRL_BUS_ADDR_IF_R_DATA           0x070
#define XIC_CODEC_CTRL_BUS_BITS_IF_R_DATA           32
#define XIC_CODEC_CTRL_BUS_ADDR_IR_DATA             0x078
#define XIC_CODEC_CTRL_BUS_BITS_IR_DATA             32
#define XIC_CODEC_CTRL_BUS_ADDR_IC_DATA             0x080
#define XIC_CODEC_CTRL_BUS_BITS_IC_DATA             32
#define XIC_CODEC_CTRL_BUS_ADDR_OF_R_DATA           0x088
#define XIC_CODEC_CTRL_BUS_BITS_OF_R_DATA           32
#define XIC_CODEC_CTRL_BUS_ADDR_OR_R_DATA           0x090
#define XIC_CODEC_CTRL_BUS_BITS_OR_R_DATA           32
#define XIC_CODEC_CTRL_BUS_ADDR_OC_DATA             0x098
#define XIC_CODEC_CTRL_BUS_BITS_OC_DATA             32
#define XIC_CODEC_CTRL_BUS_ADDR_PAD_DATA            0x0a0
#define XIC_CODEC_CTRL_BUS_BITS_PAD_DATA            32
#define XIC_CODEC_CTRL_BUS_ADDR_K_DATA              0x0a8
#define XIC_CODEC_CTRL_BUS_BITS_K_DATA              32
#define XIC_CODEC_CTRL_BUS_ADDR_S_DATA              0x0b0
#define XIC_CODEC_CTRL_BUS_BITS_S_DATA              32
#define XIC_CODEC_CTRL_BUS_ADDR_LTYPE_DATA          0x0b8
#define XIC_CODEC_CTRL_BUS_BITS_LTYPE_DATA          32
#define XIC_CODEC_CTRL_BUS_ADDR_P_STRIDE_DATA       0x0c0
#define XIC_CODEC_CTRL_BUS_BITS_P_STRIDE_DATA       8
#define XIC_CODEC_CTRL_BUS_ADDR_C_STRIDE_DATA       0x0c8
#define XIC_CODEC_CTRL_BUS_BITS_C_STRIDE_DATA       8
#define XIC_CODEC_CTRL_BUS_ADDR_WB_MODE_DATA        0x0d0
#define XIC_CODEC_CTRL_BUS_BITS_WB_MODE_DATA        8
#define XIC_CODEC_CTRL_BUS_ADDR_PS_SF_BIT_DATA      0x0d8
#define XIC_CODEC_CTRL_BUS_BITS_PS_SF_BIT_DATA      8
#define XIC_CODEC_CTRL_BUS_ADDR_PS_MASK_DATA        0x0e0
#define XIC_CODEC_CTRL_BUS_BITS_PS_MASK_DATA        8
#define XIC_CODEC_CTRL_BUS_ADDR_SQ_EN_DATA          0x0e8
#define XIC_CODEC_CTRL_BUS_BITS_SQ_EN_DATA          1
#define XIC_CODEC_CTRL_BUS_ADDR_TR_DATA             0x0f0
#define XIC_CODEC_CTRL_BUS_BITS_TR_DATA             16
#define XIC_CODEC_CTRL_BUS_ADDR_TC_DATA             0x0f8
#define XIC_CODEC_CTRL_BUS_BITS_TC_DATA             16
#define XIC_CODEC_CTRL_BUS_ADDR_TM_DATA             0x100
#define XIC_CODEC_CTRL_BUS_BITS_TM_DATA             16
#define XIC_CODEC_CTRL_BUS_ADDR_TN_DATA             0x108
#define XIC_CODEC_CTRL_BUS_BITS_TN_DATA             16
#define XIC_CODEC_CTRL_BUS_ADDR_OHW_DATA            0x110
#define XIC_CODEC_CTRL_BUS_BITS_OHW_DATA            32
#define XIC_CODEC_CTRL_BUS_ADDR_KXK_DATA            0x118
#define XIC_CODEC_CTRL_BUS_BITS_KXK_DATA            8
#define XIC_CODEC_CTRL_BUS_ADDR_IHXIW_DATA          0x120
#define XIC_CODEC_CTRL_BUS_BITS_IHXIW_DATA          32
#define XIC_CODEC_CTRL_BUS_ADDR_P_STRIDEXIR_DATA    0x128
#define XIC_CODEC_CTRL_BUS_BITS_P_STRIDEXIR_DATA    16
#define XIC_CODEC_CTRL_BUS_ADDR_P_STRIDEXIC_DATA    0x130
#define XIC_CODEC_CTRL_BUS_BITS_P_STRIDEXIC_DATA    16
#define XIC_CODEC_CTRL_BUS_ADDR_USE_BIAS_DATA       0x138
#define XIC_CODEC_CTRL_BUS_BITS_USE_BIAS_DATA       1
#define XIC_CODEC_CTRL_BUS_ADDR_NTOY_DATA           0x140
#define XIC_CODEC_CTRL_BUS_BITS_NTOY_DATA           16
#define XIC_CODEC_CTRL_BUS_ADDR_NTOX_DATA           0x148
#define XIC_CODEC_CTRL_BUS_BITS_NTOX_DATA           16
#define XIC_CODEC_CTRL_BUS_ADDR_NTOF_DATA           0x150
#define XIC_CODEC_CTRL_BUS_BITS_NTOF_DATA           16
#define XIC_CODEC_CTRL_BUS_ADDR_NTCOMB_DATA         0x158
#define XIC_CODEC_CTRL_BUS_BITS_NTCOMB_DATA         64
#define XIC_CODEC_CTRL_BUS_ADDR_NTIF_DATA           0x164
#define XIC_CODEC_CTRL_BUS_BITS_NTIF_DATA           16
#define XIC_CODEC_CTRL_BUS_ADDR_LMODE_DATA          0x16c
#define XIC_CODEC_CTRL_BUS_BITS_LMODE_DATA          8
#define XIC_CODEC_CTRL_BUS_ADDR_NTCOMB_L_DATA       0x174
#define XIC_CODEC_CTRL_BUS_BITS_NTCOMB_L_DATA       64
#define XIC_CODEC_CTRL_BUS_ADDR_IFM_SQQ_DATA        0x180
#define XIC_CODEC_CTRL_BUS_BITS_IFM_SQQ_DATA        8
#define XIC_CODEC_CTRL_BUS_ADDR_INTERQ_DATA         0x188
#define XIC_CODEC_CTRL_BUS_BITS_INTERQ_DATA         8
#define XIC_CODEC_CTRL_BUS_ADDR_FMREOD_DATA         0x190
#define XIC_CODEC_CTRL_BUS_BITS_FMREOD_DATA         8
#define XIC_CODEC_CTRL_BUS_ADDR_RTOF_DATA           0x198
#define XIC_CODEC_CTRL_BUS_BITS_RTOF_DATA           16

#define ICCODEC_BASEADDR 0xA0000000
#define WEIGHT_BASE (0x60000000) //0x1C8800
#define BETA_BASE (0x601d0000) //0x820
#define IFM_SCALE_BASE (0x601e0000) //0xA00
#define K_SCALE_BASE (0x601f0000) //0xA00
#define OFM_SCALE_BASE (0x60200000) //0xA00
#define MEM_BASE (0x60300000) //0x1fa_4000

#define LT_CONV (0)
#define LT_CONVT (1)
#define LT_GDN (2)
#define LT_IGDN (3)

#define MAX_S 4
#define MAX_K 9
#define MAX_BETA_LENGTH 128
#define MAX_Tif 16
#define MAX_Tof 16
#define LANE_NUM 8

#define MAX_Poy 1
#define MAX_Pox 16

const uint32_t MAX_KxK = 128;
const uint32_t MAX_Trc = 512*(MAX_Pox*MAX_Poy);
const uint32_t MAX_IB_HW = 512;

#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))

#define WriteReg(BaseAddress, RegOffset, Data) *(volatile unsigned int*)((BaseAddress) + (RegOffset)) = (Data)
#define ReadReg(BaseAddress, RegOffset) *(volatile unsigned int*)((BaseAddress) + (RegOffset))

#define HPAGESIZE (4*1024)

void copy_mem2dev(uint8_t *orig,uint32_t byte_num, unsigned long in_buffer);

void copy_dev2mem(uint8_t *dst,uint32_t byte_num, unsigned long in_buffer);

int copy_file2mem(char *bin_file,uint32_t byte_num,unsigned long in_buffer);

int copy_mem2file(char *bin_file,uint32_t byte_num,unsigned long in_buffer);

double what_time_is_it_now();

int IC_codec(unsigned int  ofm_addr, unsigned int ifm_addr, unsigned int weight_offset, unsigned int bias_offset,
				unsigned int ifm_scale, unsigned int ofm_scale, unsigned int kernel_scale,
                int IF, int IR, int IC, int OF, int OR, int OC, int pad, int K, int S, int ltype, 
                uint8_t p_stride, uint8_t c_stride, uint8_t wb_mode, uint8_t ps_sf_bit, uint8_t ps_mask, bool sq_en,
                uint16_t TR, uint16_t TC, uint16_t TM, uint16_t TN, uint32_t OHW, uint8_t KxK, 
                uint32_t IHxIW, uint16_t p_stridexIR, uint16_t p_stridexIC, bool use_bias,
				uint16_t NToy, uint16_t NTox, uint16_t NTof, uint64_t NTcomb, uint16_t NTif, uint8_t lmode, uint64_t NTcomb_l,
				uint8_t ifm_sqQ, uint8_t interQ, uint8_t IsFMreod);

