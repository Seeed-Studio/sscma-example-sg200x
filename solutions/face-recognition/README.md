# Face Recognition (SCRFD + MobileFaceNet)

Face recognition example for CV181x TPU, using SCRFD face detection and MobileFaceNet 128D embedding extraction.

## Features

- **Face Detection**: SCRFD-500M-KPS, 640x640 input, outputs bbox + 5-point landmarks
- **Embedding Extraction**: MobileFaceNet, 112x112 input, outputs 128D embedding
- **Face Alignment**: ArcFace standard 5-point similarity transform
- **Face Database**: Register / identify / list with cosine similarity matching

## Model Specifications

| Model | Input | Output | File Size | ION Memory |
|-------|-------|--------|-----------|------------|
| SCRFD-500M-KPS | [1,640,640,3] uint8 | 9 tensors (score+bbox+kps) | 766 KB | 5.59 MB |
| MobileFaceNet | [1,112,112,3] uint8 | embedding [1,128] float32 | 1.1 MB | 1.37 MB |
| **Total** | | | **1.86 MB** | **6.96 MB** |

## Build

```bash
cd solutions/face-recognition
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build
```

## Usage

### 1. Detect Faces

Run SCRFD face detection only (no recognition):

```bash
./face-recognition detect models/scrfd_500m_kps_int8.cvimodel photo.jpg -o result.jpg
```

### 2. Register a Face

Save a face embedding into the database:

```bash
./face-recognition register \
    models/scrfd_500m_kps_int8.cvimodel \
    models/mobilefacenet_128d_int8.cvimodel \
    alice.jpg "Alice" facedb.txt
```

Register multiple people by running the command repeatedly:

```bash
./face-recognition register models/scrfd_500m_kps_int8.cvimodel models/mobilefacenet_128d_int8.cvimodel bob.jpg "Bob" facedb.txt
```

### 3. Identify Faces

Detect faces and match against the database:

```bash
./face-recognition identify \
    models/scrfd_500m_kps_int8.cvimodel \
    models/mobilefacenet_128d_int8.cvimodel \
    photo.jpg facedb.txt -o result.jpg
```

Example output:

```
Detected 2 faces
Face 0: Alice (0.72)
Face 1: unknown (0.31)
Saved: result.jpg
```

### 4. List Registered Faces

```bash
./face-recognition list facedb.txt
```

## Output Visualization

The result image contains:
- **Green box**: Detected face bounding box
- **Red dots**: 5-point facial landmarks (left eye, right eye, nose tip, left mouth corner, right mouth corner)
- **Yellow text**: Recognized name + cosine similarity score

## Face Database Format

Plain text file, one record per line:

```
Alice 0.123 -0.456 0.789 ... (128 floats)
Bob 0.321 -0.654 0.987 ... (128 floats)
```

## Recognition Thresholds

| Cosine Similarity | Decision |
|-------------------|----------|
| > 0.5 | Same person (high confidence) |
| 0.4 - 0.5 | Same person (low confidence) |
| < 0.4 | Unknown |

Adjustable via the `threshold` parameter in `FaceDatabase::match()`.

## Pipeline

```
Input Image
  │
  ├─ SCRFD (640x640)
  │   ├─ 3 strides: 8/16/32
  │   ├─ Distance-based bbox decoding
  │   ├─ 5-point landmark decoding
  │   └─ NMS
  │
  ├─ Face Alignment (112x112)
  │   ├─ ArcFace 5-point reference coordinates
  │   ├─ Similarity transform (rotation + scale + translation)
  │   └─ Bilinear interpolation
  │
  ├─ MobileFaceNet (112x112)
  │   ├─ 128D embedding
  │   └─ L2 normalization
  │
  └─ Cosine Similarity Matching
      └─ Output: name + score
```

## File Structure

```
solutions/face-recognition/
├── CMakeLists.txt
├── README.md
├── main/
│   ├── CMakeLists.txt
│   ├── main.cpp              # Entry point: register/identify/detect/list
│   ├── face_detector.h/cpp   # SCRFD anchor decoding + NMS
│   ├── face_aligner.h/cpp    # ArcFace 5-point alignment
│   └── face_database.h/cpp   # Face database + cosine similarity
└── models/
    ├── scrfd_500m_kps_int8.cvimodel
    └── mobilefacenet_128d_int8.cvimodel
```

## Model Sources

| Model | Source | Conversion |
|-------|--------|------------|
| SCRFD-500M-KPS | [InsightFace](https://github.com/deepinsight/insightface) | ONNX → CVIMODEL (TPU-MLIR) |
| MobileFaceNet | [foamliu](https://github.com/foamliu/InsightFace-v3) | ONNX → CVIMODEL (TPU-MLIR) |
