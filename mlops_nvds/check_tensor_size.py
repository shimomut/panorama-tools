import os

import tensorflow as tf

def extract_tfrecords_features(tfrecords_file):
    """Extract features in a tfrecords file for parsing a series of tfrecords files."""
    tfrecords_iterator = tf.python_io.tf_record_iterator(tfrecords_file)

    for record in tfrecords_iterator:

            example = tf.train.Example()
            example.ParseFromString(record)

            features = example.features.feature
            #print("the features is:{}\n".format(features))

            frame_id = features['frame/id'].bytes_list.value
            #print("The frame_id is:{}\n".format(frame_id))

            frame_width = features['frame/width'].int64_list.value
            #print("The frame_width is:{}\n".format(frame_width))

            frame_height = features['frame/height'].int64_list.value
            #print("The frame_height is:{}\n".format(frame_height))

            print( frame_id, frame_width, frame_height )

            assert frame_width[0]==1920 and frame_height[0]==1080

#if __name__ == '__main__':
#    extract_tfrecords_features(tf_file)

tfrecord_dirname = "/hostfs/data/tfrecord/safety"
for filename in os.listdir( tfrecord_dirname ):
    filepath = os.path.join( tfrecord_dirname, filename )
    extract_tfrecords_features(filepath)
