# NAS + WebDav access from Panorama application (prototype)

## Overview

AWS Panorama appliance device's read-write local storage is limited. For Panorama applications which require larger read-write storage, this sample demonstrates how to use NAS drive and WebDav protocol to store files in local network.

## Tested hardware

* Synology 2 Bay NAS DiskStation DS220+ (Diskless) - https://www.amazon.com/gp/product/B087ZCBWFH/
* Seagate IronWolf 4TB NAS Internal Hard Drive HDD - https://www.amazon.com/gp/product/B07H289S79/

> Note : if you use other products, please make sure it supports WebDav server feature.


## Required configurations

* Referring to NAS drive's document, set up NAS drive itself, and WebDav server - https://kb.synology.com/en-me/DSM/tutorial/How_to_access_files_on_Synology_NAS_with_WebDAV

* Install "requests" and "webdavclient3" by using RUN pip3 command in Dockerfile.

    ``` dockerfile
    FROM public.ecr.aws/panorama/panorama-application
    COPY src /panorama
    RUN pip3 install boto3
    RUN pip3 install requests webdavclient3
    ```

## APIs

sample code:

``` python
import os
from webdav3.client import Client

options = {
    'webdav_hostname': "http://10.0.0.246:5005",    # replace with the IP address / port
    'webdav_login':    "abcde",                     # replace with your user name
    'webdav_password': "!naw%erdvHs12oh",           # replace with your password
    'disable_check' : True,
}

client = Client(options)

# list directory
r = client.list("test1")
print( r, flush=True )

# make directory
r = client.mkdir("test1/test2")
print( r, flush=True )

# confirm new directory created
r = client.list("test1")
print( r, flush=True )

# create a temp file
with open( "/tmp/test3.txt", "w" ) as fd:
    fd.write( "This is test3.txt" )

# upload file
r = client.upload_sync( local_path="/tmp/test3.txt", remote_path="test1/test2/test3.txt" )
print( r, flush=True )

# confirm file uploaded
r = client.list("test1/test2")
print( r, flush=True )

# download file
r = client.download_sync( remote_path="test1/test2/test3.txt", local_path="/tmp/test4.py" )
print( r, flush=True )

# confirm file downloaded    
r = os.listdir( "/tmp" )
print( r, flush=True )

# delete directory
r = client.clean("test1/test2")
print( r, flush=True )

# list directory again to confirm directory deletd
r = client.list("test1")
print( r, flush=True )
```

For more about how to use this module, please refer to the official site. - https://pypi.org/project/webdavclient3/

### Limitations

* Haven't succeeded to use HTTPS without skipping verifying certificate. Attached sample python code uses HTTP.
* This solution is positioned as a prototype. Please use at your own risk.
