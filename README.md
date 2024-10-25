# Exploring-Efficient-Hardware-Accelerator-for-Learning-based-Image-Compression


# Training


```  
nohup python3 bls2017.py -V train --train_glob '/xxx/train_dataset/*.png' --batchsize 16 --epochs 200 --lambda 0.0004 > train.log 2>&1 &

cat train.log | tail -n 50
```

__Compress__
``` 
python3 bls2017.py --model_path bls2017 compress org_img.png
``` 
__Decompress__
``` 
python3 bls2017.py --model_path bls2017 decompress org_img.png.tfci rec.png
``` 

# Model Extraction

