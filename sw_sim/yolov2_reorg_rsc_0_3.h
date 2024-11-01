#ifndef YOLOV2_H

#define YOLOV2_H

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#ifndef FLT_MAX
#define FLT_MAX 3.402823466e+38F
#endif

typedef struct {
    int w;
    int h;
    int c;
    float *data;
} image;

void error(const char *s)
{
    perror(s);
    assert(0);
    exit(-1);
}

void malloc_error()
{
    fprintf(stderr, "Malloc error\n");
    exit(-1);
}

void file_error(char *s)
{
    fprintf(stderr, "Couldn't open file: %s\n", s);
    exit(0);
}

unsigned char *read_file(char *filename)
{
    FILE *fp = fopen(filename, "rb");
    size_t size;

    fseek(fp, 0, SEEK_END); 
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET); 

    unsigned char *text = (unsigned char *)calloc(size+1, sizeof(unsigned char));
    fread(text, 1, size, fp);
    fclose(fp);
    return text;
}

image make_empty_image(int w, int h, int c)
{
    image out;
    out.data = 0;
    out.h = h;
    out.w = w;
    out.c = c;
    return out;
}

image make_image(int w, int h, int c)
{
    image out = make_empty_image(w,h,c);
    out.data = (float *)calloc(h*w*c, sizeof(float));
    return out;
}

void free_image(image m)
{
    if(m.data){
        free(m.data);
    }
}

image load_image_stb(char *filename, int channels)
{
    int w, h, c;
    unsigned char *data = stbi_load(filename, &w, &h, &c, channels);
    if (!data) {
        fprintf(stderr, "Cannot load image \"%s\"\nSTB Reason: %s\n", filename, stbi_failure_reason());
        exit(0);
    }
    if(channels) c = channels;
    int i,j,k;
    image im = make_image(w, h, c);
    for(k = 0; k < c; ++k){
        for(j = 0; j < h; ++j){
            for(i = 0; i < w; ++i){
                int dst_index = i + w*j + w*h*k;
                int src_index = k + c*i + c*w*j;
                im.data[dst_index] = (float)data[src_index]/255.;
            }
        }
    }
    free(data);
    return im;
}

image load_image_stb_hwc(char *filename, int channels)
{
    int w, h, c;
    unsigned char *data = stbi_load(filename, &w, &h, &c, channels);
    if (!data) {
        fprintf(stderr, "Cannot load image \"%s\"\nSTB Reason: %s\n", filename, stbi_failure_reason());
        exit(0);
    }
    if(channels) c = channels;
    int i,j,k;
    image im = make_image(w, h, c);
    for(k = 0; k < c; ++k){
        for(j = 0; j < h; ++j){
            for(i = 0; i < w; ++i){
                int index = k + c*i + c*w*j;
                im.data[index] = (float)data[index]/255.;
            }
        }
    }
    free(data);
    return im;
}

void save_image_png(image im, const char *name)
{
    char buff[256];
    //sprintf(buff, "%s (%d)", name, windows);
    sprintf(buff, "%s.png", name);
	unsigned char *data = (unsigned char *)calloc(im.w*im.h*im.c, sizeof(char));
    int i,k;
    for(k = 0; k < im.c; ++k){
        for(i = 0; i < im.w*im.h; ++i){
            data[i*im.c+k] = (unsigned char) (255*im.data[i + k*im.w*im.h]);
        }
    }
    int success = stbi_write_png(buff, im.w, im.h, im.c, data, im.w*im.c);
    free(data);
    if(!success) fprintf(stderr, "Failed to write image %s\n", buff);
}

void copy_cpu(int N, float *X, int INCX, float *Y, int INCY)
{
    int i;
    for(i = 0; i < N; ++i) Y[i*INCY] = X[i*INCX];
}

void fill_cpu(int N, float ALPHA, float *X, int INCX)
{
    int i;
    for(i = 0; i < N; ++i) X[i*INCX] = ALPHA;
}

void add_bias(float *output, float *biases, int batch, int n, int size)
{
    int i,j,b;
    for(b = 0; b < batch; ++b){
        for(i = 0; i < n; ++i){
            for(j = 0; j < size; ++j){
                output[(b*n + i)*size + j] += biases[i];
            }
        }
    }
}

void scale_bias(float *output, float *scales, int batch, int n, int size)
{
    int i,j,b;
    for(b = 0; b < batch; ++b){
        for(i = 0; i < n; ++i){
            for(j = 0; j < size; ++j){
                output[(b*n + i)*size + j] *= scales[i];
            }
        }
    }
}

float im2col_get_pixel(float *im, int height, int width, int channels,
                        int row, int col, int channel, int pad)
{
    row -= pad;
    col -= pad;

    if (row < 0 || col < 0 ||
        row >= height || col >= width) return 0;
    return im[col + width*(row + height*channel)];
}

//From Berkeley Vision's Caffe!
//https://github.com/BVLC/caffe/blob/master/LICENSE
void im2col_cpu(float* data_im,
     int channels,  int height,  int width,
     int ksize,  int stride, int pad, float* data_col) 
{
    int c,h,w;
    int height_col = (height + 2*pad - ksize) / stride + 1;
    int width_col = (width + 2*pad - ksize) / stride + 1;

    int channels_col = channels * ksize * ksize;
    for (c = 0; c < channels_col; ++c) {
        int w_offset = c % ksize;
        int h_offset = (c / ksize) % ksize;
        int c_im = c / ksize / ksize;
        for (h = 0; h < height_col; ++h) {
            for (w = 0; w < width_col; ++w) {
                int im_row = h_offset + h * stride;
                int im_col = w_offset + w * stride;
                int col_index = (c * height_col + h) * width_col + w;
                data_col[col_index] = im2col_get_pixel(data_im, height, width, channels,
                        im_row, im_col, c_im, pad);
            }
        }
    }
}

void softmax(float *input, int n, float temp, int stride, float *output)
{
    int i;
    float sum = 0;
    float largest = -FLT_MAX;
    for(i = 0; i < n; ++i){
        if(input[i*stride] > largest) largest = input[i*stride];
    }
    for(i = 0; i < n; ++i){
        float e = exp(input[i*stride]/temp - largest/temp);
        sum += e;
        output[i*stride] = e;
    }
    for(i = 0; i < n; ++i){
        output[i*stride] /= sum;
    }
}

void softmax_cpu(float *input, int n, int batch, int batch_offset, int groups, int group_offset, int stride, float temp, float *output)
{
    int g, b;
    for(b = 0; b < batch; ++b){
        for(g = 0; g < groups; ++g){
            softmax(input + b*batch_offset + g*group_offset, n, temp, stride, output + b*batch_offset + g*group_offset);
        }
    }
}

//////////////////////image begin
image copy_image(image p)
{
    image copy = p;
    copy.data = (float *)calloc(p.h*p.w*p.c, sizeof(float));
    memcpy(copy.data, p.data, p.h*p.w*p.c*sizeof(float));
    return copy;
}

image float_to_image(int w, int h, int c, float *data)
{
    image out = make_empty_image(w,h,c);
    out.data = data;
    return out;
}

image threshold_image(image im, float thresh)
{
    int i;
    image t = make_image(im.w, im.h, im.c);
    for(i = 0; i < im.w*im.h*im.c; ++i){
        t.data[i] = im.data[i]>thresh ? 1 : 0;
    }
    return t;
}

int diff_float(float *in0, float *in1, int data_num, float err_max){
	int diff_num = 0;
	float diff_max = 0.0f;
	for(int k = 0; k < data_num; ++k){
		float diff = abs(in0[k] - in1[k]);
		if(diff > err_max){
			// printf("%2.10lf,%2.10lf diff.\n", in0[k], in1[k]);
			diff_num++;
			if(diff > diff_max)
				diff_max = diff;
		}
    }
	printf("max diff = %2.10lf, map2[0,255]=%d, diff_num=%d\n", diff_max, (int)(diff_max*255.), diff_num);
	return diff_num;
}

int diff_int32(int32_t *in0, int32_t *in1, int data_num, int32_t err_max){
	int diff_num = 0;
	int32_t diff_max = 0;
	for(int k = 0; k < data_num; ++k){
		int32_t diff = abs(in0[k] - in1[k]);
		if(diff > err_max){
			// printf("%2.10lf,%2.10lf diff.\n", in0[k], in1[k]);
			diff_num++;
			if(diff > diff_max)
				diff_max = diff;
		}
    }
	printf("max diff = %d\n", diff_max);
	return diff_num;
}

void NCHW2NHWC(float *in0, float *in1, int c, int h, int w){
	for(int k = 0; k < c; ++k)
        for(int j = 0; j < h; ++j)
            for(int i = 0; i < w; ++i){
                int src_index = i + w*j + w*h*k;
                int dst_index = k + c*i + c*w*j;
                in0[dst_index] = (float)in1[src_index];
            }
}

void NHWC2NCHW(float *in0, float *in1, int c, int h, int w){
	for(int k = 0; k < c; ++k)
        for(int j = 0; j < h; ++j)
            for(int i = 0; i < w; ++i){
                int dst_index = i + w*j + w*h*k;
                int src_index = k + c*i + c*w*j;
                in0[dst_index] = (float)in1[src_index];
            }
}

void square(float *out, float *in, int data_num){
	for(int k = 0; k < data_num; ++k){
		out[k] = in[k]*in[k];
    }	
}

void rsqrt_mul(float *in0, float *in1, int data_num){
	for(int k = 0; k < data_num; ++k){
		in0[k] = in1[k]/sqrt(in0[k]);
    }
}

void sqrt_mul(float *in0, float *in1, int data_num){
	for(int k = 0; k < data_num; ++k){
		in0[k] = in1[k]*sqrt(in0[k]);
    }
}

void wb_comb(float *dst0, float *src0, float *src1, int data_num, int mode){//mode=0 normal, 1=src0*sqrt(src1), 2=src0/sqrt(src1)
	for(int k = 0; k < data_num; ++k){

        float tmp_in, tmp_in0, tmp1, tmp_out;

		// dst0[k] = in1[k]/sqrt(in0[k]);
        tmp_in = src0[k];
        if(mode==0){
            tmp_out = tmp_in;
        }else{
            tmp_in0 = src1[k];
            tmp1 = sqrt(tmp_in);
            if(mode==1){
                tmp_out = tmp_in0*tmp1;
            }else{
                tmp_out = tmp_in0/tmp1;
                assert(mode==2);
            }            
        }

        dst0[k] = tmp_out;
    }
}

void wb_comb_b_chw(float *dst0, float *src0, float *src1, float *bias, int OF, int OHW, int mode, bool bias_en){//mode=0 normal, 1=src0*sqrt(src1), 2=src0/sqrt(src1)
	for(int c = 0; c < OF; ++c)
    for(int k = 0; k < OHW; ++k){
        float tmp_in, tmp_in0, tmp1, tmp_out;

        int idx = c*OHW + k;
        tmp_in = src0[idx];
        if(bias_en){
            tmp_in = tmp_in + bias[c];
        }

        if(mode==0){
            tmp_out = tmp_in;
        }else{
            tmp_in0 = src1[idx];
            tmp1 = sqrt(tmp_in);
            if(mode==1){
                tmp_out = tmp_in0*tmp1;
            }else{
                tmp_out = tmp_in0/tmp1;
                assert(mode==2);
            }            
        }

        dst0[idx] = tmp_out;
    }
}

void fm_padding(float *in0, float *in1, int h, int w, int c, int pad){
	int h_pad = h+pad*2;
	int w_pad = w+pad*2;
	for(int k = 0; k < c; ++k)
        for(int j = 0; j < h_pad; ++j)
            for(int i = 0; i < w_pad; ++i){
				bool y_en = ((j - pad) >= 0)&&((j - pad) <h);
				bool x_en = ((i - pad) >= 0)&&((i - pad) <w);
				bool px_en = y_en && x_en;
                int src_index = k + c*(i-pad) + c*w*(j-pad);
                int dst_index = k + c*i + c*w_pad*j;
				if(px_en)
                	in0[dst_index] = in1[src_index];
				else
					in0[dst_index] = 0;
            }
}

void fm_padding_chw(float *in0, float *in1, int h, int w, int c, int pad){
	int h_pad = h+pad*2;
	int w_pad = w+pad*2;
	for(int k = 0; k < c; ++k)
        for(int j = 0; j < h_pad; ++j)
            for(int i = 0; i < w_pad; ++i){
				bool y_en = ((j - pad) >= 0)&&((j - pad) <h);
				bool x_en = ((i - pad) >= 0)&&((i - pad) <w);
				bool px_en = y_en && x_en;
                // int src_index = k + c*(i-pad) + c*w*(j-pad);
                // int dst_index = k + c*i + c*w_pad*j;
                int src_index = k*h*w + (j-pad)*w + (i-pad);
                int dst_index = k*h_pad*w_pad + j*w_pad + i;                
				if(px_en)
                	in0[dst_index] = in1[src_index];
				else
					in0[dst_index] = 0;
            }
}

void fm_padding_comb_chw(float *in0, float *in1, int h, int w, int c, int pad, int stride, int mode){//mode=0 normal, mode=1 expand

	int h_pad, w_pad;
    if(mode==1){
        h_pad = h*stride+pad*2;
        w_pad = w*stride+pad*2;
    }else{
	    h_pad = h+pad*2;
	    w_pad = w+pad*2;
        assert(mode==0);        
    }

	for(int k = 0; k < c; ++k)
        for(int j = 0; j < h_pad; ++j)
            for(int i = 0; i < w_pad; ++i){

                int src_index, dst_index;
                bool y_en, x_en;
                if(mode==1){
                    y_en = ((j - pad) >= 0)&&((j - pad) <stride*h)&&((j - pad)%stride==0);
                    x_en = ((i - pad) >= 0)&&((i - pad) <stride*w)&&((i - pad)%stride==0);

                    src_index = k*h*w + (i-pad)/stride + w*(j-pad)/stride;
                    dst_index = k*h_pad*w_pad + i + w_pad*j;
                }else{
                    y_en = ((j - pad) >= 0)&&((j - pad) <h);
                    x_en = ((i - pad) >= 0)&&((i - pad) <w);

                    src_index = k*h*w + (j-pad)*w + (i-pad);
                    dst_index = k*h_pad*w_pad + j*w_pad + i;        
                }                
                
                bool px_en = y_en && x_en;               
				if(px_en)
                	in0[dst_index] = in1[src_index];
				else
					in0[dst_index] = 0;
            }
}

void fm_padding_comb_sq_chw(float *in0, float *in1, int h, int w, int c, int pad, int h_pad, int w_pad, int stride, int mode, bool sq_en){//mode=0 normal, mode=1 expand

	for(int k = 0; k < c; ++k)
        for(int j = 0; j < h_pad; ++j)
            for(int i = 0; i < w_pad; ++i){

                int src_index, dst_index;
                bool y_en, x_en;
                if(mode==1){
                    y_en = ((j - pad) >= 0)&&((j - pad) <stride*h)&&((j - pad)%stride==0);
                    x_en = ((i - pad) >= 0)&&((i - pad) <stride*w)&&((i - pad)%stride==0);
                    src_index = k*h*w + (i-pad)/stride + w*(j-pad)/stride;
                }else{
                    y_en = ((j - pad) >= 0)&&((j - pad) <h);
                    x_en = ((i - pad) >= 0)&&((i - pad) <w);
                    src_index = k*h*w + (j-pad)*w + (i-pad);      
                }                
                dst_index = k*h_pad*w_pad + j*w_pad + i;

                bool px_en = y_en && x_en;
                float tmp_in, tmp_out;                               
				if(px_en){
                    tmp_in = in1[src_index];
                    if(sq_en)
                        tmp_out = tmp_in*tmp_in;
                    else
                        tmp_out = tmp_in;
                }
				else
					tmp_out = 0;
                
                in0[dst_index] = tmp_out;
            }
}

void fm_padding_comb_sq_chw(float *in0, float *in1, int h, int w, int c, int pad, int stride, int mode, bool sq_en){//mode=0 normal, mode=1 expand

	int h_pad, w_pad;
    if(mode==1){
        h_pad = h*stride+pad*2;
        w_pad = w*stride+pad*2;
    }else{
	    h_pad = h+pad*2;
	    w_pad = w+pad*2;
        assert(mode==0);        
    }

	for(int k = 0; k < c; ++k)
        for(int j = 0; j < h_pad; ++j)
            for(int i = 0; i < w_pad; ++i){

                int src_index, dst_index;
                bool y_en, x_en;
                if(mode==1){
                    y_en = ((j - pad) >= 0)&&((j - pad) <stride*h)&&((j - pad)%stride==0);
                    x_en = ((i - pad) >= 0)&&((i - pad) <stride*w)&&((i - pad)%stride==0);

                    src_index = k*h*w + (i-pad)/stride + w*(j-pad)/stride;
                    dst_index = k*h_pad*w_pad + i + w_pad*j;
                }else{
                    y_en = ((j - pad) >= 0)&&((j - pad) <h);
                    x_en = ((i - pad) >= 0)&&((i - pad) <w);

                    src_index = k*h*w + (j-pad)*w + (i-pad);
                    dst_index = k*h_pad*w_pad + j*w_pad + i;        
                }                
                
                bool px_en = y_en && x_en;
                float tmp_in, tmp_out;                               
				if(px_en){
                    tmp_in = in1[src_index];
                    if(sq_en)
                        tmp_out = tmp_in*tmp_in;
                    else
                        tmp_out = tmp_in;
                }
				else
					tmp_out = 0;
                
                in0[dst_index] = tmp_out;
            }
}

void conv2d(float *ifm, float *ofm, float *kernel, float *bias, int OR, int OC, int IR, int IC, int OF, int IF, int K, int S, bool use_bias){//NHWC, kernel KKNM
	for(int r=0; r<OR; r++)
	for(int c=0; c<OC; c++)
	for(int m=0; m<OF; m++){
		float tmp = 0.0f;
		for(int n=0; n<IF; n++)
		for(int ky=0; ky<K; ky++)
		for(int kx=0; kx<K; kx++){
			int ifm_idx = (S*r+ky)*IC*IF + (S*c+kx)*IF + n;//NHWC
			int k_idx = ky*K*IF*OF + kx*IF*OF + n*OF + m;//KKNM
			tmp += kernel[k_idx]*ifm[ifm_idx];
		}
		if(use_bias)
			tmp += bias[m];
		
		int out_idx = r*OC*OF + c*OF + m;//NHWC
		ofm[out_idx] = tmp;
	}
}

void conv2d_chw(float *ifm, float *ofm, float *kernel, float *bias, int OR, int OC, int IR, int IC, int OF, int IF, int K, int S, bool use_bias){//NHWC, kernel KKNM
	for(int r=0; r<OR; r++)
	for(int c=0; c<OC; c++)
	for(int m=0; m<OF; m++){
		float tmp = 0.0f;
		for(int n=0; n<IF; n++)
		for(int ky=0; ky<K; ky++)
		for(int kx=0; kx<K; kx++){
			int ifm_idx = (S*r+ky)*IC + (S*c+kx) + n*IR*IC;//NHWC
			int k_idx = ky*K*IF*OF + kx*IF*OF + n*OF + m;//KKNM
			tmp += kernel[k_idx]*ifm[ifm_idx];
		}
		if(use_bias)
			tmp += bias[m];
		
		int out_idx = m*OR*OC + r*OC + c;//NHWC
		ofm[out_idx] = tmp;
	}
}

void conv2d_chw(float *ifm, float *ofm, float *kernel, int OR, int OC, int IR, int IC, int OF, int IF, int K, int S){//NHWC, kernel KKNM
	for(int r=0; r<OR; r++)
	for(int c=0; c<OC; c++)
	for(int m=0; m<OF; m++){
		float tmp = 0.0f;
		for(int n=0; n<IF; n++)
		for(int ky=0; ky<K; ky++)
		for(int kx=0; kx<K; kx++){
			int ifm_idx = (S*r+ky)*IC + (S*c+kx) + n*IR*IC;//NHWC
			int k_idx = ky*K*IF*OF + kx*IF*OF + n*OF + m;//KKNM
			tmp += kernel[k_idx]*ifm[ifm_idx];
		}
		
		int out_idx = m*OR*OC + r*OC + c;//NHWC
		ofm[out_idx] = tmp;
	}
}

void conv2d_skip0(float *ifm, float *ofm, float *kernel, float *bias, int OR, int OC, int IR, int IC, int OF, int IF, int K, int S, bool use_bias){//NHWC, kernel KKNM
	     
    for(int r=0; r<OR; r++)
	for(int c=0; c<OC; c++)
	for(int m=0; m<OF; m++){
		float tmp = 0.0f;
		for(int n=0; n<IF; n++)
		for(int ky=0; ky<K; ky++)
		for(int kx=0; kx<K; kx++){
			int ifm_idx = (S*r+ky)*IC*IF + (S*c+kx)*IF + n;//NHWC
			int k_idx = ky*K*IF*OF + kx*IF*OF + n*OF + m;//KKNM
            float tmp_ifm = ifm[ifm_idx];
            if(tmp_ifm!=0)
                tmp += kernel[k_idx]*tmp_ifm;
		}
		if(use_bias)
			tmp += bias[m];
		
		int out_idx = r*OC*OF + c*OF + m;//NHWC
		ofm[out_idx] = tmp;
	}
}

void conv2d_skip0_chw(float *ifm, float *ofm, float *kernel, float *bias, int OR, int OC, int IR, int IC, int OF, int IF, int K, int S, bool use_bias){//NHWC, kernel KKNM
	     
    for(int r=0; r<OR; r++)
	for(int c=0; c<OC; c++)
	for(int m=0; m<OF; m++){
		float tmp = 0.0f;
		for(int n=0; n<IF; n++)
		for(int ky=0; ky<K; ky++)
		for(int kx=0; kx<K; kx++){
			// int ifm_idx = (S*r+ky)*IC*IF + (S*c+kx)*IF + n;//NHWC
			int ifm_idx = (S*r+ky)*IC + (S*c+kx) + n*IR*IC;//            
			int k_idx = ky*K*IF*OF + kx*IF*OF + n*OF + m;//KKNM
            float tmp_ifm = ifm[ifm_idx];
            if(tmp_ifm!=0)
                tmp += kernel[k_idx]*tmp_ifm;
		}
		if(use_bias)
			tmp += bias[m];
		
		int out_idx = r*OC + c + m*OR*OC;//NHWC
		ofm[out_idx] = tmp;
	}
}

void GDN(float *ifm, float *ofm, float *kernel, float *bias, int OR, int OC, int IR, int IC, int OF, int IF, int K, int S, bool use_bias){
	int ofm_data_num = OR*OC*OF;
	float *ifm_square = (float *)malloc(sizeof(float)*ofm_data_num);
	square(ifm_square, ifm, ofm_data_num);//x^2
	conv2d(ifm_square, ofm, kernel, bias, OR, OC, IR, IC, OF, IF, K, S, use_bias);//y=conv1x1(x^2)
	rsqrt_mul(ofm, ifm, ofm_data_num);//y_bar = x/sqrt(y);
	free(ifm_square);
}

void GDN_chw(float *ifm, float *ofm, float *kernel, float *bias, int OR, int OC, int IR, int IC, int OF, int IF, int K, int S, bool use_bias){
	int ofm_data_num = OR*OC*OF;
	float *ifm_square = (float *)malloc(sizeof(float)*ofm_data_num);
	square(ifm_square, ifm, ofm_data_num);//x^2
	conv2d_chw(ifm_square, ofm, kernel, bias, OR, OC, IR, IC, OF, IF, K, S, use_bias);//y=conv1x1(x^2)
	rsqrt_mul(ofm, ifm, ofm_data_num);//y_bar = x/sqrt(y);
	free(ifm_square);
}

void IGDN(float *ifm, float *ofm, float *kernel, float *bias, int OR, int OC, int IR, int IC, int OF, int IF, int K, int S, bool use_bias){
	int ofm_data_num = OR*OC*OF;
	float *ifm_square = (float *)malloc(sizeof(float)*ofm_data_num);
	square(ifm_square, ifm, ofm_data_num);
	conv2d(ifm_square, ofm, kernel, bias, OR, OC, IR, IC, OF, IF, K, S, use_bias);
	sqrt_mul(ofm, ifm, ofm_data_num);
	free(ifm_square);
}

void IGDN_chw(float *ifm, float *ofm, float *kernel, float *bias, int OR, int OC, int IR, int IC, int OF, int IF, int K, int S, bool use_bias){
	int ofm_data_num = OR*OC*OF;
	float *ifm_square = (float *)malloc(sizeof(float)*ofm_data_num);
	square(ifm_square, ifm, ofm_data_num);
	conv2d_chw(ifm_square, ofm, kernel, bias, OR, OC, IR, IC, OF, IF, K, S, use_bias);
	sqrt_mul(ofm, ifm, ofm_data_num);
	free(ifm_square);
}

void fm_padding_s(float *in0, float *in1, int h, int w, int c, int pad, int stride){
	int h_pad = h*stride+pad*2;
	int w_pad = w*stride+pad*2;
	for(int k = 0; k < c; ++k)
        for(int j = 0; j < h_pad; ++j)
            for(int i = 0; i < w_pad; ++i){
				bool y_en = ((j - pad) >= 0)&&((j - pad) <stride*h)&&((j - pad)%stride==0);
				bool x_en = ((i - pad) >= 0)&&((i - pad) <stride*w)&&((i - pad)%stride==0);
				bool px_en = y_en && x_en;
                int src_index = k + c*(i-pad)/stride + c*w*(j-pad)/stride;
                int dst_index = k + c*i + c*w_pad*j;
				if(px_en)
                	in0[dst_index] = in1[src_index];
				else
					in0[dst_index] = 0;
            }
}

void fm_padding_s_chw(float *in0, float *in1, int h, int w, int c, int pad, int stride){
	int h_pad = h*stride+pad*2;
	int w_pad = w*stride+pad*2;
	for(int k = 0; k < c; ++k)
        for(int j = 0; j < h_pad; ++j)
            for(int i = 0; i < w_pad; ++i){
				bool y_en = ((j - pad) >= 0)&&((j - pad) <stride*h)&&((j - pad)%stride==0);
				bool x_en = ((i - pad) >= 0)&&((i - pad) <stride*w)&&((i - pad)%stride==0);
				bool px_en = y_en && x_en;
                // int src_index = k + c*(i-pad)/stride + c*w*(j-pad)/stride;
                // int dst_index = k + c*i + c*w_pad*j;
                int src_index = k*h*w + (i-pad)/stride + w*(j-pad)/stride;
                int dst_index = k*h_pad*w_pad + i + w_pad*j;                
				if(px_en)
                	in0[dst_index] = in1[src_index];
				else
					in0[dst_index] = 0;
            }
}

void kernel_rot180(float *in0, float *in1, int K, int IF, int OF){
	float *tmp_KK = (float *)malloc(sizeof(float)*K*K);
	for(int m=0; m<OF; m++)
	for(int n=0; n<IF; n++){
		for(int ky=0; ky<K; ky++)
		for(int kx=0; kx<K; kx++){
			int k_idx = ky*K*IF*OF + kx*IF*OF + n*OF + m;//KKNM
			tmp_KK[ (K-1-ky)*K + (K-1-kx)] = in1[k_idx];
		}

		for(int ky=0; ky<K; ky++)
		for(int kx=0; kx<K; kx++){
			int k_idx = ky*K*IF*OF + kx*IF*OF + n*OF + m;//KKNM
			in0[k_idx] = tmp_KK[ ky*K + kx];
		}		
	}

	free(tmp_KK);
}

void save_image_png_hwc(float *in0, int h, int w, int c, const char *name)
{
    char buff[256];
    //sprintf(buff, "%s (%d)", name, windows);
    sprintf(buff, "%s.png", name);
	unsigned char *data = (unsigned char *)calloc(w*h*c, sizeof(char));
    int i,k;
    for(k = 0; k < c; ++k){
        for(i = 0; i < w*h; ++i){
			float tmp_in = in0[i*c+k];
			if(tmp_in < 0.0f)
				tmp_in = 0.0f;
			else if(tmp_in > 1.0f)
				tmp_in = 1.0f;
			unsigned char tmp_out = floor(255*tmp_in + 0.5);
            data[i*c+k] = tmp_out;
        }
    }
    int success = stbi_write_png(buff, w, h, c, data, w*c);
    free(data);
    if(!success) fprintf(stderr, "Failed to write image %s\n", buff);
}

void save_image_png_chw(float *in0, int h, int w, int c, const char *name)
{
    char buff[256];
    //sprintf(buff, "%s (%d)", name, windows);
    sprintf(buff, "%s.png", name);
	unsigned char *data = (unsigned char *)calloc(w*h*c, sizeof(char));
    int i,k;
    for(k = 0; k < c; ++k){
        for(i = 0; i < w*h; ++i){
			float tmp_in = in0[k*w*h + i];
			if(tmp_in < 0.0f)
				tmp_in = 0.0f;
			else if(tmp_in > 1.0f)
				tmp_in = 1.0f;
			unsigned char tmp_out = floor(255*tmp_in + 0.5);
            data[i*c+k] = tmp_out;
        }
    }
    int success = stbi_write_png(buff, w, h, c, data, w*c);
    free(data);
    if(!success) fprintf(stderr, "Failed to write image %s\n", buff);
}

double what_time_is_it_now()
{
    struct timeval time;
    if (gettimeofday(&time,NULL)){
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
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

int read_binfile_int32_rb(int32_t *buf, char *filename, int data_num)
{
	FILE *fp;
	if( (fp = fopen(filename, "rb")) == NULL)
		printf("cannot open bin_file %s\n", filename);
	int rd_num  = fread(buf, sizeof(int32_t), data_num, fp);
	fclose(fp);

	return rd_num;
}

long int GetFileSize(char *fn){
	FILE * pFile = fopen (fn,"rb");
	if (pFile==NULL){
		printf("Error opening file\n");
		return -1;
	}
	else
	{
		fseek (pFile, 0, SEEK_END);
		long int size = ftell(pFile);
		fclose (pFile);
		return size;
	}
	return 0;
}

void syn_add_channel_median_hwc(float *in0, float *median, int h, int w, int c){

	for(int k = 0; k < c; ++k)
        for(int j = 0; j < h; ++j)
            for(int i = 0; i < w; ++i){
                int index = k + c*i + c*w*j;
                in0[index] = in0[index] + median[k];
    }
}

void syn_add_channel_median_chw(float *in0, float *median, int h, int w, int c){

	for(int k = 0; k < c; ++k)
        for(int j = 0; j < h; ++j)
            for(int i = 0; i < w; ++i){
                // int index = k + c*i + c*w*j;
                int index = k*h*w + i + w*j;
                in0[index] = in0[index] + median[k];
    }
}

void i32tof32_1d(float *dst, int32_t *src, int data_num){

	for(int k = 0; k < data_num; ++k){
        dst[k] = (float)(src[k]);
    }
}

void INT32_to_FT32_hwc(float *in0, int32_t *in1, int h, int w, int c){

	for(int k = 0; k < c; ++k)
        for(int j = 0; j < h; ++j)
            for(int i = 0; i < w; ++i){
                int index = k + c*i + c*w*j;
                in0[index] = (float)(in1[index]);
    }
}

void FT32_to_INT32_hwc(int32_t *in0, float *in1, int h, int w, int c){

	for(int k = 0; k < c; ++k)
        for(int j = 0; j < h; ++j)
            for(int i = 0; i < w; ++i){
                int index = k + c*i + c*w*j;
                in0[index] = in1[index];
    }
}

void ana_a05_sub_median_chw(int32_t *in0, float* in1, float *median, int h, int w, int c){

	for(int k = 0; k < c; ++k)
        for(int j = 0; j < h; ++j)
            for(int i = 0; i < w; ++i){
                int index = k*h*w + i + w*j;
                in0[index] = floor(in1[index] + 0.5 - median[k]);
    }
}

void ana_a05_sub_median_hwc(int32_t *in0, float* in1, float *median, int h, int w, int c){

	for(int k = 0; k < c; ++k)
        for(int j = 0; j < h; ++j)
            for(int i = 0; i < w; ++i){
                int index = k + c*i + c*w*j;
                in0[index] = floor(in1[index] + 0.5 - median[k]);
    }
}

template <typename T>
int diff_set(T *in0, T *in1, int data_num, double err_max, int idx, bool init_en, bool exit_en){

    float tmp_dataQ = pow(0.5, 14);

    static int idx_val;
    static double diff_sum_min, diff_max_min;
    if(init_en){
        idx_val = -1;
        diff_sum_min = 65536;
    }

    int diff_num = 0;
    float diff_max = 0.0f;
    double diff_sum = 0.0f;
    for(int k = 0; k < data_num; ++k){
            float diff = abs(in0[k]*tmp_dataQ - in1[k]*tmp_dataQ);
            if(diff > err_max){
                // printf("%2.10lf,%2.10lf diff.\n", in0[k], in1[k]);
                diff_num++;
                if(diff > diff_max)
                    diff_max = diff;
            }
            diff_sum += diff;
    }
    printf("[%d]: diff max= %2.10lf, diff sum = %3.7lf\n", idx, diff_max, diff_sum);

    if(init_en){
        diff_sum_min = diff_sum;
        idx_val = idx;
        diff_max_min = diff_max;        
    }

    if(diff_sum < diff_sum_min){
        diff_sum_min = diff_sum;
        idx_val = idx;
        diff_max_min = diff_max;          
    }

    if(exit_en){
        printf("[Q=%d]: diff max= %2.10lf, diff sum_min = %3.7lf\n", idx_val, diff_max_min, diff_sum_min);
    }

    return idx_val;
}

#define LT_CONV (0)
#define LT_CONVT (1)
#define LT_GDN (2)
#define LT_IGDN (3)

#include "acc_new_rsc_4_3.h"

void w_sr(int16_t *data, int data_num, int8_t sr_bit){
    for(int x=0; x<data_num; x++){
        data[x] = data[x] >> sr_bit;
    }
}

int quantize_ifm_i16c_scale(float *in,float *out,int wh_num, int c_num, int lane_num, float *scale_array)
{

const float MIN_VALUE = (-1024*1024*1024);
const float MAX_VALUE = (1024*1024*1024);

	printf("Layer[x]'s fm, c=%12d, hw=%12d ", c_num, wh_num);
	for(int j=0; j<c_num; j++){
	    float max_abs = 0.;
        for(int i=0; i<wh_num; i++)
        {
            float tmp_in_float = in[((j/lane_num)*wh_num + i)*lane_num + (j & (lane_num-1))];
            tmp_in_float = abs(tmp_in_float);
            if(tmp_in_float>max_abs)
                max_abs = tmp_in_float;
        }
        scale_array[j] = max_abs;
        // printf("[c:%d]float min=%.7lf,max=%.7lf ",j,min,max);//find float min max
    }

	int maxQ = 14;

	double max_error,min_error,sum_error;
	sum_error = 0;
	max_error = MIN_VALUE;
	min_error = MAX_VALUE;
	for(int j=0; j<c_num; j++)
    for(int i=0; i<wh_num; i++)    
	{
        int idx = ((j/lane_num)*wh_num + i)*lane_num + (j & (lane_num-1));
		float tmp_in_float = in[idx];
        float tmp_in_f_s = tmp_in_float/scale_array[j];
		short tmp_fixed = (short)(tmp_in_f_s*pow(2.0,maxQ));
		float tmp_out_f_s = (float)tmp_fixed*pow(2.0,-maxQ);
        float tmp_out_float = tmp_out_f_s*scale_array[j];
		double error = (tmp_out_float - tmp_in_float)*(tmp_out_float - tmp_in_float);
		error = sqrt(error);
		sum_error += error;
		if(error<min_error)
			min_error = error;
		if(error>max_error)
			max_error = error;

		out[idx] = tmp_out_float;
	}
	printf("sum2_error = %.7lf,min_error=%.7lf,max_error=%.7lf",sum_error,min_error,max_error);
	printf("\n");


	return 0;
}

// int quantize_ifm_int16(float *in,float *out,int offset,float *ap16_range,int *maxQ_array)
// {

// const float MIN_VALUE = (-1024*1024*1024);
// const float MAX_VALUE = (1024*1024*1024);

// 	printf("Layer[x]'s ofm; num=%12d ", offset);
// 	int j;
// 	float min,max;
// 	min = MAX_VALUE;
// 	max = MIN_VALUE;
// 	for(j=0;j<offset;j++)
// 	{
// 		float tmp_in_float = in[j];
// 		if(tmp_in_float<min)
// 			min = tmp_in_float;
// 		if(tmp_in_float>max)
// 			max = tmp_in_float;
// 	}
// 	printf("float min=%.7lf,max=%.7lf ",min,max);//find float min max

// 	int maxQ = -1;
// 	for(j=0;j<16;j++)//find maxQ
// 	{
// 		if(min>ap16_range[2*j]&&max<ap16_range[2*j+1])
// 		{
// 			maxQ = j;
// 		}
// 		else if(j==0)
// 		{
// 			printf("beyond Q0 min=%.7lf,max=%.7lf ",min,max);
// 			break;
// 		}
// 	}
	
// 	*maxQ_array = maxQ;
// 	printf("maxQ=%d ", maxQ);

// 	double max_error,min_error,sum_error;
// 	sum_error = 0;
// 	max_error = MIN_VALUE;
// 	min_error = MAX_VALUE;
// 	for(j=0;j<offset;j++)
// 	{
// 		float tmp_in_float = in[j];
// 		short tmp_fixed = (short)(tmp_in_float*pow(2.0,maxQ));
// 		float tmp_out_float = (float)tmp_fixed*pow(2.0,-maxQ);
// 		double error = (tmp_out_float - tmp_in_float)*(tmp_out_float - tmp_in_float);
// 		error = sqrt(error);
// 		sum_error += error;
// 		if(error<min_error)
// 			min_error = error;
// 		if(error>max_error)
// 			max_error = error;

// 		out[j] = tmp_out_float;
// 	}
// 	printf("sum2_error = %.7lf,min_error=%.7lf,max_error=%.7lf",sum_error,min_error,max_error);
// 	printf("\n");


// 	return 0;
// }

#endif

