import os
import shlex
import argparse
import subprocess
import ast

import onnx

import numpy as np

def args_parser():
    parser = argparse.ArgumentParser(description="A CLI tool for convert onnx model to cvimodel.")

    parser.add_argument(
        'model',
        type=str,
        help='Path to the model file.'
    )
    
    parser.add_argument(
        '--input_shapes',
        type=lambda x: ast.literal_eval(x),
        default=[[1,3,640,640]],
        help='Input shapes as a multidimensional array, e.g., "[[1,3,640,640]]".'
    )
    
    parser.add_argument(
        '--mean',
        type=lambda x: list(map(float, x.split(','))),
        default=[0.0, 0.0, 0.0],
        help='Mean values separated by commas, e.g., "0.0,0.0,0.0".'
    )

    parser.add_argument(
        '--scale',
        type=lambda x: list(map(float, x.split(','))),
        default=[0.0039216, 0.0039216, 0.0039216],
        help='Scale values separated by commas, e.g., "0.0039216,0.0039216,0.0039216".'
    )

    parser.add_argument(
        '--pixel_format',
        type=str,
        default='rgb',
        choices=['rgb', 'bgr', 'gray'],
        help='Pixel format, e.g., "rgb".'
    )

    parser.add_argument(
        '--output_names',
        type=lambda x: [s.strip() for s in x.split(',')],
        default=None,
        help='Output names separated by commas, e.g., "output1,output2".'
    )
    
    parser.add_argument(
        '--dataset',
        type=str,
        required=True,
        help='Path to the dataset.'
    )
    
    parser.add_argument(
        '--test_input',
        type=str,
        default=os.path.dirname(os.path.realpath(__file__)) + '/../images/bus.jpg',
        help='Path to the test image.'
    )

    parser.add_argument(
        '--epoch',
        type=int,
        default=200,
        help='Number of epochs, e.g., 100.'
    )
    
    parser.add_argument(
        '--hybrid',
        action='store_true',
        default=False,
        help='Whether to use hybrid quantization, e.g., True.'
    )

    parser.add_argument(
        '--quantize',
        type=str,
        default='INT8',
        choices=['F32', 'BF16', 'F16', 'INT8', 'INT4', 'W8F16', 'W8BF16', 'W4F16', 'W4BF16', 'F8E4M3', 'F8E5M2', 'QDQ'],
        help='Quantization type, e.g., "INT8".'
    )
    
    parser.add_argument(
        '--quantize_table',
        type=str,
        default=None,
        help='Quantization table path, e.g., "./quantize_table".'
    )

    parser.add_argument(
        '--chip',
        type=str,
        default='cv181x',
        choices=['bm1688', 'bm1684x', 'bm1684', 'bm1690', 'mars3', 'cv183x', 'cv182x', 'cv181x', 'cv180x', 'cv186x', 'cpu'],
        help='Target chip, e.g., "cv181x".'
    )

    parser.add_argument(
        '--quant_input',
        type=bool,
        default=True,
        help='Whether to quantize input, e.g., True.'
    )
    
    parser.add_argument(
        '--work_dir',
        type=str,
        default='./work_dir',
        help='Work directory, e.g., "./work_dir".'
    )

    args = parser.parse_args()
    
    return args

model_name = ""
model_path = ""
target_dir = ""

def model_transform(args):
    
    global model_name
    global model_path
    global target_dir
    
    try:
        print("Model Transforming...")
        cmd =[
            'model_transform',
            '--model_name', model_name,
            '--model_def', model_path,
            '--input_shapes', str(args.input_shapes).replace(' ', ''),
            '--mean', ','.join(map(str, args.mean)),
            '--scale', ','.join(map(str, args.scale)),
            '--keep_aspect_ratio',
            '--pixel_format', args.pixel_format,
            '--output_names', ','.join(map(str, args.output_names)),
            '--test_input', args.test_input,
            '--test_result', model_name + '_top_outputs.npz',
            '--mlir', model_name + '.mlir',
        ]
        print(shlex.join(cmd))
        result = subprocess.run(cmd, check=True, text=True, capture_output=True)
        print("Command output:\n", result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"An error occurred while executing the command: {e}")
        print("Command error output:\n", e.stderr)
        print("Command error code:",e.stdout)
        exit(1)
        
def model_quantize(args):
        
    global model_name
    global model_path
    global target_dir
    
    try:
        print("Model Quantizing...")
        cmd = [
        'run_calibration', model_name + '.mlir',
        '--dataset', args.dataset,
        "--input_num", str(args.epoch),
        "-o", model_name + '_calib_table',
        ]
        print(shlex.join(cmd))
        result = subprocess.run(cmd, check=True, text=True, capture_output=True)
        print("Command output:\n", result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"An error occurred while executing the command: {e}")
        print("Command error output:\n", e.stderr)
        exit(1)
        
        
def model_qtable(args):
        
    global model_name
    global model_path
    global target_dir
    
    try:
        print("Model Hybrid Quantizing...")
        cmd = [
        'run_qtable', model_name + '.mlir',
        '--dataset', args.dataset,
        "--calibration_table", model_name + '_calib_table',
        "--chip", args.chip,
        "-o", model_name + '_qtable',
        ]
        print(shlex.join(cmd))
        result = subprocess.run(cmd, check=True, text=True, capture_output=True)
        print("Command output:\n", result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"An error occurred while executing the command: {e}")
        print("Command error output:\n", e.stderr)
        exit(1)
        
        
def model_depoly(args):
        
    global model_name
    global model_path
    global target_dir
    
    try:
        print("Model Deploying...")
        cmd = [
        'model_deploy',
        '--mlir', model_name + '.mlir',
        '--quantize', args.quantize,
        '--quant_input',
        '--processor', args.chip,
        '--calibration_table', model_name + '_calib_table',
        '--test_input', args.test_input,
        '--test_reference', model_name + '_top_outputs.npz',
        '--customization_format', 'RGB_PACKED',
        '--fuse_preprocess',
        '--aligned_input']
        if args.quantize_table != None:
            cmd.append('--quantize_table')
            cmd.append(args.quantize_table)
            cmd.append('--model')
            cmd.append(model_name + '_'  + args.chip + '_mix.cvimodel')
        elif os.path.exists(model_name + '_qtable'):
            cmd.append('--quantize_table')
            cmd.append(model_name + '_qtable')
            cmd.append('--model')
            cmd.append(model_name + '_'  + args.chip + '_mix.cvimodel')
        else:
            cmd.append('--model')
            cmd.append(model_name + '_'  + args.chip + '_' + args.quantize.lower() + '.cvimodel')
        print(shlex.join(cmd))
        result = subprocess.run(cmd, check=True, text=True, capture_output=True)
        print("Command output:\n", result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"An error occurred while executing the command: {e}")
        print("Command error output:\n", e.stderr)
        exit(1)
    
def main():
        
    global model_name
    global model_path
    
    args = args_parser()       
       
    if args.output_names == None:
        if args.model.split('.')[-1] == 'onnx':
            model = onnx.load(args.model)
            graph = model.graph
            args.output_names = [output.name for output in graph.output]
        
    if args.output_names == None:
        print("Error: Output names not specified.")
        exit(1)
        

    print("Model:", args.model)
    print("Work Directory:", args.work_dir)
    print("Input Shapes:", args.input_shapes)
    print("Mean Values:", args.mean)
    print("Scale Values:", args.scale)
    print("Pixel Format:", args.pixel_format)
    print("Output Names:", args.output_names)
    print("Epoch:", args.epoch)
    print("Quantization Type:", args.quantize)
    print("Chip:", args.chip)
    print("Quantize Input:", args.quant_input)
    print("Quantize Table:", args.quantize_table)
    
    model_name = os.path.splitext(os.path.split(args.model)[-1])[0]
    model_path = os.path.realpath(args.model)
    
    target_dir = args.work_dir + '/' + model_name
    if not os.path.exists(target_dir):
        os.makedirs(target_dir)
        
    if args.quantize_table != None:
        args.quantize_table = os.path.realpath(args.quantize_table)
    
    if args.test_input != None:
        args.test_input = os.path.realpath(args.test_input)
    
    if args.dataset != None:
        args.dataset = os.path.realpath(args.dataset)
        
    os.chdir(target_dir)
    
    model_transform(args)
    model_quantize(args)
    
    if args.hybrid:
        model_qtable(args)
        
    model_depoly(args)

if __name__ == "__main__":
    main()
