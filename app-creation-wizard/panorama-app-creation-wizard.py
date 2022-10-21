import sys
import os
import re
import time
import datetime
import argparse
import tempfile
import shutil
import gzip
import json


def input2( instruction_message, default="" ):

    s = input(instruction_message)
    s = s.strip()
    if not s:
        s = default

    # FIXME : add validation here

    return s

class Config:
    pass

def gather_configuration():

    c = Config()

    c.application_name = input2("Application name (e.g. people_detection_test): ")
    
    default_code_package_name = f"{c.application_name}_code"
    c.code_package_name = input2( f"Code package name [{default_code_package_name}]: ", default=default_code_package_name )

    default_py_file_name = "app.py"
    c.py_file_name = input2( f"Main Python script file name [{default_py_file_name}]: ", default=default_py_file_name )

    default_camera_node_name = f"camera_node"
    c.camera_node_name = input2( f"Camera node name [{default_camera_node_name}]: ", default=default_camera_node_name )
    
    need_hdmi_node = input2( f"Do you use HDMI data sink? [Yn]: ", default="y" ).lower()
    if need_hdmi_node=="y":
        c.hdmi_node_name = "hdmi_node"
    else:
        c.hdmi_node_name = None

    framework_options_message = (
        "Choose framework from following options.\n"
        "  1. Minimal (no deeplearning framework)\n"
        "  2. Tensorflow\n"
        "  3. PyTorch\n"
        "  4. TensorRT\n"
        "[1-4]: "
    )

    c.framework_choice = input2( framework_options_message )
    # FIXME : only option 1 is implemented for now

    return c

def create_text_file( filepath, contents ):
    
    print( f"Creating {filepath}" )
    
    with open( filepath, "w" ) as fd:
        fd.write(contents)

def create_manifest_file(c):

    d = {
        "nodeGraph": {
            "envelopeVersion": "2021-01-01",
            "packages": [
                {
                    "name": f"123456789012::{c.code_package_name}",
                    "version": "1.0"
                },
                {
                    "name": "panorama::abstract_rtsp_media_source",
                    "version": "1.0"
                }
            ],
            "nodes": [
                {
                    "name": "code_node",
                    "interface": f"123456789012::{c.code_package_name}.code_interface",
                    "overridable": False,
                    "launch": "onAppStart"
                },
                {
                    "name": c.camera_node_name,
                    "interface": "panorama::abstract_rtsp_media_source.rtsp_v1_interface",
                    "overridable": True,
                    "launch": "onAppStart",
                    "decorator": {
                        "title": "camera(s)",
                        "description": "camera(s)"
                    }
                }
            ],
            "edges": [
                {
                    "producer": f"{c.camera_node_name}.video_out",
                    "consumer": "code_node.video_in"
                }
            ]
        }
    }

    if c.hdmi_node_name is not None:

        d["nodeGraph"]["packages"].append(
            {
                "name": "panorama::hdmi_data_sink",
                "version": "1.0"
            }
        )

        d["nodeGraph"]["nodes"].append(
            {
                "name": c.hdmi_node_name,
                "interface": "panorama::hdmi_data_sink.hdmi0",
                "overridable": False,
                "launch": "onAppStart"
            }
        )

        d["nodeGraph"]["edges"].append(
            {
                "producer": "code_node.video_out",
                "consumer": f"{c.hdmi_node_name}.video_in"
            }
        )

    s = json.dumps( d, indent=4 )

    create_text_file( os.path.join( c.application_name, "graphs", c.application_name, "graph.json" ), s )


py_file_template = """
import sys
import time

import panoramasdk

# application class
class Application(panoramasdk.node):
    
    # initialize application
    def __init__(self):
        
        super().__init__()
        
        self.frame_count = 0

    # run top-level loop of application  
    def run(self):
        
        while True:
            
            print("Frame :", self.frame_count, flush=True )

            # get video frames from camera inputs 
            media_list = self.inputs.video_in.get()

            # TODO : implement application specific code to media objects
            
            # put video output to HDMI
            self.outputs.video_out.put(media_list)
            
            self.frame_count += 1

app = Application()
app.run()
"""

def create_py_file(c):
    create_text_file( os.path.join( c.application_name, "packages", f"123456789012-{c.code_package_name}-1.0", "src", c.py_file_name ), py_file_template )

dockerfile_template = """
FROM public.ecr.aws/panorama/panorama-application
RUN python3.7 -m pip install opencv-python matplotlib boto3
COPY src /panorama
"""

def create_dockerfile(c):
    create_text_file( os.path.join( c.application_name, "packages", f"123456789012-{c.code_package_name}-1.0", "Dockerfile" ), dockerfile_template )

def create_code_descriptor_file(c):

    d = {
        "runtimeDescriptor": {
            "envelopeVersion": "2021-01-01",
            "entry": {
                "path": "python3",
                "name": f"/panorama/{c.py_file_name}"
            }
        }
    }    

    s = json.dumps( d, indent=4 )

    create_text_file( os.path.join( c.application_name, "packages", f"123456789012-{c.code_package_name}-1.0", "descriptor.json" ), s )


def create_code_package_file(c):

    d = {
        "nodePackage": {
            "envelopeVersion": "2021-01-01",
            "name": c.code_package_name,
            "version": "1.0",
            "description": "",
            "assets": [
            ],
            "interfaces": [
                {
                    "name": "code_interface",
                    "category": "business_logic",
                    "asset": "code",
                    "inputs": [
                        {
                            "name": "video_in",
                            "type": "media"
                        }
                    ]
                }
            ]
        }
    }

    if c.hdmi_node_name is not None:

        code_interface = d["nodePackage"]["interfaces"][0]

        assert code_interface["name"] == "code_interface"

        if "outputs" not in code_interface:
            code_interface["outputs"] = []
            
        code_interface["outputs"].append(
            {
                "name": "video_out",
                "type": "media"
            }
        )

    s = json.dumps( d, indent=4 )

    create_text_file( os.path.join( c.application_name, "packages", f"123456789012-{c.code_package_name}-1.0", "package.json" ), s )


def create_application_files(c):

    # create directories
    os.makedirs( c.application_name )
    os.makedirs( os.path.join( c.application_name, "assets" ) )
    os.makedirs( os.path.join( c.application_name, "graphs", c.application_name ) )
    os.makedirs( os.path.join( c.application_name, "packages", f"123456789012-{c.code_package_name}-1.0", "src" ) )

    # create files
    create_text_file( os.path.join( c.application_name, "assets/.keepme" ), "" )
    create_manifest_file(c)
    create_py_file(c)
    create_dockerfile(c)
    create_code_descriptor_file(c)
    create_code_package_file(c)


def main():

    c = gather_configuration()

    create_application_files( c )

    print(f"Successfully created Panorama application files under [/{c.application_name}].")


main()
