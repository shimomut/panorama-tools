import os
import re
import json
import shutil
import argparse

# ---

argparser = argparse.ArgumentParser( description = 'GroundTruth manifest file to KITTI files conversion tool' )
argparser.add_argument('--src-manifest', dest="src_manifest", action='store', required=True, help='GroundTruth manifest file path')
argparser.add_argument('--src-images-dir', dest="src_images_dir", action='store', required=True, help='Directory path for source images')
argparser.add_argument('--dst', action='store', required=True, help='KITTI output path on S3 (e.g. s3://bucket/yolov5_labels/bbb)')
args = argparser.parse_args()

# ---

def processSingleImage(d):
    
    # Find elements from the naming rule of the elements
    gt_job = None
    gt_job_metadata = None
    source_image = None    
    for k in d:
        if k == "source-ref":
            source_image = d[k]
            continue
        elif k.endswith("-metadata"):
            assert gt_job_metadata is None, "Found multiple metadata in the GroundTruth manifest"
            gt_job_metadata = d[k]
            continue
        else:
            assert gt_job is None, "Found multiple results in the GroundTruth manifest"
            gt_job = d[k]
            continue

    assert gt_job_metadata is not None, "Metadata not found in the GroundTruth manifest"

    # Skip failed ones
    if "failure-reason" in gt_job_metadata:
        print( "Skipping failed result :", gt_job_metadata["failure-reason"] )
        return

    assert gt_job is not None, "Result data not found"
    assert source_image is not None, "Image path not found"

    # Decide label path from image filename
    _, image_filename = os.path.split( source_image )
    src_image_filepath = os.path.join( args.src_images_dir, image_filename )
    dst_image_filepath = os.path.join( args.dst, "images", image_filename )
    dst_label_filepath = os.path.join( args.dst, "labels", os.path.splitext(image_filename)[0] + ".txt" )

    # Get image width / height for normalization
    image_size_list = gt_job["image_size"]
    assert len(image_size_list)==1, "Unexpected length of image_size list."

    image_width, image_height = image_size_list[0]["width"], image_size_list[0]["height"]

    # Skip empty results
    if not gt_job["annotations"]:
        print( "Skipping empty result :", source_image )
        return

    # Write label file
    with open(dst_label_filepath, "w") as fd_label:

        for annotation in gt_job["annotations"]:
            
            klass_name = gt_job_metadata["class-map"][str(annotation["class_id"])]
            truncation = 0.0
            occlusion = 0
            alpha = 0.0
            bbox = ( annotation["left"], annotation["top"], annotation["left"] + annotation["width"], annotation["top"] + annotation["height"] )
            three_d_dimension = (0.0, 0.0, 0.0)
            location = (0.0, 0.0, 0.0)
            rotation_y = (0.0)
            elements = ( klass_name, truncation, occlusion, alpha, *bbox, *three_d_dimension, *location, rotation_y )
            elements = list(map( str, elements ))

            assert len(elements)==15
            s = " ".join(elements)
            
            fd_label.write( s + "\n" )

    # Copy image file
    shutil.copyfile( src_image_filepath, dst_image_filepath )


os.makedirs( os.path.join(args.dst, "images" ), exist_ok=True )
os.makedirs( os.path.join(args.dst, "labels" ), exist_ok=True )

# read GroundTruth manifest file
with open(args.src_manifest) as fd:

    # process line by line and create KITTI directory structure
    for line in fd.readlines():

        d = json.loads( line )
        print(d)

        processSingleImage( d )

print("Done.")
