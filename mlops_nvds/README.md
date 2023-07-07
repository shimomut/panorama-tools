## Overview

Experiment to setup MLops pipeline for DeepStream apps, with SageMaker Pipelines

## Instruction

1. python3 ./gt_to_kitti.py --src-manifest ./data/src/safety/output.manifest --src-images-dir ./data/src/safety --dst ./data/dst/safety
1. docker run --gpus all -it -v .:/hostfs --rm --entrypoint "" nvcr.io/nvidia/tlt-streamanalytics:v2.0_py3 bash
1. tlt-dataset-convert -d /hostfs/kitti_to_tfrecord.cfg -o /hostfs/safety/safety
