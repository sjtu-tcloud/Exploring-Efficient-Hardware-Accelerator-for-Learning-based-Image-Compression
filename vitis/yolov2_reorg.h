#ifndef YOLOV2_H

#define YOLOV2_H

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
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

#include "acc_new_reorg.h"

#endif

