
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <hls_math.h>
#include <math.h>
#include "ap_int.h"

#define LT_CONV (0)
#define LT_CONVT (1)
#define LT_GDN (2)
#define LT_IGDN (3)

#define MAX(x,y) ((x)>(y)?(x):(y))
#define MIN(x,y) ((x)<(y)?(x):(y))

#define PRAGMA_SUB(x) _Pragma (#x)
#define DO_PRAGMA(x) PRAGMA_SUB(x)

#define MAX_S 4
#define MAX_K 9
#define MAX_BETA_LENGTH 128

#define LANE_NUM (8)

#define MAX_Tif 16
#define MAX_Tof 16
#define MAX_Pif (4)
#define MAX_Pof (16)
#define MAX_Poy (1)
#define MAX_Pox (16)

const uint32_t MAX_KxK = 128;
const uint32_t MAX_Trc = 512*(MAX_Pox*MAX_Poy);
const uint32_t MAX_IB_HW = 512;
const uint32_t Tmax_dx = (MAX_Tof+LANE_NUM-1)/LANE_NUM;
const uint32_t Tnax_dx = (MAX_Tif+LANE_NUM-1)/LANE_NUM;
const uint32_t Betanx_dx = (MAX_BETA_LENGTH+LANE_NUM-1)/LANE_NUM;

const uint32_t Np_rd = 2;
const uint32_t Np_wr = 2;
const uint32_t Np_rd_k = 2;
const uint32_t Tm_fm_w_d2 = Tmax_dx/Np_wr;
const uint32_t Tm_fm_d4 = Tmax_dx/Np_rd_k;
const uint32_t Tn_fm_d4 = Tnax_dx/Np_rd;

typedef ap_uint<16*LANE_NUM> DT_IO;
