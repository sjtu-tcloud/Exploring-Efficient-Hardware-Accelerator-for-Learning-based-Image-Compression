# Exploring-Efficient-Hardware-Accelerator-for-Learning-based-Image-Compression

X-LIC is a framework to efficiently explore the design spaces of LIC accelerators. Here, we provide an FPGA demo of LIC design in INT16 precision.

This FPGA demo includes:
* Training and Inference Settings
* The python script of Model Extraction
* Quantization and software-simulation
* HLS Design
* Hardware/Software project for target ARM+FPGA platform

## Training and Inference Settings

Training framework is Tensorflow-2.13 with Tensorflow-compression library, and training settings follow [default configuration](https://github.com/tensorflow/compression/blob/master/models/bls2017.py). The evaluation platform is Xilinx's ARM+FPGA MPSoC ZCU102, and toolchain version is Vivado/Vivado HLS/Vitis v2019.2.

**Training in Tensorflow**
```  
nohup python3 bls2017.py -V train --train_glob '/xxx/train_dataset/*.png' --batchsize 16 --epochs 200 --lambda 0.0004 > train.log 2>&1 &
cat train.log | tail -n 50
```
__Inference in Tensorflow__
``` 
# Inference_encoder
python3 bls2017.py --model_path bls2017 compress org_img.png

# Inference_decoder
python3 bls2017.py --model_path bls2017 decompress org_img.png.tfci rec.png
``` 


# Model Extraction

# Reference

