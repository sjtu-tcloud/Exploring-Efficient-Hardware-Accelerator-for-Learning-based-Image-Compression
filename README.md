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
We would further optimize the flow. Generated image is stored as **img/recon_i16c.png**.

**make gen_all** 

**make test_as_rsc_i16c_NTP**
```
X-LIC/sw_sim$ make gen_all 
g++ -O3 -w -o test_reorder weight_reorder_sk_f32c.cpp -I . -lm
./test_reorder  8 16 32
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
g++ -O3 -w -o test_ana main_ana_chw_rsc_0_1.cpp -I . -lm -D MAX_Pif=16 -D MAX_Pof=4 -D MAX_Poy=1 -D MAX_Pox=16 -D MAX_S=4 -D MAX_K=9 -D MAX_Tif=16 -D MAX_Tof=32 -D MAX_Tr=24 -D MAX_Tc=24 -D MAX_BETA_LENGTH=128 -D LANE_NUM=8 -D _DEF_IN_MAKEFILE_
g++ -O3 -w -o test_syn main_syn_chw_rsc_0_1.cpp -I . -lm -D MAX_Pif=16 -D MAX_Pof=4 -D MAX_Poy=1 -D MAX_Pox=16 -D MAX_S=4 -D MAX_K=9 -D MAX_Tif=16 -D MAX_Tof=32 -D MAX_Tr=24 -D MAX_Tc=24 -D MAX_BETA_LENGTH=128 -D LANE_NUM=8 -D _DEF_IN_MAKEFILE_
./test_ana ../img/abigail-keenan-27293.png
Input img:../img/abigail-keenan-27293.png
 w=2048,h=1365,c=3
ana_kernel_w_f32rc.bin's data size is 3532288
ana_bias_f32c.bin's data size is 2048
Layer[x]'s fm, c=           3, hw=     2795520 sum2_error = 254.3543486,min_error=0.0000000,max_error=0.0000608
Layer[x]'s fm, c=         128, hw=      175104 sum2_error = 1267.6597215,min_error=0.0000000,max_error=0.0005072
ana_0: ic_h_w= [3, 1365, 2048], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, lat= 20.40577
Layer[x]'s fm, c=         128, hw=      175104 sum2_error = 1267.6597215,min_error=0.0000000,max_error=0.0005072
Layer[x]'s fm, c=         128, hw=      175104 sum2_error = 504.9331022,min_error=0.0000000,max_error=0.0001407
gdn_0: ic_h_w= [128, 342, 512], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, lat= 2.67024
Layer[x]'s fm, c=         128, hw=      175104 sum2_error = 504.9331022,min_error=0.0000000,max_error=0.0001407
Layer[x]'s fm, c=         128, hw=       43776 sum2_error = 701.5041479,min_error=0.0000000,max_error=0.0005592
ana_1: ic_h_w= [128, 342, 512], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, lat= 12.55625
Layer[x]'s fm, c=         128, hw=       43776 sum2_error = 701.5041479,min_error=0.0000000,max_error=0.0005592
Layer[x]'s fm, c=         128, hw=       43776 sum2_error = 513.0118060,min_error=0.0000000,max_error=0.0007035
gdn_1: ic_h_w= [128, 171, 256], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, lat= 0.67850
Layer[x]'s fm, c=         128, hw=       43776 sum2_error = 513.0118060,min_error=0.0000000,max_error=0.0007035
Layer[x]'s fm, c=         128, hw=       11008 sum2_error = 722.0902725,min_error=0.0000000,max_error=0.0106125
ana_2: ic_h_w= [128, 171, 256], oc_h_w= [128, 86, 128], w_aoffset=0, bias_aoffset=0, lat= 3.14991
ana_output: ana_q_out_chw.bin, h_w_c= [86, 128, 128]
./test_syn ana_q_out_chw.bin ana_IHW_OHW_set.bin
ana_IHW_OHW_set.bin's data size is 80
fm_max_size_calc = 44826624
syn_kernel_w_T_f32rc.bin's data size is 3739648
syn_bias_f32c.bin's data size is 2080
syn_input: ana_q_out_chw.bin, h_w_c= [86, 128, 128]
Layer[x]'s fm, c=         128, hw=       11008 sum2_error = 722.0902725,min_error=0.0000000,max_error=0.0106125
Layer[x]'s fm, c=         128, hw=       43776 sum2_error = 4290.3761984,min_error=0.0000000,max_error=0.0039738
syn_0: ic_h_w= [128, 86, 128], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, lat= 3.17336
Layer[x]'s fm, c=         128, hw=       43776 sum2_error = 4290.3761984,min_error=0.0000000,max_error=0.0039738
Layer[x]'s fm, c=         128, hw=       43776 sum2_error = 180.8427711,min_error=0.0000000,max_error=0.0008378
igdn_0: ic_h_w= [128, 171, 256], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, lat= 0.70338
Layer[x]'s fm, c=         128, hw=       43776 sum2_error = 180.8427711,min_error=0.0000000,max_error=0.0008378
Layer[x]'s fm, c=         128, hw=      175104 sum2_error = 1702.2629616,min_error=0.0000000,max_error=0.0002722
syn_1: ic_h_w= [128, 171, 256], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, lat= 12.66654
Layer[x]'s fm, c=         128, hw=      175104 sum2_error = 1702.2629616,min_error=0.0000000,max_error=0.0002722
Layer[x]'s fm, c=         128, hw=      175104 sum2_error = 533.9362322,min_error=0.0000000,max_error=0.0001443
igdn_1: ic_h_w= [128, 342, 512], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, lat= 2.83821
Layer[x]'s fm, c=         128, hw=      175104 sum2_error = 533.9362322,min_error=0.0000000,max_error=0.0001443
Layer[x]'s fm, c=           3, hw=     2795520 sum2_error = 273.5039273,min_error=0.0000000,max_error=0.0000705
syn_2: ic_h_w= [128, 342, 512], oc_h_w= [3, 1365, 2048], w_aoffset=51840, bias_aoffset=5, lat= 41.22453
save_png(s): 0.93261
syn_output: recon_c.png, h_w_c= [1365, 2048, 3]
g++ -O3 -w -o test_reorder weight_reorder_sk_i16c_scale.cpp -I . -lm
./test_reorder  8 16 32
Layer[x]; param num=       31104 sum2_error = 0.9448845,min_error=0.0000000,max_error=0.0000610,add_offset = 0
ana_kernel_scale[layer 0]: 128
0.225055 0.237296 0.373880 0.260098 0.531330 0.165818 0.148947 0.445908 0.170151 0.200833 0.167590 0.182580 0.199225 0.980556 0.510683 0.448036 0.111066 0.631133 0.186018 0.189641 0.142772 0.192934 0.438250 0.520388 0.210549 0.177195 0.280721 0.176198 0.205126 0.661552 0.510595 0.436457 0.204955 0.361915 0.234862 0.191611 0.427454 0.611326 0.190420 0.228873 1.141163 0.877437 0.396672 0.255384 0.729060 0.198744 0.169571 0.160956 0.246616 0.540760 0.155805 0.225777 0.293986 0.567261 0.392496 0.216836 0.204307 0.827114 0.201451 0.228318 0.525964 0.518119 0.229340 0.217786 0.178666 0.510584 0.236121 0.210427 0.303614 0.167409 0.112202 0.202841 0.180769 0.183855 0.218421 0.500220 0.262842 0.206621 0.615586 0.208496 0.126984 0.190780 0.301233 0.473933 0.278001 0.749024 0.463060 0.625574 0.822353 0.186323 0.138109 0.254235 0.242063 0.558940 0.693118 0.167648 0.491871 0.200210 0.200082 0.198618 0.184500 0.676253 0.177592 0.605657 0.581336 0.528060 0.278074 0.200355 0.161340 0.724994 0.862936 0.318258 0.213048 0.210001 0.758210 0.270510 0.195200 0.277854 0.196408 0.220545 0.675083 0.509716 0.286615 0.206250 0.149472 0.236433 0.148629 0.443114 
Layer[x]; param num=       16384 sum2_error = 0.1091819,min_error=0.0000000,max_error=0.0000610,add_offset = 0
ana_kernel_scale[layer 1]: 128
0.031147 0.000994 0.917249 0.064610 1.773192 0.006014 0.000022 1.083094 0.027781 0.017409 0.069763 0.021264 0.000020 2.591479 0.052098 1.825990 0.028488 4.148032 0.086192 0.092456 0.003413 0.031486 1.499829 2.352924 0.009111 0.031125 0.032819 0.322771 1.284640 2.922270 1.834260 0.123121 0.000009 0.238006 0.000017 0.000012 2.934916 1.586229 0.021563 0.158086 3.503239 2.969779 0.999871 1.739478 1.430983 0.010429 0.000091 0.017507 0.031142 2.291910 0.000352 0.000980 2.745074 1.641775 2.835114 0.042163 0.087813 3.191728 0.028445 0.000010 4.292156 2.081964 0.021621 4.193572 0.000005 0.531968 0.032499 0.041076 0.001190 0.085120 0.204190 0.006309 0.082815 0.002656 0.021245 2.972432 0.037492 0.007545 2.118101 0.034720 0.250841 0.267169 0.056243 1.415591 0.393664 2.807176 2.457098 2.574513 2.458302 0.012572 0.000010 0.594831 0.062328 2.507060 3.400230 0.113844 3.607721 0.016259 0.001798 0.060283 0.033855 2.920895 0.023380 1.751863 3.301947 3.775176 0.031130 0.245461 0.543633 1.629322 3.574587 1.310744 0.000016 0.005284 2.417200 0.030942 0.098967 0.660635 0.000019 0.233566 2.263718 2.047902 0.083831 0.016424 0.005144 0.346454 0.209380 1.929998 
Layer[x]; param num=      409600 sum2_error = 12.5154430,min_error=0.0000000,max_error=0.0000610,add_offset = 0
ana_kernel_scale[layer 2]: 128
1.041856 0.641916 0.462146 0.387104 0.606160 0.493124 0.497160 0.478551 0.419493 0.548309 0.854383 0.458903 0.462944 0.602404 0.478335 0.474708 0.496113 0.647565 0.624261 1.069949 0.441687 0.860811 0.390646 0.529091 0.378573 0.486991 0.672581 0.525961 0.727371 0.441628 0.529915 0.846093 0.528079 0.576039 0.317257 0.543614 1.090287 0.515437 0.476315 0.493445 0.683831 0.587894 0.972766 0.632019 0.488754 0.540586 0.453084 0.737196 0.682935 0.359626 0.552318 0.821144 0.535776 0.756995 0.432804 0.520991 0.504678 0.338343 0.382012 0.446501 0.496451 0.346181 0.636842 0.628777 0.412958 0.488068 0.531319 0.545944 0.405578 0.336560 0.694092 0.445187 0.536115 0.559032 0.510096 0.802232 0.634371 0.400471 0.588955 0.399064 0.656833 0.440687 0.461172 0.738143 0.480313 0.679742 0.678318 0.549839 0.550081 0.436790 0.546642 0.827783 1.101233 0.425219 0.443169 0.399670 0.484448 0.565104 0.513691 0.394630 0.536995 0.448727 0.438991 0.421724 0.366782 0.439024 0.404632 0.588932 0.326572 0.413965 0.432353 0.503120 0.492383 0.509755 0.398704 0.251750 0.658331 0.714396 0.381841 0.354799 0.671605 0.284033 0.561109 0.620649 0.486757 0.606509 0.646353 0.356007 
Layer[x]; param num=       16384 sum2_error = 0.1625405,min_error=0.0000000,max_error=0.0000610,add_offset = 0
ana_kernel_scale[layer 3]: 128
0.046144 0.730936 0.404517 0.016251 0.050200 0.478955 0.002989 0.101177 0.244274 0.063992 0.104361 0.005713 0.003666 0.715628 0.001737 1.003897 1.846145 0.003832 0.001185 1.032861 0.002879 0.123857 0.010689 0.784684 0.138518 0.227651 0.004473 0.787519 0.034952 0.542330 0.034728 0.021779 0.010181 0.005315 0.029316 0.005677 0.011603 0.011323 0.000020 0.393513 0.824758 0.008231 0.002459 0.008508 0.006151 0.003856 1.059539 0.068721 0.001846 0.222806 0.922524 0.074067 0.344694 0.071218 0.093439 0.746680 0.002819 0.135685 0.019124 0.008044 0.061544 0.189933 0.993708 0.980550 0.325430 0.014055 0.004233 0.021040 0.632051 0.629885 0.012413 3.369975 0.543556 1.082364 2.283819 0.065866 0.022350 2.036103 0.057355 0.011776 1.624131 0.031070 1.855071 0.006051 0.097436 0.020411 0.014552 0.018227 0.002596 0.001815 0.013971 0.148585 0.121497 0.006129 0.007664 0.016338 3.653621 0.080070 1.272481 0.010641 1.848974 0.017837 0.096603 0.072445 0.084457 0.999178 0.871463 0.005900 1.161402 0.001662 0.012083 0.000023 0.000016 0.996533 0.007326 0.000017 1.173587 1.121154 0.468973 0.031208 0.116281 0.022956 1.572614 1.557843 0.784756 2.276247 0.422096 0.037217 
Layer[x]; param num=      409600 sum2_error = 12.4677284,min_error=0.0000000,max_error=0.0000610,add_offset = 0
ana_kernel_scale[layer 4]: 128
2.923947 10.322408 8.680382 5.039129 5.954485 2.491461 3.205051 4.837365 1.746119 5.175746 1.999218 2.837621 3.499777 3.577028 4.925704 2.505244 2.450547 2.320180 3.821879 1.722577 1.946533 3.634374 2.370606 4.615684 2.524274 2.184777 2.447839 2.107969 2.084080 7.890220 3.272768 3.880426 3.963903 6.020605 3.098922 3.629713 1.863828 3.055017 1.989013 2.183816 4.395061 2.075487 5.750112 4.415492 2.057778 8.084857 2.268886 1.731475 1.715138 6.948359 5.662016 1.711246 3.313944 2.379232 3.957419 3.599319 2.848653 4.356260 1.812489 3.926515 2.229153 2.246059 5.405558 6.886427 2.151324 1.910339 3.327307 5.429876 8.585103 1.472740 5.870682 5.475686 2.628093 3.802093 1.476167 8.759886 9.216216 2.747183 2.918596 6.905808 3.743461 4.403009 4.984287 8.193434 8.215320 10.167226 6.264430 5.028947 6.275967 2.087776 2.172397 5.412045 2.777023 4.552153 2.145207 3.387580 3.470748 5.573725 4.177909 4.391069 7.409708 7.635388 3.045192 4.687135 2.862561 1.929939 1.387904 2.272588 2.108236 2.025977 2.828318 5.276394 3.089356 3.308066 4.800056 4.949317 4.758601 1.643609 3.413962 4.123566 4.512076 4.838839 4.182241 1.986061 7.167818 6.782355 4.270787 4.644061 
Layer  0;param num=         128 
Layer  1;param num=         128 
Layer  2;param num=         128 
Layer  3;param num=         128 
Layer[x]; param num=      409600 sum2_error = 12.4787013,min_error=0.0000000,max_error=0.0000610,add_offset = 0
syn_kernel_scale[layer 0]: 128
10.545935 11.220606 5.894372 18.580221 14.172844 11.742649 7.934359 12.472867 6.512974 13.509349 8.844347 6.992039 12.429928 18.781969 13.033720 30.294325 7.284652 10.943562 15.761875 14.158024 17.108732 14.108118 11.591031 11.415376 22.665098 12.057085 9.858669 14.682076 7.709925 8.873486 15.681151 17.452765 14.292487 6.232162 21.032591 9.383430 14.467843 10.266370 9.838019 7.334391 15.940626 22.200487 9.428760 8.945144 16.157553 12.416565 6.446595 18.214334 9.009165 12.767478 18.291180 10.684918 13.816285 7.894396 22.788002 17.037914 19.558891 11.996474 10.575626 12.314756 32.737648 8.125493 10.924119 7.638798 14.659727 0.882191 10.556086 13.712625 24.636131 13.059223 8.920827 15.610270 9.695519 17.147831 10.328279 18.219175 5.936819 40.411049 13.003510 13.561673 6.645676 22.867479 19.490753 8.109791 7.789054 19.293324 13.395112 5.476259 7.637269 27.406004 9.696424 11.105989 11.004752 11.826965 3.829393 11.489029 12.876169 13.075474 30.856308 10.341284 37.469326 18.538809 11.943660 6.944187 9.471856 23.288956 15.449793 21.577156 8.422313 7.292045 6.076042 5.177770 12.419411 12.442942 6.609837 18.593576 14.085267 13.049467 13.537670 27.680996 19.115782 11.402274 13.528372 20.143757 5.650653 18.339470 14.503698 3.841490 
Layer[x]; param num=       16384 sum2_error = 0.3920681,min_error=0.0000000,max_error=0.0000610,add_offset = 0
syn_kernel_scale[layer 1]: 128
0.000476 0.000713 0.000252 0.064226 0.000376 0.000370 0.001189 0.006625 0.001067 0.000283 0.000675 0.000062 0.000887 0.018685 0.000149 0.009670 0.002108 0.013942 0.000299 0.000809 0.000166 0.000261 0.018114 0.000224 0.000255 0.000478 0.001574 0.000847 0.002369 0.000390 0.000132 0.009570 0.012053 0.001160 0.000515 0.000544 0.000698 0.000886 0.001175 0.094045 0.001223 0.000102 0.000394 0.001446 0.000247 0.000265 0.000589 0.018064 0.000710 0.001508 0.000678 0.000647 0.000742 0.153771 0.016105 0.000323 0.000187 0.000582 0.009158 0.000259 0.017845 0.013175 0.000776 0.000390 0.022896 0.000372 0.002546 0.000774 0.000511 0.001234 0.001531 0.000382 0.000444 0.001417 0.000786 0.000201 0.000200 0.021519 0.001592 0.000912 0.001407 0.000402 0.000436 0.001796 0.000198 0.000205 0.000777 0.002015 0.000619 0.004966 0.000279 0.000189 0.000945 0.015481 0.011786 0.032796 0.000299 0.000252 0.011747 0.000261 0.000112 0.001010 0.011096 0.003276 0.000333 0.000312 0.000451 0.000305 0.001117 0.002457 0.013868 0.001101 0.000183 0.000176 0.037529 0.000259 0.011110 0.098928 0.000293 0.000229 0.000375 0.040527 0.020167 0.000276 0.016666 0.000336 0.000786 0.006723 
Layer[x]; param num=      409600 sum2_error = 12.5022373,min_error=0.0000000,max_error=0.0000610,add_offset = 0
syn_kernel_scale[layer 2]: 128
0.581394 0.952564 1.151048 0.739274 0.836730 0.912252 1.152374 0.806737 0.982264 1.027219 1.399866 1.934632 1.337994 1.599005 0.579546 1.009802 1.993249 2.043401 0.894019 0.744953 1.237017 0.564994 0.780738 0.514860 1.463013 0.559413 1.172089 0.911584 1.219949 2.143977 1.114082 1.152260 0.694947 0.553034 0.984976 0.542213 1.175234 0.442299 1.253923 1.159255 0.999771 1.846647 0.478692 1.090201 1.732432 1.157110 1.127012 0.911472 0.754866 1.045733 1.166232 0.814642 0.957714 0.692575 0.525165 1.172487 0.572738 1.345894 1.027143 1.168100 0.543036 1.044394 1.078660 0.953465 0.495688 1.159568 1.052435 1.375248 0.936337 0.794160 1.352140 0.884565 0.607123 1.797091 0.597948 0.962274 1.075468 0.610868 1.375628 0.545391 2.243261 1.054031 1.685254 0.458111 0.778800 0.839175 0.975595 0.601523 1.013995 1.095380 1.198880 0.802404 0.991222 0.727996 1.036959 0.937215 0.732550 2.133775 0.961070 1.181018 0.879551 0.537071 0.996910 2.709047 0.476939 1.746620 0.893342 0.528994 0.721539 0.744786 0.825951 0.921037 0.523052 1.072109 1.513177 1.486975 1.068513 0.830429 0.900323 0.633302 1.489266 0.456821 1.073372 1.111975 1.651283 0.864434 0.854373 1.265860 
Layer[x]; param num=       16384 sum2_error = 0.2220924,min_error=0.0000000,max_error=0.0000610,add_offset = 0
syn_kernel_scale[layer 3]: 128
0.156208 0.060915 0.013454 0.001756 0.155528 0.453054 0.015216 0.035856 0.143984 0.013334 0.015107 0.013364 0.285261 0.171145 0.072585 0.004814 0.012324 0.015449 0.043593 0.065906 0.131536 0.264501 0.054059 0.051011 0.041618 0.072341 0.016641 0.061614 0.220682 0.006836 0.162004 0.068616 0.313302 0.174545 0.082594 0.052572 0.218042 0.184946 0.035594 0.081188 0.082067 0.063340 0.115552 0.276228 0.010048 0.114067 0.129330 0.059430 0.018542 0.097799 0.063218 0.051858 0.050759 0.043034 0.213324 0.109717 0.119590 0.019549 0.060186 0.005751 0.077433 0.078357 0.014404 0.070622 0.076817 0.021503 0.069742 0.018564 0.075691 0.059881 0.000463 0.057254 0.054753 0.195211 0.226450 0.118517 0.202369 0.304747 0.018636 0.081465 0.023909 0.128670 0.219400 0.098644 0.047941 0.263975 0.022055 0.283439 0.024324 0.012187 0.121599 0.068217 0.128785 0.022709 0.072857 0.046966 0.058925 0.009352 0.055813 0.288701 0.051927 0.058695 0.096938 0.003626 0.140377 0.022198 0.068178 0.144276 0.303494 0.021850 0.040631 0.293260 0.113479 0.124060 0.009813 0.014846 0.125859 0.060690 0.009304 0.018345 0.009058 0.029808 0.052498 0.090598 0.002679 0.064159 0.012586 0.049156 
Layer[x]; param num=       31104 sum2_error = 0.9480077,min_error=0.0000000,max_error=0.0000610,add_offset = 51840
syn_kernel_scale[layer 4]: 3
0.439908 0.449402 0.430605 
Layer  0;param num=         128 
Layer  1;param num=         128 
Layer  2;param num=         128 
Layer  3;param num=         128 
Layer  4;param num=           3 add 5
g++ -O3 -w -o test_ana main_ana_chw_rsc_0_3.cpp -I . -lm -D MAX_Pif=16 -D MAX_Pof=4 -D MAX_Poy=1 -D MAX_Pox=16 -D MAX_S=4 -D MAX_K=9 -D MAX_Tif=16 -D MAX_Tof=32 -D MAX_Tr=24 -D MAX_Tc=24 -D MAX_BETA_LENGTH=128 -D LANE_NUM=8 -D _DEF_IN_MAKEFILE_
g++ -O3 -w -o test_syn main_syn_chw_rsc_0_3.cpp -I . -lm -D MAX_Pif=16 -D MAX_Pof=4 -D MAX_Poy=1 -D MAX_Pox=16 -D MAX_S=4 -D MAX_K=9 -D MAX_Tif=16 -D MAX_Tof=32 -D MAX_Tr=24 -D MAX_Tc=24 -D MAX_BETA_LENGTH=128 -D LANE_NUM=8 -D _DEF_IN_MAKEFILE_
./test_ana ../img/abigail-keenan-27293.png
Input img:../img/abigail-keenan-27293.png
 w=2048,h=1365,c=3
ana_kernel_w_i16rc.bin's data size is 1766144
ana_bias_f32c.bin's data size is 2048
[0]: diff max= 0.0000000000, diff sum = 0.0000000
[Q=0]: diff max= 0.0000000000, diff sum_min = 0.0000000
maxQ = 28
[0]: diff max= 2.0399169922, diff sum = 128657.1503296
[1]: diff max= 1.8579101562, diff sum = 881.3211060
[2]: diff max= 0.0000610352, diff sum = 5.2509155
[3]: diff max= 0.0000610352, diff sum = 14.0045166
[4]: diff max= 0.0000610352, diff sum = 32.8590088
[Q=2]: diff max= 0.0000610352, diff sum_min = 5.2509155
ana_0: ic_h_w= [3, 1365, 2048], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, ifm_sqQ=0, interQ=2, lat= 313.76129
[0]: diff max= 0.0000000000, diff sum = 0.0000000
[Q=0]: diff max= 0.0000000000, diff sum_min = 0.0000000
maxQ = 28
[0]: diff max= 0.0000610352, diff sum = 0.0320435
[1]: diff max= 0.0000610352, diff sum = 0.0587769
[2]: diff max= 0.0000610352, diff sum = 0.2025757
[Q=0]: diff max= 0.0000610352, diff sum_min = 0.0320435
gdn_0: ic_h_w= [128, 342, 512], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, ifm_sqQ=0, interQ=0, lat= 33.47577
[0]: diff max= 0.0000000000, diff sum = 0.0000000
[Q=0]: diff max= 0.0000000000, diff sum_min = 0.0000000
maxQ = 28
[0]: diff max= 2.0097656250, diff sum = 226556.3510132
[1]: diff max= 1.9299926758, diff sum = 32097.9395752
[2]: diff max= 0.0000610352, diff sum = 11.5443115
[3]: diff max= 0.0000610352, diff sum = 31.6424561
[4]: diff max= 0.0000610352, diff sum = 74.7365723
[Q=2]: diff max= 0.0000610352, diff sum_min = 11.5443115
ana_1: ic_h_w= [128, 342, 512], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, ifm_sqQ=0, interQ=2, lat= 198.47264
[0]: diff max= 0.0000000000, diff sum = 0.0000000
[Q=0]: diff max= 0.0000000000, diff sum_min = 0.0000000
maxQ = 28
[0]: diff max= 0.0000610352, diff sum = 0.0073853
[1]: diff max= 0.0000610352, diff sum = 0.0094604
[2]: diff max= 0.0000610352, diff sum = 0.0272827
[Q=0]: diff max= 0.0000610352, diff sum_min = 0.0073853
gdn_1: ic_h_w= [128, 171, 256], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, ifm_sqQ=0, interQ=0, lat= 8.42805
[0]: diff max= 0.0000000000, diff sum = 0.0000000
[Q=0]: diff max= 0.0000000000, diff sum_min = 0.0000000
maxQ = 28
[0]: diff max= 1.8496704102, diff sum = 3372.7236328
[1]: diff max= 1.8884277344, diff sum = 3428.9384155
[2]: diff max= 0.0000610352, diff sum = 5.9699707
[3]: diff max= 0.0000610352, diff sum = 16.4581909
[Q=2]: diff max= 0.0000610352, diff sum_min = 5.9699707
ana_2: ic_h_w= [128, 171, 256], oc_h_w= [128, 86, 128], w_aoffset=0, bias_aoffset=0, ifm_sqQ=0, interQ=2, lat= 40.76141
ana_output: ana_q_out_chw.bin, h_w_c= [86, 128, 128]
./test_syn ana_q_out_chw.bin ana_IHW_OHW_set.bin
ana_IHW_OHW_set.bin's data size is 80
fm_max_size_calc = 44826624
syn_kernel_w_T_i16rc.bin's data size is 1869824
syn_bias_f32c.bin's data size is 2080
syn_input: ana_q_out_chw.bin, h_w_c= [86, 128, 128]
[0]: diff max= 0.0000000000, diff sum = 0.0000000
[Q=0]: diff max= 0.0000000000, diff sum_min = 0.0000000
maxQ = 28
[0]: diff max= 0.0000610352, diff sum = 0.3021240
[1]: diff max= 0.0000610352, diff sum = 2.2265625
[2]: diff max= 0.0000610352, diff sum = 8.8473511
[Q=0]: diff max= 0.0000610352, diff sum_min = 0.3021240
syn_0: ic_h_w= [128, 86, 128], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, ifm_sqQ=0, interQ=0, lat= 31.94204
[0]: diff max= 0.0000000000, diff sum = 0.0000000
[Q=0]: diff max= 0.0000000000, diff sum_min = 0.0000000
maxQ = 28
[0]: diff max= 1.5436401367, diff sum = 4.1616821
[1]: diff max= 0.0000610352, diff sum = 0.0119019
[2]: diff max= 0.0000610352, diff sum = 0.0330200
[3]: diff max= 0.0000610352, diff sum = 0.0922852
[Q=1]: diff max= 0.0000610352, diff sum_min = 0.0119019
igdn_0: ic_h_w= [128, 171, 256], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, ifm_sqQ=0, interQ=1, lat= 10.38710
[0]: diff max= 0.0000000000, diff sum = 0.0000000
[Q=0]: diff max= 0.0000000000, diff sum_min = 0.0000000
maxQ = 28
[0]: diff max= 0.0000610352, diff sum = 1.2314453
[1]: diff max= 0.0000610352, diff sum = 7.6048584
[2]: diff max= 0.0000610352, diff sum = 30.2382202
[Q=0]: diff max= 0.0000610352, diff sum_min = 1.2314453
syn_1: ic_h_w= [128, 171, 256], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, ifm_sqQ=0, interQ=0, lat= 127.39691
[0]: diff max= 0.0000000000, diff sum = 0.0000000
[Q=0]: diff max= 0.0000000000, diff sum_min = 0.0000000
maxQ = 28
[0]: diff max= 0.0000610352, diff sum = 0.0366211
[1]: diff max= 0.0000610352, diff sum = 0.0474243
[2]: diff max= 0.0000610352, diff sum = 0.1217041
[Q=0]: diff max= 0.0000610352, diff sum_min = 0.0366211
igdn_1: ic_h_w= [128, 342, 512], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, ifm_sqQ=0, interQ=0, lat= 33.59070
[0]: diff max= 0.0000000000, diff sum = 0.0000000
[Q=0]: diff max= 0.0000000000, diff sum_min = 0.0000000
maxQ = 28
[0]: diff max= 0.0000610352, diff sum = 0.4783325
[1]: diff max= 0.0000610352, diff sum = 0.8795776
[2]: diff max= 0.0000610352, diff sum = 3.0659790
[Q=0]: diff max= 0.0000610352, diff sum_min = 0.4783325
syn_2: ic_h_w= [128, 342, 512], oc_h_w= [3, 1365, 2048], w_aoffset=51840, bias_aoffset=5, ifm_sqQ=0, interQ=0, lat= 387.38145
save_png(s): 0.94908
syn_output: recon_c.png, h_w_c= [1365, 2048, 3]
g++ -O3 -w -o test_ana gen_w_sr_cb.cpp -I . -lm -D MAX_Pif=16 -D MAX_Pof=4 -D MAX_Poy=1 -D MAX_Pox=16 -D MAX_S=4 -D MAX_K=9 -D MAX_Tif=16 -D MAX_Tof=32 -D MAX_Tr=24 -D MAX_Tc=24 -D MAX_BETA_LENGTH=128 -D LANE_NUM=8 -D _DEF_IN_MAKEFILE_
./test_ana
ana_kernel_w_i16rc.bin's data size is 1766144
syn_kernel_w_T_i16rc.bin's data size is 1869824

X-LIC/sw_sim$ make test_as_rsc_i16c_NTP
g++ -O3 -w -o test_ana main_ana_chw_rsc_0_4_NTP.cpp -I . -lm -D MAX_Pif=16 -D MAX_Pof=4 -D MAX_Poy=1 -D MAX_Pox=16 -D MAX_S=4 -D MAX_K=9 -D MAX_Tif=16 -D MAX_Tof=32 -D MAX_Tr=24 -D MAX_Tc=24 -D MAX_BETA_LENGTH=128 -D LANE_NUM=8 -D _DEF_IN_MAKEFILE_
g++ -O3 -w -o test_syn main_syn_chw_rsc_0_4_NTP.cpp -I . -lm -D MAX_Pif=16 -D MAX_Pof=4 -D MAX_Poy=1 -D MAX_Pox=16 -D MAX_S=4 -D MAX_K=9 -D MAX_Tif=16 -D MAX_Tof=32 -D MAX_Tr=24 -D MAX_Tc=24 -D MAX_BETA_LENGTH=128 -D LANE_NUM=8 -D _DEF_IN_MAKEFILE_
./test_ana ../img/abigail-keenan-27293.png
Input img:../img/abigail-keenan-27293.png
 w=2048,h=1365,c=3
ana_kernel_w_i16rc_srcb.bin's data size is 1766144
read weight byte_num = 441536
ana_bias_f32c.bin's data size is 2048
512
read bias byte_num = 2048
5
5
TR,TC,TM,TN=2,16,32,3
ana_0: ic_h_w= [3, 1365, 2048], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, ifm_sqQ=0, interQ=2, lat= 65.52812
TR,TC,TM,TN=2,512,32,16
gdn_0: ic_h_w= [128, 342, 512], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, ifm_sqQ=0, interQ=0, lat= 7.14576
TR,TC,TM,TN=1,96,32,16
ana_1: ic_h_w= [128, 342, 512], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, ifm_sqQ=0, interQ=2, lat= 41.09157
TR,TC,TM,TN=4,256,32,16
gdn_1: ic_h_w= [128, 171, 256], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, ifm_sqQ=0, interQ=0, lat= 1.78098
TR,TC,TM,TN=1,96,32,16
ana_2: ic_h_w= [128, 171, 256], oc_h_w= [128, 86, 128], w_aoffset=0, bias_aoffset=0, ifm_sqQ=0, interQ=2, lat= 10.37997
ana_output: ana_q_out_chw.bin, h_w_c= [86, 128, 128]
./test_syn ana_q_out_chw.bin ana_IHW_OHW_set.bin
ana_IHW_OHW_set.bin's data size is 80
fm_max_size_calc = 44826624
syn_kernel_w_T_i16rc_srcb.bin's data size is 1869824
read weight byte_num = 467456
syn_bias_f32c.bin's data size is 2080
520
read bias byte_num = 2080
5
5
syn_input: ana_q_out_chw.bin, h_w_c= [86, 128, 128]
TR,TC,TM,TN=2,160,32,16
syn_0: ic_h_w= [128, 86, 128], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, lat= 10.02213
TR,TC,TM,TN=4,256,32,16
igdn_0: ic_h_w= [128, 171, 256], oc_h_w= [128, 171, 256], w_aoffset=0, bias_aoffset=0, lat= 1.78630
TR,TC,TM,TN=2,160,32,16
syn_1: ic_h_w= [128, 171, 256], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, lat= 39.81513
TR,TC,TM,TN=2,512,32,16
igdn_1: ic_h_w= [128, 342, 512], oc_h_w= [128, 342, 512], w_aoffset=0, bias_aoffset=0, lat= 7.17127
TR,TC,TM,TN=4,64,3,16
syn_2: ic_h_w= [128, 342, 512], oc_h_w= [3, 1365, 2048], w_aoffset=51840, bias_aoffset=5, lat= 19.15339
save_png(s): 0.94992
syn_output: recon_c.png, h_w_c= [1365, 2048, 3]
```

**img/abigail-keenan-27293.png (Left); img/recon_i16c.png (Right)**
<div style="display: flex;">
    <img src="img/abigail-keenan-27293.png" width="45%" alt="First Image"/> 
    <div style="width: 2%;"></div>
    <img src="img/recon_i16c.png" width="45%" alt="Second Image"/>
</div>

## HLS Design



## Reference
[1] Ballé J, Laparra V, Simoncelli E P. End-to-end optimized image compression[J]. arXiv preprint arXiv:1611.01704, 2016.

[2] 
