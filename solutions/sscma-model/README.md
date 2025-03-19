# SSCMA Model Example

## Overview

**sscma-model** is an example application demonstrating how to utilize a model with the **SSCMA-Micro** framework. This application enables users to input an image, perform inference using the model, and visualize the results.

## Getting Started

Before building this solution, ensure that you have set up the **ReCamera-OS** environment as described in the main project documentation:

🔗 **[SSCMA Example for SG200X - Main README](../../README.md)**

This includes:

- Setting up **ReCamera-OS**
- Configuring the SDK path
- Preparing the necessary toolchain

If you haven't completed these steps, follow the instructions in the main project README before proceeding.

## Building & Installing

### 1. Navigate to the `sscma-model` Solution

```bash
cd solutions/sscma-model
```

### 2. Build the Application

```bash
mkdir build
cd build
cmake ..
make
```

### 3. Run the Application

To run the application, use the following command:

```bash
./sscma-model <model_file> <input_image> [<output_image>]
```

- `<model_file>`: Path to the model file (e.g., `yolo11.cvimodel`).
- `<input_image>`: Path to the input image file (e.g., `cat.jpg`).
- `<output_image>` (optional): Path to save the output image (default is `result.jpg`).

**Example:**

```bash
./sscma-model yolo11.cvimodel cat.jpg out.jpg
```

## Expected Output

Upon executing the application, the following occurs:

1. The specified model is loaded.
2. The input image is read and preprocessed.
3. Inference is performed using the loaded model.
4. Depending on the model output type, the application will:

   - For **classification**: Display the predicted class and confidence score on the image.
   - For **keypoint detection**: Draw keypoints and their confidence scores on the image.
   - For **bounding box detection**: Draw bounding boxes around detected objects with their scores.
   - For **segmentation**: Overlay masks on the detected segments with scores.

5. The output image is saved to the specified path or as `result.jpg` if no output path is provided.

### Example Output Messages

- For classification:
  ```
  score: 0.897 target: 1
  ```

- For keypoint detection:
  ```
  x: 0.324, y: 0.456, w: 0.123, h: 0.234, score: 0.789, target: 2
  ```

- For bounding box detection:
  ```
  x: 0.123, y: 0.456, w: 0.789, h: 0.234, score: 0.876, target: 3
  ```

- For segmentation:
  ```
  x: 0.234, y: 0.567, w: 0.345, h: 0.456, score: 0.654, target: 4
  ```

## Conclusion

This example serves as a basic introduction to using the SSCMA model for image inference. Users can modify the code and adapt it for their specific needs, experimenting with different models and input images.

For further details on the SSCMA framework, refer to the [SSCMA-Micro documentation](https://github.com/Seeed-Studio/SSCMA-Micro).
