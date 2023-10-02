import json
import uuid
import time

import paho.mqtt.client as mqtt

class MqttRequester:

    def __init__(self):

        self.mqtt_client = mqtt.Client( "my_mqtt_client4" )
        self.remote_command_results = {}

        self.mqtt_client.on_connect = self.on_connect
        self.mqtt_client.on_message = self.on_message

        broker = "broker.emqx.io"
        port = 1883

        self.mqtt_client.connect( broker, port )

        self.mqtt_client.loop_start()

    def shutdown(self):
        self.mqtt_client.loop_stop()

    def on_connect( self, client, userdata, flags, rc ):

        if 0:
            print( "on_connect", [ client, userdata, flags, rc ] )
        
        if rc != 0:
            print("Connection to broker failed %d\n" % rc)

    def on_message( self, client, userdata, message ):

        if 0:
            print( "on_message", [ client, userdata, message ] )

        response_payload_s = message.payload.decode("utf-8")
        response_payload_d = json.loads(response_payload_s)

        if 0:
            print("message topic :", message.topic )
            print("message payload :", response_payload_s )
            print("message qos :", message.qos )
            print("message retain flag :", message.retain )

        if message.topic.startswith("panorama/remote_command/response/"):
            self.remote_command_results[message.topic] = response_payload_d

    def invoke_remote_command( self, command ):

        remote_command_id = str(uuid.uuid4())

        response_topic = f"panorama/remote_command/response/{remote_command_id}"

        request_payload_d = {
            "command" : command,
            "response_topic" : response_topic,
        }

        self.mqtt_client.subscribe(response_topic)

        print("sending request", request_payload_d )
        self.mqtt_client.publish("panorama/remote_command/request", json.dumps(request_payload_d) )

        # FIXME : Should handle timeout also
        result = None
        while True:
            if response_topic in self.remote_command_results:
                result = self.remote_command_results[response_topic]
                break
            time.sleep(0.1)

        self.mqtt_client.unsubscribe(response_topic)

        return result

mqtt_requester = MqttRequester()

try:
    while True:

        result = mqtt_requester.invoke_remote_command("SayHello")
        print("result :", result)

        time.sleep(1)
except KeyboardInterrupt:
    pass

mqtt_requester.shutdown()