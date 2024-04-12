//constrict to only use 1 core
#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

#include <WiFi.h>
#include <esp_now.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for SSD1306 display connected using I2C
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

String string_sub = ""; //this is the data it wants to send out. it gets this by subscribing to /mouth
String string_pub = "heard this"; //this is the data it has retrieved from surrounding robots. this data will be published to /ear

static TaskHandle_t task_1 = NULL;
static TaskHandle_t task_2 = NULL;



void task1(void *paramater){
  
  while(1){
    broadcast("im am the one with OLED");
    delay(500);
  }
  
}

void task2(void *paramater){
  
  while(1){
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
  }
  
}

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
  Serial.printf("Received message from: %s - %s\n", macStr, buffer);

}

// callback when data is sent
void sentCallback(const uint8_t *macAddr, esp_now_send_status_t status)
{
  char macStr[18];
  formatMacAddress(macAddr, macStr, 18);

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


void setup()
{
  Serial.begin(115200);
  delay(1000);
 
    
  //Set device in STA mode to begin with
  WiFi.mode(WIFI_STA);
  Serial.println("ESPNow Example");
  // Output my MAC address - useful for later
  Serial.print("My MAC Address is: ");
  Serial.println(WiFi.macAddress());
  // shut down wifi
  WiFi.disconnect();
  // startup ESP Now
  if (esp_now_init() == ESP_OK)
  {
    Serial.println("ESPNow Init Success");
    esp_now_register_recv_cb(receiveCallback);
    esp_now_register_send_cb(sentCallback);
  }
  else
  {
    Serial.println("ESPNow Init Failed");
    delay(3000);
    ESP.restart();
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }

  // Clear the buffer.
  display.clearDisplay();
  
  xTaskCreatePinnedToCore(
    task1,
    "broadcast string",
    4096,
    NULL,
    1,
    &task_1,
    app_cpu);

  xTaskCreatePinnedToCore(
    task2,
    "display data",
    4096,
    NULL,
    1,
    &task_2,
    app_cpu);
    
}

void loop()
{

}
