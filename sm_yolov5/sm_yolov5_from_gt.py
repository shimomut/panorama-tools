import os
import re
import json
import io
import argparse

import boto3

# ---

argparser = argparse.ArgumentParser( description = 'GroundTruth manifest file to Yolov5 label files conversion tool' )
argparser.add_argument('--src', action='store', required=True, help='GroundTruth manifest file on S3 (e.g. s3://bucket/aaa/bbb/manifests/output/output.manifest)')
argparser.add_argument('--dst', action='store', required=True, help='Yolov5 label output path on S3 (e.g. s3://bucket/yolov5_labels/bbb)')
args = argparser.parse_args()

s3_manifest = args.src
s3_yolov5_label_dst = args.dst

print( "GroundTruth Manifest file :", s3_manifest )
print( "Yolov5 label files output path :", s3_yolov5_label_dst )

# ---

s3_client = boto3.client("s3")

def splitS3Path( s3_path ):
    re_pattern_s3_path = "s3://([^/]+)/(.*)"
    re_result = re.match( re_pattern_s3_path, s3_path )
    bucket = re_result.group(1)
    key = re_result.group(2)
    return bucket, key

def saveYoloV5LabelOnS3( d, s3_dst ):
    
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
    s3_label = os.path.join( s3_dst, os.path.splitext(image_filename)[0] + ".txt" ).replace("\\","/")

    # Get image width / height for normalization
    image_size_list = gt_job["image_size"]
    assert len(image_size_list)==1, "Unexpected length of image_size list."

    image_width, image_height = image_size_list[0]["width"], image_size_list[0]["height"]

    # Skip empty results
    if not gt_job["annotations"]:
        print( "Skipping empty result :", source_image )
        return

    # Write label data on memory and upload to S3
    with io.StringIO("") as buf:

        for annotation in gt_job["annotations"]:
            
            k = annotation["class_id"]
            center_x = ( annotation["left"] + annotation["width"] * 0.5 ) / image_width
            center_y = ( annotation["top"] + annotation["height"] * 0.5 ) / image_height
            width = annotation["width"] / image_width
            height = annotation["height"] / image_height

            s = "%d %f %f %f %f\n" % (k, center_x, center_y, width, height )
            buf.write(s)

        print( f"Writing {s3_label}" )
        s3_bucket, s3_key = splitS3Path(s3_label)
        s3_client.put_object( Bucket=s3_bucket, Key=s3_key, Body=buf.getvalue().encode("utf-8") )

# read manifest file on S3
s3_manifest_bucket, s3_manifest_key = splitS3Path(s3_manifest)
response = s3_client.get_object( Bucket=s3_manifest_bucket, Key=s3_manifest_key )

# process line by line and write Yolov5 label files on S3
for line in response["Body"].readlines():
    
    line = line.decode("utf-8")
    d = json.loads( line )
    #print(d)
    
    saveYoloV5LabelOnS3( d, s3_yolov5_label_dst )

print("Done.")

