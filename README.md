# Exploring-Efficient-Hardware-Accelerator-for-Learning-based-Image-Compression

X-LIC is a framework to efficiently explore the design spaces of LIC accelerators. Here, we provide an FPGA demo of LIC design in INT16 precision.

This FPGA demo includes:
* Training and Inference Settings
* The python script of Model Extraction
* Quantization and software-simulation
* HLS Design
* Hardware/Software project for target ARM+FPGA platform

## Training and Inference Settings
Training framework is **Tensorflow-2.13** with **[Tensorflow-compression](https://github.com/tensorflow/compression)** library, and training settings follow [default configuration](https://github.com/tensorflow/compression/blob/master/models/bls2017.py). 
We use E2E model [[1](https://arxiv.org/abs/1611.01704)] to evaluate our framework.
The evaluation platform is Xilinx's ARM+FPGA MPSoC **ZCU102**, and toolchain version is Vivado/Vivado HLS/Vitis **v2019.2**.

**Training in Tensorflow**
```  
nohup python3 bls2017.py -V train --train_glob '/xxx/train_dataset/*.png' --batchsize 16 --epochs 200 --lambda 0.125 > train.log 2>&1 &
cat train.log | tail -n 50
```
Default model path is **bls2017**, we move this repo to **bls2017_0xxxx** for different lambda params. Generally, bigger lambda results in higher bpp & PSNR. In **bls2017_0125**, we provide pre-train model with lambda=0.125.

**Inference in Tensorflow**
``` 
# Inference_encoder
python3 bls2017.py --model_path bls2017 compress abigail-keenan-27293.png

# Inference_decoder
python3 bls2017.py --model_path bls2017 decompress abigail-keenan-27293.png.tfci rec.png
``` 
We use abigail-keenan-27293.png from [CLIC traing dataset](http://clic.compression.cc/2021/tasks/index.html) as test image in this demo.

## Model Extraction
```
python3 bls2017_me.py --model_path bls2017_0125
```
We also provide extracted weight params with lambda=0.125 in **bin** repo.

In E2E model, convolution/transposed-convloution (CONV/TPCV) operators' weights require IRFFT transformation to generate 2D weights. 
And, processed weights in TPCV operator require a 180° reversal in kernel size dimensions.

## Quantization and SW-Sim
After Model Extraction, weight/bias should be reordered to match the bitwidth of databus (Bit<sub>DB</sub>=Bit<sub>Data</sub>&times;N<sub>LN</sub>) or increase burst length. => **make gen_reorder_w_sk_f32c**

**make test_as_rsc_f32c_NTP** just tests the extracted params and optimized-TPCV in **FT32** precision. Hardware/Software design params could be configured in sw_sim's **Makefile**.


```
X-LIC/sw_sim$ make gen_reorder_w_sk_f32c 
g++ -O3 -w -o test_reorder weight_reorder_sk_f32c.cpp -I . -lm
./test_reorder  4 16 32
ana_0: ic,oc= [3, 128], data_num =    31104, add_offset = 0.
gdn_0: ic,oc= [128, 128], data_num =    16384, add_offset = 0.
ana_1: ic,oc= [128, 128], data_num =   409600, add_offset = 0.
gdn_1: ic,oc= [128, 128], data_num =    16384, add_offset = 0.
ana_2: ic,oc= [128, 128], data_num =   409600, add_offset = 0.
bias[ 0] data_num=  128.
bias[ 1] data_num=  128.
bias[ 2] data_num=  128.
bias[ 3] data_num=  128.
syn_0: ic,oc= [128, 128], data_num =   409600, add_offset = 0.
igdn_0: ic,oc= [128, 128], data_num =    16384, add_offset = 0.
syn_1: ic,oc= [128, 128], data_num =   409600, add_offset = 0.
igdn_1: ic,oc= [128, 128], data_num =    16384, add_offset = 0.
syn_2: ic,oc= [128, 3], data_num =    31104, add_offset = 10368.
bias[ 0] data_num=  128.
bias[ 1] data_num=  128.
bias[ 2] data_num=  128.
bias[ 3] data_num=  128.
bias[ 4] data_num=    3, add 1.

X-LIC/sw_sim$ make test_as_rsc_f32c_NTP 
g++ -O3 -w -o test_ana main_ana_chw_rsc_0_NTP.cpp -I . -lm -D MAX_Pif=16 -D MAX_Pof=4 -D MAX_Poy=1 -D MAX_Pox=4 -D MAX_S=4 -D MAX_K=9 -D MAX_Tif=16 -D MAX_Tof=32 -D MAX_Tr=24 -D MAX_Tc=24 -D MAX_BETA_LENGTH=128 -D LANE_NUM=4 -D _DEF_IN_MAKEFILE_
g++ -O3 -w -o test_syn main_syn_chw_rsc_0_NTP.cpp -I . -lm -D MAX_Pif=16 -D MAX_Pof=4 -D MAX_Poy=1 -D MAX_Pox=4 -D MAX_S=4 -D MAX_K=9 -D MAX_Tif=16 -D MAX_Tof=32 -D MAX_Tr=24 -D MAX_Tc=24 -D MAX_BETA_LENGTH=128 -D LANE_NUM=4 -D _DEF_IN_MAKEFILE_
./test_ana ../abigail-keenan-27293.png
Input img:../abigail-keenan-27293.png
 w=2048,h=1365,c=3
ana_kernel_w_f32rc.bin's data size is 3532288
ana_bias_f32c.bin's data size is 2048
ana_0: ic_h_w= [3, 1365, 2048], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, lat= 60.38215
gdn_0: ic_h_w= [128, 342, 512], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, lat= 6.17650
ana_1: ic_h_w= [128, 342, 512], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, lat= 36.58452
gdn_1: ic_h_w= [128, 171, 256], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, lat= 1.53056
ana_2: ic_h_w= [128, 171, 256], oc_h_w= [128, 86, 128], w_aoffset=0, bias_aoffset=0, lat= 9.18525
ana_output: ana_q_out_chw.bin, h_w_c= [86, 128, 128]

./test_syn ana_q_out_chw.bin ana_IHW_OHW_set.bin
ana_IHW_OHW_set.bin's data size is 80
fm_max_size_calc = 44826624
syn_kernel_w_T_f32rc.bin's data size is 3573760
syn_bias_f32c.bin's data size is 2064
syn_input: ana_q_out_chw.bin, h_w_c= [86, 128, 128]
syn_0: ic_h_w= [128, 86, 128], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, lat= 8.92508
igdn_0: ic_h_w= [128, 171, 256], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, lat= 1.48268
syn_1: ic_h_w= [128, 171, 256], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, lat= 35.17740
igdn_1: ic_h_w= [128, 342, 512], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, lat= 6.00696
syn_2: ic_h_w= [128, 342, 512], oc_h_w= [3, 1365, 2048], w_aoffset=10368, bias_aoffset=1, lat= 21.62807
save_png(s): 2.01311
syn_output: recon_c.png, h_w_c= [1365, 2048, 3]
```
## HLS Design



## Reference
[1] Ballé J, Laparra V, Simoncelli E P. End-to-end optimized image compression[J]. arXiv preprint arXiv:1611.01704, 2016.

[2] 
