# DRGraph

### Install
linux environment  
Gsl(2.4)  Boost(1.58) Cmake(3.11)

### Build
```bash
# CPU
mkdir build
bash build.sh

# GPU (unstable)
mkdir build
bash build_GPU.sh
```

### Run
```bash

./Vis -input ./data/block_2000.txt -output block_2000.txt -neg 5 -samples 400 -gamma 0.1 -mode 1 -A 2 -B 1

python ./visualization/layout.py -graph ./data/block_2000.txt -layout block_2000.txt -outpng block_2000.png
```

### system
```bash 
cd system
# frontend
cd frontend
npm build
npm run start
# backend
cd backend
pip install -r requirements.txt
python router.py
```
