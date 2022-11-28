import os
import urllib
import tarfile
import shutil
import datetime
import argparse
import subprocess

import sagemaker
from sagemaker.session import TrainingInput
from sagemaker.pytorch import PyTorch

# ---

argparser = argparse.ArgumentParser( description = 'Yolov5 training with SageMaker training' )
argparser.add_argument('--name', action='store', required=True, help='Training name (e.g. experiment1)')
argparser.add_argument('--s3-path', dest="s3_path", action='store', required=True, help='S3 path to put input/output (e.g. s3://bucket/experiment1)')
argparser.add_argument('--role', action='store', required=True, help='IAM Role ARM')
args = argparser.parse_args()

training_name = args.name
role = args.role
s3_path = args.s3_path

assert s3_path.startswith("s3://")
s3_path = s3_path.rstrip("/")

# ---

#yolov5_version = "6.2"
yolov5_version = "6.1"
#yolov5_version = "6.0"
#yolov5_version = "5.0"

if yolov5_version >= "6.1":
    hyp_filename = "hyps/hyp.VOC.yaml"
elif yolov5_version == "6.0":
    hyp_filename = "hyps/hyp.finetune.yaml"
elif yolov5_version == "5.0":
    hyp_filename = "hyp.finetune.yaml"
else:
    assert False, f"Unsupported YoloV5 version {yolov5_version}"

# ---

# create a temp working directory
work_dirname = "./work-" + datetime.datetime.now().strftime( "%Y%m%d-%H%M%S" )
os.makedirs( os.path.join( work_dirname, "config" ) )

# download yolov5 source package and pretrained model weight file
urllib.request.urlretrieve( f"https://github.com/ultralytics/yolov5/archive/refs/tags/v{yolov5_version}.tar.gz", os.path.join( work_dirname, f"yolov5-v{yolov5_version}.tar.gz" ) )
urllib.request.urlretrieve( f"https://github.com/ultralytics/yolov5/releases/download/v{yolov5_version}/yolov5s.pt", os.path.join( work_dirname, "config", "yolov5s.pt" ) )

# extract source package
def extract_targz( targz_filename, dst_dirname ):
    with tarfile.open( targz_filename, "r" ) as tar_fd:
        def is_within_directory(directory, target):
            
            abs_directory = os.path.abspath(directory)
            abs_target = os.path.abspath(target)
        
            prefix = os.path.commonprefix([abs_directory, abs_target])
            
            return prefix == abs_directory
        
        def safe_extract(tar, path=".", members=None, *, numeric_owner=False):
        
            for member in tar.getmembers():
                member_path = os.path.join(path, member.name)
                if not is_within_directory(path, member_path):
                    raise Exception("Attempted Path Traversal in Tar File")
        
            tar.extractall(path, members, numeric_owner=numeric_owner) 

        safe_extract(tar_fd, path=dst_dirname)

extract_targz( os.path.join( work_dirname, f"yolov5-v{yolov5_version}.tar.gz" ), work_dirname )

# construct input/output directory structure
shutil.copy( os.path.join( work_dirname, f"yolov5-{yolov5_version}/models/yolov5s.yaml" ), os.path.join( work_dirname, "config" ) )
shutil.copy( os.path.join( work_dirname, f"yolov5-{yolov5_version}/data/{hyp_filename}" ), os.path.join( work_dirname, "config", "hyp.yaml" ) )
shutil.copy( f"./data.yaml", os.path.join( work_dirname, "config" ) ) # FIXME : make configurable
shutil.copy( f"./sm_entry_point.py", os.path.join( work_dirname, f"yolov5-{yolov5_version}/" ) )

# upload to S3
subprocess.run( [ "aws", "s3", "sync", os.path.join( work_dirname, "config" ), f"{s3_path}/config" ] )


s3_config = f"{s3_path}/config"
s3_sources = f"{s3_path}/sources"
s3_images = f"{s3_path}/images" # FIXME : make configurabke
s3_labels = f"{s3_path}/labels" # FIXME : make configurabke
s3_results = f"{s3_path}/results"

print( "s3_config :", s3_config )
print( "s3_sources :", s3_sources )
print( "s3_images :", s3_images )
print( "s3_labels :", s3_labels )
print( "s3_results :", s3_results )


estimator = PyTorch(
    entry_point = "sm_entry_point.py",
    source_dir = os.path.join( work_dirname, f"yolov5-{yolov5_version}" ),
    py_version="py38",
    framework_version="1.11.0",
    role=role,
    instance_count=1,
    instance_type='ml.p3.2xlarge',
    input_mode='File',
    code_location=s3_sources,
    output_path=s3_results,
    base_job_name=f'{training_name}'
)


inputs = {
    "config": TrainingInput(s3_config),
    "images": TrainingInput(s3_images),
    "labels": TrainingInput(s3_labels),
}

estimator.fit(inputs)

s3_output = f"{s3_path}/results/{estimator._current_job_name}/output/"
print(s3_output)

subprocess.run( [ "aws", "s3", "ls", s3_output ] )
