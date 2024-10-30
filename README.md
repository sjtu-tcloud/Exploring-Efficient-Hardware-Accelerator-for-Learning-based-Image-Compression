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
### FT32-Simulation 
After Model Extraction, weight/bias should be reordered to match the bitwidth of databus (Bit<sub>DB</sub>=Bit<sub>Data</sub>&times;N<sub>LN</sub>) or increase burst length. => **make gen_reorder_w_sk_f32c**

**make test_as_rsc_f32c_NTP** just tests the extracted params and optimized-TPCV in **FT32** precision. Hardware/Software design params (e.g., T*, P*) could be configured in sw_sim's **Makefile**.

```
X-LIC/sw_sim$ make gen_reorder_w_sk_f32c 
g++ -O3 -w -o test_reorder weight_reorder_sk_f32c.cpp -I . -lm
./test_reorder  8 8 64
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
syn_2: ic,oc= [128, 3], data_num =    31104, add_offset = 51840.
bias[ 0] data_num=  128.
bias[ 1] data_num=  128.
bias[ 2] data_num=  128.
bias[ 3] data_num=  128.
bias[ 4] data_num=    3, add 5.

X-LIC/sw_sim$ make test_as_rsc_f32c_NTP 
g++ -O3 -w -o test_ana main_ana_chw_rsc_0_NTP.cpp -I . -lm -D MAX_Pif=4 -D MAX_Pof=64 -D MAX_Poy=1 -D MAX_Pox=2 -D MAX_S=4 -D MAX_K=9 -D MAX_Tif=8 -D MAX_Tof=64 -D MAX_Tr=24 -D MAX_Tc=24 -D MAX_BETA_LENGTH=128 -D LANE_NUM=8 -D _DEF_IN_MAKEFILE_
g++ -O3 -w -o test_syn main_syn_chw_rsc_0_NTP.cpp -I . -lm -D MAX_Pif=4 -D MAX_Pof=64 -D MAX_Poy=1 -D MAX_Pox=2 -D MAX_S=4 -D MAX_K=9 -D MAX_Tif=8 -D MAX_Tof=64 -D MAX_Tr=24 -D MAX_Tc=24 -D MAX_BETA_LENGTH=128 -D LANE_NUM=8 -D _DEF_IN_MAKEFILE_
./test_ana ../img/abigail-keenan-27293.png
Input img:../img/abigail-keenan-27293.png
 w=2048,h=1365,c=3
ana_kernel_w_f32rc.bin's data size is 3532288
ana_bias_f32c.bin's data size is 2048
ana_0: ic_h_w= [3, 1365, 2048], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, lat= 22.46917
gdn_0: ic_h_w= [128, 342, 512], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, lat= 8.32909
ana_1: ic_h_w= [128, 342, 512], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, lat= 51.02505
gdn_1: ic_h_w= [128, 171, 256], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, lat= 2.07788
ana_2: ic_h_w= [128, 171, 256], oc_h_w= [128, 86, 128], w_aoffset=0, bias_aoffset=0, lat= 12.83056
ana_output: ana_q_out_chw.bin, h_w_c= [86, 128, 128]

./test_syn ana_q_out_chw.bin ana_IHW_OHW_set.bin
ana_IHW_OHW_set.bin's data size is 80
fm_max_size_calc = 44826624
syn_kernel_w_T_f32rc.bin's data size is 3739648
syn_bias_f32c.bin's data size is 2080
syn_input: ana_q_out_chw.bin, h_w_c= [86, 128, 128]
syn_0: ic_h_w= [128, 86, 128], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, lat= 13.06222
igdn_0: ic_h_w= [128, 171, 256], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, lat= 2.09547
syn_1: ic_h_w= [128, 171, 256], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, lat= 51.77906
igdn_1: ic_h_w= [128, 342, 512], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, lat= 8.38848
syn_2: ic_h_w= [128, 342, 512], oc_h_w= [3, 1365, 2048], w_aoffset=51840, bias_aoffset=5, lat= 348.81109
save_png(s): 0.96445
syn_output: recon_c.png, h_w_c= [1365, 2048, 3]
```
We simplify the X-LIC flow and just reserve the FPGA part in this demo (skip the **Q/IQ** and **entropy coding** in codec). **ana_q_out_chw.bin** is the transformed **y** after encoder.

**img/abigail-keenan-27293.png (Left); img/recon_f32c.png (Right)**
<div style="display: flex;">
    <img src="img/abigail-keenan-27293.png" width="45%" alt="First Image"/> 
    <div style="width: 2%;"></div>
    <img src="img/recon_f32c.png" width="45%" alt="Second Image"/>
</div>

### INT16-Simulation 


## HLS Design



## Reference
[1] Ballé J, Laparra V, Simoncelli E P. End-to-end optimized image compression[J]. arXiv preprint arXiv:1611.01704, 2016.

[2] 
