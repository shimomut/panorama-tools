import time

import paho.mqtt.client as mqtt


mqtt_client = mqtt.Client( "my_mqtt_client2" )

def on_connect( client, userdata, flags, rc ):

    print( "on_connect", [ client, userdata, flags, rc ] )
    
    if rc != 0:
        print("Connection to broker failed %d\n" % rc)

def on_message( client, userdata, message ):

    print( "on_message", [ client, userdata, message ] )

    print("message received ", str(message.payload.decode("utf-8")) )
    print("message topic=", message.topic )
    print("message qos=", message.qos )
    print("message retain flag=", message.retain )

mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

broker = "broker.emqx.io"
port = 1883

mqtt_client.connect( broker, port )

mqtt_client.subscribe("panorama/test1")

mqtt_client.loop_forever()

