### Overview

s3_launcher is a Panorama application which loads python codes from your S3 bucket. You can modify your code and run it without deploying application to the device, as far as manifest files and model are same.

### Usage

1. Use "app.py" as your Panorama application code.
1. Build & Package & Deploy the application to your device. Please note that you need to specify an IAM Role with readonly access to S3 and full access to SQS.
1. Open CloudWatch Logs, and confirm you see "Waiting for launcher command ..." in the console_output log stream.
1. Upload "helloworkd/*.py" to your S3 bucket. (e.g. "s3://your-bucket-name/helloworld/...")
1. From your host device, run "panorama_tools.py run" command. ([Get panorama_tools.py from here](https://code.amazon.com/packages/ShimomutPersonal/blobs/mainline/--/panorama/tools/panorama_tools.py))
    ```
    $ python panorama_tools.py run --region us-east-1 --app-name myapp --s3_path "s3://your-bucket-name/helloworld/main.py" --funcname "run" --params "{ \"a\" : 123 }"
    ```
1. Confirm you see "params : { "a" : 123 }", "Hello World!" in the code_node stream.
1. Edit your code on S3.
    * option1 - edit locally and upload.
    * option2 - use any solution to mount S3 bucket on your computer, and edit directly.
1. run "panorama_tools.py run" command again, confirm you see updated behavior.


### How to run more realistic applications on the s3_launcher

1. Because panoramasdk.node class doesn't expect instantiating multiple times, you need to modify your application code slightly. (See [this sample code](https://code.amazon.com/packages/ShimomutPersonal/blobs/mainline/--/panorama/samples/pose_estimation/internal/dev_src/app.py))
    1. Stop deriving panoramasdk.node by Application class and get a node instance as an argument for Application.\_\_init__ method, 
    2. Create single panoramasdk.node instance and reuse across multiple runs,
    3. Replace self.inputs, self.outputs, self.call with self.node.inputs, self.node.outputs, self.node.call respectively.
    
2. Because Panorama conputer vision applications typically run infinitely using infinite loop, you need to use "interrupt" command to exit the infinite loop of the app, before running application next. 
    ```
    $ python panorama_tools.py run --region us-east-1 --app-name myapp
    ```


### Features

* Run a python source file on S3 (entry file), and call a function in it.
* Import modules from S3. (have to be at the same place as the entry file.)
* Pass any string as a parameter (you can use JSON format to pass complex data).
* Register additional commands to control the behavior of your application. ( e.g. taking screenshots, triggering profiler, dump statistics information, force pulling Andon Cord, etc )
* Redirecting stdout/stderr with telnet client, so that developers can see logs in real time.
* Running single line shell command ("subprocess-run")
* Uploading local files to S3 bucket ("upload-files")

### Limitations

* region is hardcoded in the launcher source code.
* Native extensions (*.so) cannot be loaded from S3.
* Modules on S3 have to be place at the same location as the entry file. Subfolders are not allowed.