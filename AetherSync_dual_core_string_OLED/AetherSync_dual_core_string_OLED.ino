#include <micro_ros_arduino.h>

#include <stdio.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <micro_ros_utilities/string_utilities.h>
#include <micro_ros_utilities/type_utilities.h>
#include <std_msgs/msg/string.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//#include <Arduino_FreeRTOS.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for SSD1306 display connected using I2C
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

rcl_subscription_t subscriber;
rcl_publisher_t publisher;

std_msgs__msg__String msg_sub;
std_msgs__msg__String msg_pub;

rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;
rcl_timer_t timer;

#define LED_PIN 23

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){error_loop();}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}

String string_sub = ""; //this is the data it wants to send out. it gets this by subscribing to /mouth
String string_pub = "heard this"; //this is the data it has retrieved from surrounding robots. this data will be published to /ear


void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength)
{
  snprintf(buffer, maxLength, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}

void receiveCallback(const uint8_t *macAddr, const uint8_t *data, int dataLen)
{
  // only allow a maximum of 250 characters in the message + a null terminating byte
  char buffer[ESP_NOW_MAX_DATA_LEN + 1];
  int msgLen = min(ESP_NOW_MAX_DATA_LEN, dataLen);
  strncpy(buffer, (const char *)data, msgLen);
  // make sure we are null terminated
  buffer[msgLen] = 0;
  // format the mac address
  char macStr[18];
  formatMacAddress(macAddr, macStr, 18);
  // debug log the message to the serial port
//  Serial.printf("Received message from: %s - %s\n", macStr, buffer);
  string_pub = buffer;
}

void sentCallback(const uint8_t *macAddr, esp_now_send_status_t status)
{
  char macStr[18];
  formatMacAddress(macAddr, macStr, 18);
//  Serial.print("Last Packet Sent to: ");
//  Serial.println(macStr);
//  Serial.print("Last Packet Send Status: ");
//  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void broadcast(const String &message)
{
  // this will broadcast a message to everyone in range
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_peer_info_t peerInfo = {};
  memcpy(&peerInfo.peer_addr, broadcastAddress, 6);
  if (!esp_now_is_peer_exist(broadcastAddress))
  {
    esp_now_add_peer(&peerInfo);
  }
  esp_err_t result = esp_now_send(broadcastAddress, (const uint8_t *)message.c_str(), message.length());
}

void error_loop(){
  while(1){
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(100);
  }
}

void subscription_callback(const void * msgin)
{  
  
  const std_msgs__msg__String * msg = (const std_msgs__msg__String *)msgin;
  
  string_sub = msg->data.data;
  
  if (string_sub == "led on"){
    digitalWrite(LED_PIN, HIGH);
  }
  else{
    digitalWrite(LED_PIN, LOW);
  }

}

void espNowTask(void * parameter) {
//  delay(1000);
  vTaskDelay(pdMS_TO_TICKS(1000));
  
  WiFi.mode(WIFI_STA);

  WiFi.disconnect();
  // startup ESP Now
  if (esp_now_init() == ESP_OK)
  {
    esp_now_register_recv_cb(receiveCallback);
    esp_now_register_send_cb(sentCallback);
  }
  else
  {
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP.restart();
  }

    // Task loop
    while(1) {
      if (string_sub.length()!= 0){
        broadcast(string_sub);
//        vTaskDelay(pdMS_TO_TICKS(500));
      }
    }
}


void setup() {
  set_microros_transports();
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  
  
  delay(2000);
  
  allocator = rcl_get_default_allocator();

  //create init_options
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

  // create node
  RCCHECK(rclc_node_init_default(&node, "AetherSync", "", &support));

  // create subscriber
  RCCHECK(rclc_subscription_init_default(
    &subscriber,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),
    "mouth"));

  RCCHECK(rclc_publisher_init_default(
    &publisher,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),
    "ear"));

  micro_ros_utilities_create_message_memory(
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),
    &msg_pub,
    (micro_ros_utilities_memory_conf_t) {}
  );

  micro_ros_utilities_create_message_memory(
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),
    &msg_sub,
    (micro_ros_utilities_memory_conf_t) {}
  );

  RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
  RCCHECK(rclc_executor_add_subscription(&executor, &subscriber, &msg_sub, &subscription_callback, ON_NEW_DATA)); 

  msg_pub.data = micro_ros_string_utilities_set(msg_pub.data, string_pub.c_str());
  
  xTaskCreatePinnedToCore(
      espNowTask,           // Function to implement the task
      "espNowTask",         // Name of the task
      10000,                // Stack size in words
      NULL,                 // Task input parameter
      3,                    // Priority of the task
      NULL,                 // Task handle
      1                     // Core to which the task should be pinned
  );
    

  
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }

  // Clear the buffer.
  display.clearDisplay();
}

void loop() {

  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);

  //display.setCursor(0, 10);
  display.print("AetherSync");

  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print("sending: ");
  display.println(string_sub);

  display.setCursor(0, 30);
  display.print("heard: ");
  display.println(string_pub);

  display.display();
  //delay(2000);
  display.clearDisplay(); 
  
  delay(100);
  RCCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100)));
  RCSOFTCHECK(rcl_publish(&publisher, &msg_pub, NULL));


  
   

}
