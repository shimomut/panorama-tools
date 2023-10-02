import json
import uuid
import time

import paho.mqtt.client as mqtt


mqtt_client = mqtt.Client( "my_mqtt_client4" )
remote_command_results = {}


def on_connect( client, userdata, flags, rc ):

    print( "on_connect", [ client, userdata, flags, rc ] )
    
    if rc != 0:
        print("Connection to broker failed %d\n" % rc)

def on_message( client, userdata, message ):

    print( "on_message", [ client, userdata, message ] )

    response_payload_s = message.payload.decode("utf-8")
    response_payload_d = json.loads(response_payload_s)

    print("message topic :", message.topic )
    print("message payload :", response_payload_s )
    print("message qos :", message.qos )
    print("message retain flag :", message.retain )

    if message.topic.startswith("panorama/remote_command/response/"):
        remote_command_results[message.topic] = response_payload_d

mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

broker = "broker.emqx.io"
port = 1883

print("Connecting")
mqtt_client.connect( broker, port )

# one way message
if 0:
    mqtt_client.publish("panorama/test1","Hello MQTT")

# request-response
if 1:
    def invoke_remote_command( command ):

        remote_command_id = str(uuid.uuid4())

        response_topic = f"panorama/remote_command/response/{remote_command_id}"

        request_payload_d = {
            "command" : command,
            "response_topic" : response_topic,
        }

        mqtt_client.subscribe(response_topic)

        print("sending request", request_payload_d )
        mqtt_client.publish("panorama/remote_command/request", json.dumps(request_payload_d) )

        # FIXME : Should handle timeout also
        result = None
        while True:
            if response_topic in remote_command_results:
                result = remote_command_results[response_topic]
                break
            time.sleep(0.1)

        mqtt_client.unsubscribe(response_topic)

        return result

mqtt_client.loop_start()

result = invoke_remote_command("SayHello")
print("result :", result)

while True:
    time.sleep(0.1)

mqtt_client.loop_stop()
