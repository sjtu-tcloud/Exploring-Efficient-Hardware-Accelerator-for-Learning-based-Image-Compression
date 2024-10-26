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

## HLS Design



## Reference
[1] Ballé J, Laparra V, Simoncelli E P. End-to-end optimized image compression[J]. arXiv preprint arXiv:1611.01704, 2016.

[2] 
