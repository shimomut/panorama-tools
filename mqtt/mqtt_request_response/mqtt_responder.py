import json
import datetime

import paho.mqtt.client as mqtt

mqtt_client = mqtt.Client( "my_mqtt_client3" )

def on_connect( client, userdata, flags, rc ):

    print( "on_connect", [ client, userdata, flags, rc ] )
    
    if rc != 0:
        print("Connection to broker failed %d\n" % rc)

def on_message( client, userdata, message ):

    print( "on_message", [ client, userdata, message ] )

    request_payload_s = message.payload.decode("utf-8")
    request_payload_d = json.loads(request_payload_s)

    print("message topic :", message.topic )
    print("message payload :", request_payload_s )
    print("message qos :", message.qos )
    print("message retain flag :", message.retain )

    command = request_payload_d["command"]
    if command == "SayHello":

        t = datetime.datetime.now()

        response_payload_d = {
            "result" : f"Hello {t}"
        }

        print( "sending response :", request_payload_d["response_topic"], response_payload_d )

        mqtt_client.publish( request_payload_d["response_topic"], json.dumps(response_payload_d) )

    else:
        raise ValueError(f"Unknown command {command}")

mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

broker = "broker.emqx.io"
port = 1883

mqtt_client.connect( broker, port )

mqtt_client.subscribe("panorama/remote_command/request")

mqtt_client.loop_forever()
