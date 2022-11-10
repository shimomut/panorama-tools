import sys
import os
import subprocess
import shutil

# FIXME : use environment variables

print( "Starting sm_entry_point.py" )

if 0:
    print( "Current working directory :", os.getcwd() )
    subprocess.run( [ "ls", "-al", "/opt/ml" ] )
    subprocess.run( [ "find", "/opt/ml/code" ] )
    subprocess.run( [ "find", "/opt/ml/input" ] )
    subprocess.run( [ "find", "/opt/ml/model" ] )
    subprocess.run( [ "find", "/opt/ml/output" ] )

cmd = [
    sys.executable,
    "/opt/ml/code/train.py",
    "--img", "640",
    "--batch", "16",
    "--epochs", "8",
    "--hyp", "/opt/ml/input/data/config/hyp.yaml",
    "--data", "/opt/ml/input/data/config/data.yaml",
    "--cfg", "/opt/ml/input/data/config/yolov5s.yaml",
    "--weights", "/opt/ml/input/data/config/yolov5s.pt",
    "--name", "sm_yolov5",
]

print( "Starting training :", cmd )

subprocess.run(cmd)

print( "Training ended" )

if 0:
    subprocess.run( [ "find", "/opt/ml/code" ] )
    subprocess.run( [ "find", "/opt/ml/input" ] )
    subprocess.run( [ "find", "/opt/ml/model" ] )
    subprocess.run( [ "find", "/opt/ml/output" ] )

result_src = "/opt/ml/code/runs/train/sm_yolov5"
result_dst = "/opt/ml/model/sm_yolov5"

print( f"Copying {result_src} -> {result_dst}" )
shutil.copytree( result_src, result_dst )

print( "sm_entry_point.py ended" )
