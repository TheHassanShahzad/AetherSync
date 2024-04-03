#include <micro_ros_arduino.h>

#include <stdio.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <micro_ros_utilities/string_utilities.h>
#include <micro_ros_utilities/type_utilities.h>
#include <std_msgs/msg/string.h>

rcl_subscription_t subscriber;
rcl_publisher_t publisher;

std_msgs__msg__String msg_sub;
std_msgs__msg__String msg_pub;

rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;
rcl_timer_t timer;

#define LED_PIN 5

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){error_loop();}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}

String string_sub; //this is the data it wants to send out. it gets this by subscribing to /mouth
String string_pub = "hey im hassan"; //this is the data it has retrieved from surrounding robots. this data will be published to /ear

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

  if (string_sub == "hey"){
    digitalWrite(LED_PIN, HIGH);
  }
  else{
    digitalWrite(LED_PIN, LOW);
  }
//  else if (string_sub == "he"){
//    digitalWrite(LED_PIN, LOW);
//  }
//  digitalWrite(LED_PIN, !strcmp(string_sub, "hey")); 

}

void timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{  
  RCLC_UNUSED(last_call_time);
  if (timer != NULL) {
    RCSOFTCHECK(rcl_publish(&publisher, &msg_pub, NULL));
//    msg_pub.data++;
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
//  const unsigned int timer_timeout = 1000;
//  RCCHECK(rclc_timer_init_default(
//    &timer,
//    &support,
//    RCL_MS_TO_NS(timer_timeout),
//    timer_callback));

  // create executor
//  RCCHECK(rclc_executor_init(&executor, &support.context, 2, &allocator));
  RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
  RCCHECK(rclc_executor_add_subscription(&executor, &subscriber, &msg_sub, &subscription_callback, ON_NEW_DATA)); 
//  RCCHECK(rclc_executor_add_timer(&executor, &timer));  

  msg_pub.data = micro_ros_string_utilities_set(msg_pub.data, string_pub.c_str());

  
//  msg_pub.data.data = "this is esp32 speaking";
}

void loop() {
  delay(100);
  RCCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100)));
  RCSOFTCHECK(rcl_publish(&publisher, &msg_pub, NULL));

}
