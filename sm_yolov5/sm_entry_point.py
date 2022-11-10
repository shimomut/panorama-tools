
import sys
import os
import subprocess
import shutil

print( "Hello from sm_entry_point.py" )

print( "Current working directory :", os.getcwd() )

subprocess.run( [ "ls", "-al", "/opt/ml" ] )
subprocess.run( [ "find", "/opt/ml/code" ] )
subprocess.run( [ "find", "/opt/ml/input" ] )
subprocess.run( [ "find", "/opt/ml/model" ] )
subprocess.run( [ "find", "/opt/ml/output" ] )

# FIXME : use environment variables

cmd = [
    sys.executable,
    "/opt/ml/code/train.py",
    "--img", "640",
    "--batch", "16",
    "--epochs", "8",
    "--hyp", "/opt/ml/input/data/config/hyp.VOC.yaml",
    "--data", "/opt/ml/input/data/config/data.yaml",
    "--cfg", "/opt/ml/input/data/config/yolov5s.yaml",
    "--weights", "/opt/ml/input/data/config/yolov5s.pt",
    "--name", "sm_yolov5",
]
print( "Training command :", cmd )
subprocess.run(cmd)

print( "Training ended" )

subprocess.run( [ "find", "/opt/ml/code" ] )
subprocess.run( [ "find", "/opt/ml/input" ] )
subprocess.run( [ "find", "/opt/ml/model" ] )
subprocess.run( [ "find", "/opt/ml/output" ] )

shutil.copytree( f"/opt/ml/code/runs/train/sm_yolov5", f"/opt/ml/model/sm_yolov5" )

print( "sm_entry_point ended" )
