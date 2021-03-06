#include "accelerometer_handler.h"
#include "config.h"
#include "magic_wand_model_data.h"

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"
// add uLCD
#include "uLCD_4DGL.h"

// add PRC library and mbed library
#include "mbed_rpc.h"
#include "mbed.h"

// add wifi mtqq lib
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include "stm32l475e_iot01_accelero.h"
#include "math.h"


// uLCD declare
uLCD_4DGL uLCD(D1, D0, D2); // serial tx, serial rx, reset pin;
// PRC declare
DigitalOut myled1(LED1);
DigitalOut myled2(LED2);
DigitalOut myled3(LED3);
BufferedSerial pc(USBTX, USBRX);
void sel_gesture(Arguments *in, Reply *out);
// tilt angle detect
void tilt_detect (Arguments *in, Reply *out);
void show_data (Arguments *in, Reply *out);
int acc2angle(void);
RPCFunction rpcFun1(&sel_gesture, "sel_gesture");
RPCFunction rpcFun2(&tilt_detect, "tilt_detect");
RPCFunction rpcFun3(&show_data, "show_data");
double x, y;
int sel_enable = true;
// default angle 30
int sel_angle = 30;
// homework declare
InterruptIn btnSelect(USER_BUTTON);
EventQueue queue(32 * EVENTS_EVENT_SIZE);
Thread t;

// examdeclare
int gesture_index = 0;
int data_change_dir[6] = {0};
int trigger_time = 0;
// acc value
int16_t AccDataXYZ[3] = {0};
int16_t ref_ACC[3] = {0};
double ref_length = 1;
int tilt_angle = 0;
void initial_ref_acc(void);

// wifi mtqq declare
WiFiInterface *wifi;
//InterruptIn btn2(USER_BUTTON);
//InterruptIn btn3(SW3);
volatile int message_num = 0;
volatile int arrivedcount = 0;
volatile bool closed = false;

const char* topic = "Mbed";

Thread mqtt_thread(osPriorityHigh);
EventQueue mqtt_queue;
void messageArrived(MQTT::MessageData& md);
void publish_message(MQTT::Client<MQTTNetwork, Countdown>* client);
void close_mqtt();


// Create an area of memory to use for input, output, and intermediate arrays.
// The size of this will depend on the model you're using, and may need to be
// determined by experimentation.
constexpr int kTensorArenaSize = 55 * 1024;
uint8_t tensor_arena[kTensorArenaSize];
// Return the result of the last prediction
int PredictGesture(float* output);
// interrupt fun
void stopRecord(void);



// PRC main
int main() 
{
    //The mbed RPC classes are now wrapped to create an RPC enabled version - see RpcClasses.h so don't add to base class
    // receive commands, and send back the responses
    char buf[128], outbuf[200];

    FILE *devin = fdopen(&pc, "r");
    FILE *devout = fdopen(&pc, "w");

    while(1) {
        memset(buf, 0, 128);
        for (int i = 0; ; i++) {
            char recv = fgetc(devin);
            if (recv == '\n') {
                printf("\r\n");
                break;
            }
            buf[i] = fputc(recv, devout);
        }
        //Call the static call method on the RPC class
        RPC::call(buf, outbuf);
        printf("%s\r\n", outbuf);
    }
  return 0;
}

void stopRecord(void) 
{
  
  sel_enable = false;
  uLCD.locate(1,3);
  uLCD.printf("Comfirmed!");
  printf("---stop---\n");
  printf("confirmed_angle: %d\n", sel_angle);

}

// gesture main revise to PRC function
//int main(int argc, char* argv[]) {
void sel_gesture (Arguments *in, Reply *out) 
{
  //////////------------
  int i = 0;
    message_num = 0;
    trigger_time = 0;
    // wifi
    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
            printf("ERROR: No WiFiInterface found.\r\n");
            //return -1;
    }

    printf("\nConnecting to %s...\r\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
            printf("\nConnection error: %d\r\n", ret);
            //return -1;
    }

    NetworkInterface* net = wifi;
    MQTTNetwork mqttNetwork(net);
    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

    //TODO: revise host to your IP!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    const char* host = "172.18.1.133";
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    printf("Connecting to TCP network...\r\n");

    SocketAddress sockAddr;
    sockAddr.set_ip_address(host);
    sockAddr.set_port(1883);

    printf("address is %s/%d\r\n", (sockAddr.get_ip_address() ? sockAddr.get_ip_address() : "None"),  (sockAddr.get_port() ? sockAddr.get_port() : 0) ); //check setting

    int rc = mqttNetwork.connect(sockAddr);//(host, 1883);
    if (rc != 0) {
            printf("Connection error.");
            //return -1;
    }
    printf("Successfully connected!\r\n");

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "Mbed";

    if ((rc = client.connect(data)) != 0){
            printf("Fail to connect MQTT\r\n");
    }
    if (client.subscribe(topic, MQTT::QOS0, messageArrived) != 0){
            printf("Fail to subscribe\r\n");
    }

    mqtt_thread.start(callback(&mqtt_queue, &EventQueue::dispatch_forever));
    
    
  ///////////--------------wifi
  
  //bool success = true;
  myled2 = 1;
  uLCD.cls();
  uLCD.reset();
  uLCD.locate(1,2);
  uLCD.printf("gesture ID = %d", gesture_index);
  sel_enable = true;
    // In this scenario, when using RPC delimit the one argument with a space.
    x = in->getArg<double>();
    // Have code here to call another RPC function to wake up specific led or close it.
    char buffer[100], outbuf[100];
    char strings[20];
  

  //interrrupt
  t.start(callback(&queue, &EventQueue::dispatch_forever));
  btnSelect.rise(queue.event(stopRecord));

  // Whether we should clear the buffer next time we fetch data
  bool should_clear_buffer = false;
  bool got_data = false;
  

  // The gesture index of the prediction
  

  // Set up logging.
  static tflite::MicroErrorReporter micro_error_reporter;
  tflite::ErrorReporter* error_reporter = &micro_error_reporter;

  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  const tflite::Model* model = tflite::GetModel(g_magic_wand_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    error_reporter->Report(
        "Model provided is schema version %d not equal "
        "to supported version %d.",
        model->version(), TFLITE_SCHEMA_VERSION);
    //return -1;
  }

  // Pull in only the operation implementations we need.
  // This relies on a complete list of all the ops needed by this graph.
  // An easier approach is to just use the AllOpsResolver, but this will
  // incur some penalty in code space for op implementations that are not
  // needed by this graph.
  static tflite::MicroOpResolver<6> micro_op_resolver;
  micro_op_resolver.AddBuiltin(
      tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
      tflite::ops::micro::Register_DEPTHWISE_CONV_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_MAX_POOL_2D,
                               tflite::ops::micro::Register_MAX_POOL_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_CONV_2D,
                               tflite::ops::micro::Register_CONV_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED,
                               tflite::ops::micro::Register_FULLY_CONNECTED());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_SOFTMAX,
                               tflite::ops::micro::Register_SOFTMAX());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_RESHAPE,
                               tflite::ops::micro::Register_RESHAPE(), 1);

  // Build an interpreter to run the model with
  static tflite::MicroInterpreter static_interpreter(
      model, micro_op_resolver, tensor_arena, kTensorArenaSize, error_reporter);
  tflite::MicroInterpreter* interpreter = &static_interpreter;

  // Allocate memory from the tensor_arena for the model's tensors
  interpreter->AllocateTensors();

  // Obtain pointer to the model's input tensor
  TfLiteTensor* model_input = interpreter->input(0);
  if ((model_input->dims->size != 4) || (model_input->dims->data[0] != 1) ||
      (model_input->dims->data[1] != config.seq_length) ||
      (model_input->dims->data[2] != kChannelNumber) ||
      (model_input->type != kTfLiteFloat32)) {
    error_reporter->Report("Bad input tensor parameters in model");
    //return -1;
  }

  int input_length = model_input->bytes / sizeof(float);

  TfLiteStatus setup_status = SetupAccelerometer(error_reporter);
  if (setup_status != kTfLiteOk) {
    error_reporter->Report("Set up failed\n");
    //return -1;
  }

  error_reporter->Report("Set up successful...\n");

  while (sel_enable) {
    // Attempt to read new data from the accelerometer
    got_data = ReadAccelerometer(error_reporter, model_input->data.f,
                                 input_length, should_clear_buffer);
    // If there was no new data,
    // don't try to clear the buffer again and wait until next time
    if (!got_data) {
      should_clear_buffer = false;
      continue;
    }

    // Run inference, and report any error
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
      error_reporter->Report("Invoke failed on index: %d\n", begin_index);
      continue;
    }

    // Analyze the results to obtain a prediction
    gesture_index = PredictGesture(interpreter->output(0)->data.f);
    
    ///-------------
// another way to classify data
    //printf("x = %d,y = %d, z = %d\n", pDataXYZ[0], pDataXYZ[1], pDataXYZ[2]);
    
    //----------


    //determin angle
    if (gesture_index == 0 ||gesture_index == 1 || gesture_index == 2) {
      mqtt_queue.call(&publish_message, &client);
      uLCD.locate(1,2);
      uLCD.printf("gesture ID = %d", gesture_index);
      error_reporter->Report("gesture ID = %d\n", gesture_index);

      int change_dir = 0;
      for (i = 0; i < 600; i = i + 30) {
      
        float val_x = *(model_input->data.f + i * sizeof(float));
        float val_y = *(model_input->data.f + (i + 1) * sizeof(float));
        float val_z = *(model_input->data.f + (i + 2) * sizeof(float));

        double length_acc = sqrt(val_x * val_x + val_y * val_y + val_z * val_z);
        double theta_acc = acos((val_x* val_x + val_y * val_y + val_z * val_z) / (length_acc * length_acc));
        int angle_acc = (int)(theta_acc * 180 / 3.1415926);
        if (angle_acc > 30) {
          change_dir = 1;
        }
      }

      data_change_dir[trigger_time] = change_dir;
      trigger_time++;
      
    
      if (trigger_time == 5) {
        sel_enable = 0;
        close_mqtt();
      }
    }



    // Clear the buffer next time we read data
    should_clear_buffer = gesture_index < label_num;

    // Produce an output
    if (gesture_index < label_num) {
      error_reporter->Report(config.output_message[gesture_index]);
    }
  }
  myled2 = 0;
/////---close wifi

// wifi wait here
    //btn3.rise(&close_mqtt);
    int num = 0;
    while (num != 5) {
            client.yield(100);
            ++num;
    }
    
    while (1) {
            if (closed) break;
            client.yield(500);
            ThisThread::sleep_for(500ms);
    }
    printf("Ready to close MQTT Network......\n");

    if ((rc = client.unsubscribe(topic)) != 0) {
            printf("Failed: rc from unsubscribe was %d\n", rc);
    }
    if ((rc = client.disconnect()) != 0) {
    printf("Failed: rc from disconnect was %d\n", rc);
    }

    mqttNetwork.disconnect();
    printf("Successfully closed!\n");

}

int PredictGesture(float* output) 
{
  // How many times the most recent gesture has been matched in a row
  static int continuous_count = 0;
  // The result of the last prediction
  static int last_predict = -1;

  // Find whichever output has a probability > 0.8 (they sum to 1)
  int this_predict = -1;
  for (int i = 0; i < label_num; i++) {
    if (output[i] > 0.8) this_predict = i;
  }

  // No gesture was detected above the threshold
  if (this_predict == -1) {
    continuous_count = 0;
    last_predict = label_num;
    return label_num;
  }

  if (last_predict == this_predict) {
    continuous_count += 1;
  } else {
    continuous_count = 0;
  }
  last_predict = this_predict;

  // If we haven't yet had enough consecutive matches for this gesture,
  // report a negative result
  if (continuous_count < config.consecutiveInferenceThresholds[this_predict]) {
    return label_num;
  }
  // Otherwise, we've seen a positive result, so clear all our variables
  // and report it
  continuous_count = 0;
  last_predict = -1;
  return this_predict;
}

// tilt angle detectation
void tilt_detect (Arguments *in, Reply *out) 
{
    myled3 = 1;
    uLCD.cls();
    uLCD.reset();
    uLCD.locate(1,2);
    uLCD.printf("Sel Angle = %d", sel_angle);
    uLCD.locate(1,3);
    uLCD.printf("initial acc....");
    message_num = 0;
    // initial ACC
    BSP_ACCELERO_Init();
    printf("Start accelerometer init\n");

    // wifi
    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
            printf("ERROR: No WiFiInterface found.\r\n");
            //return -1;
    }

    printf("\nConnecting to %s...\r\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
            printf("\nConnection error: %d\r\n", ret);
            //return -1;
    }

    NetworkInterface* net = wifi;
    MQTTNetwork mqttNetwork(net);
    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

    //TODO: revise host to your IP!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    const char* host = "172.18.1.133";
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    printf("Connecting to TCP network...\r\n");

    SocketAddress sockAddr;
    sockAddr.set_ip_address(host);
    sockAddr.set_port(1883);

    printf("address is %s/%d\r\n", (sockAddr.get_ip_address() ? sockAddr.get_ip_address() : "None"),  (sockAddr.get_port() ? sockAddr.get_port() : 0) ); //check setting

    int rc = mqttNetwork.connect(sockAddr);//(host, 1883);
    if (rc != 0) {
            printf("Connection error.");
            //return -1;
    }
    printf("Successfully connected!\r\n");

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "Mbed";

    if ((rc = client.connect(data)) != 0){
            printf("Fail to connect MQTT\r\n");
    }
    if (client.subscribe(topic, MQTT::QOS0, messageArrived) != 0){
            printf("Fail to subscribe\r\n");
    }

    mqtt_thread.start(callback(&mqtt_queue, &EventQueue::dispatch_forever));
    
    int num_call = 0;
    int tilt_enable = true;
    tilt_angle = 0;
    initial_ref_acc();
    while (tilt_enable) {
        myled2 = 1;
        BSP_ACCELERO_AccGetXYZ(AccDataXYZ);
        tilt_angle = acc2angle();
        uLCD.locate(1,3);
        uLCD.printf("NOW Angle = %3d", tilt_angle);
        printf("ACC: x= %d, y= %d, z=%d, ANGLE = %d\n",AccDataXYZ[0],AccDataXYZ[1],AccDataXYZ[2],tilt_angle);
        if (tilt_angle > sel_angle) {
            num_call++;
            uLCD.locate(1,4);
            uLCD.printf("OVERthreshold%2d", num_call);
            mqtt_queue.call(&publish_message, &client);
        }
        if (num_call == 15) {
            tilt_enable = false;
            myled2 = 0;
            printf("start to close\n");
            close_mqtt();
      }
      ThisThread::sleep_for(100ms);
    }

    //btn3.rise(&close_mqtt);
    int num = 0;
    while (num != 5) {
            client.yield(100);
            ++num;
    }

    while (1) {
            if (closed) break;
            client.yield(500);
            ThisThread::sleep_for(500ms);
    }
    printf("Ready to close MQTT Network......\n");

    if ((rc = client.unsubscribe(topic)) != 0) {
            printf("Failed: rc from unsubscribe was %d\n", rc);
    }
    if ((rc = client.disconnect()) != 0) {
    printf("Failed: rc from disconnect was %d\n", rc);
    }

    mqttNetwork.disconnect();
    printf("Successfully closed!\n");
    //return 0;
    myled3 = 0;
    uLCD.cls();
    uLCD.reset();
}


void messageArrived(MQTT::MessageData& md) {
    MQTT::Message &message = md.message;
    char msg[300];
    sprintf(msg, "Message arrived: QoS%d, retained %d, dup %d, packetID %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf(msg);

    ThisThread::sleep_for(1000ms);
    char payload[300];
    sprintf(payload, "Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    printf(payload);
    ++arrivedcount;
}

void publish_message(MQTT::Client<MQTTNetwork, Countdown>* client) {
    message_num++;
    MQTT::Message message;
    char buff[100];
    sprintf(buff, "gesture ID:%d ----%d", gesture_index, message_num);
        message.qos = MQTT::QOS0;
        message.retained = false;
        message.dup = false;
        message.payload = (void*) buff;
        message.payloadlen = strlen(buff) + 1;
        int rc = client->publish(topic, message);

        printf("rc:  %d\r\n", rc);
        printf("Puslish message: %s\r\n", buff);
    
}

void close_mqtt() {
    closed = true;
}

// calculate acc-xyz value to tilt angle
int acc2angle(void) 
{
    double length = sqrt(AccDataXYZ[0] * AccDataXYZ[0] + AccDataXYZ[1] * AccDataXYZ[1] + AccDataXYZ[2] * AccDataXYZ[2]);
    double theta = acos((AccDataXYZ[0]* ref_ACC[0] + AccDataXYZ[1] * ref_ACC[1] + AccDataXYZ[2] * ref_ACC[2]) / (length * ref_length));
    int angle = (int)(theta * 180 / 3.1415926);

  //  double deg = atan2(abs(dz), sqrt(dx * dx + dy * dy));
  //  int angle = (int)(90 - (deg * 180 / 3.1415926));
    return angle;
}

// find reference acc vector (must start accmeter first)
void initial_ref_acc(void)
{
    printf("initial ref\n");

    
    myled1 = 1;
    for (int j = 0; j < 20; j++) {
        ThisThread::sleep_for(100ms);
        myled1 = !myled1;
    }
    myled1 = 1;
    ThisThread::sleep_for(200ms);
    BSP_ACCELERO_AccGetXYZ(ref_ACC);
    ref_length = sqrt(ref_ACC[0]*ref_ACC[0]+ref_ACC[1]*ref_ACC[1]+ref_ACC[2]*ref_ACC[2]);
    myled1 = 0;
}

void show_data (Arguments *in, Reply *out)
{
  
  for (int j = 0; j < 6 ;j++) {
    printf("%d\n", data_change_dir[j]);
  }

}