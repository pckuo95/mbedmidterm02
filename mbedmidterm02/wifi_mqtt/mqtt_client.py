import serial
import paho.mqtt.client as paho
import time
import matplotlib.pyplot as plt
import numpy as np
serdev = '/dev/ttyACM0'
s = serial.Serial(serdev, 9600)

s.write(bytes("\r", 'UTF-8'))
line=s.readline() # Read an echo string from mbed terminated with '\n' (putc())
print(line)
line=s.readline() # Read an echo string from mbed terminated with '\n' (RPC reply)
print(line)
time.sleep(1)

s.write(bytes("/LEDControl/run 1 1\r", 'UTF-8'))
line=s.readline() # Read an echo string from mbed terminated with '\n' (putc())
print(line)
line=s.readline() # Read an echo string from mbed terminated with '\n' (RPC reply)
print(line)
time.sleep(1)
s.write(bytes("/sel_gesture/run\r", 'UTF-8'))
# https://os.mbed.com/teams/mqtt/wiki/Using-MQTT#python-client
count_t = 0
# MQTT broker hosted on local machine
mqttc = paho.Client()

# Settings for connection
# TODO: revise host to your IP!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
host = "172.18.1.133"
topic = "Mbed"

# Callbacks
def on_connect(self, mosq, obj, rc):
    print("Connected rc: " + str(rc))

def on_message(mosq, obj, msg):
    print("[Received] Topic: " + msg.topic + ", Message: " + str(msg.payload) + "\n");
    count_t += 1
    value[count_t] = str(msg.payload)

def on_subscribe(mosq, obj, mid, granted_qos):
    print("Subscribed OK")

def on_unsubscribe(mosq, obj, mid, granted_qos):
    print("Unsubscribed OK")

# Set callbacks
mqttc.on_message = on_message
mqttc.on_connect = on_connect
mqttc.on_subscribe = on_subscribe
mqttc.on_unsubscribe = on_unsubscribe

# Connect and subscribe
print("Connecting to " + host + "/" + topic)
mqttc.connect(host, port=1883, keepalive=60)
mqttc.subscribe(topic, 0)

# Publish messages from Python
num = 0
while num != 5:
    ret = mqttc.publish(topic, "Message from Python!\n", qos=0)
    if (ret[0] != 0):
            print("Publish failed")
    mqttc.loop()
    time.sleep(1.5)
    num += 1

while count_t == 5:
    time.sleep(5)
    s.write(bytes("/sel_gesture/run\r", 'UTF-8'))
    for x in range(0, 5):
        tilt_value[x]=s.readline() # Read an echo string from B_L4S5I_IOT01A terminated with '\n'

    t = [1,2,3,4,5,6]
    fig, ax = plt.subplots(2, 1)
    ax[0].plot(t,value)
    ax[0].set_xlabel('Time')

    ax[0].set_ylabel('gesture ID')

    ax[1].plot(t,tilt_value) # plotting the spectrum

    ax[1].set_xlabel('Time')

    ax[1].set_ylabel('tilted!')

    plt.show()


# Loop forever, receiving messages
mqttc.loop_forever()