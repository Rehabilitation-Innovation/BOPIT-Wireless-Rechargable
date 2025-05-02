#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_event.h>
#include <esp_mac.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/timers.h>

uint8_t s_master_address[] = {0xac, 0x0b, 0xfb, 0x6c, 0x7a, 0x00};

#define PUSH_PIN 27
#define PULL_PIN 26
#define TWIST_PIN 25

#define TRANSMITTER_SLAVE_ADDR 0x1a

void readMacAddress() {
  uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK) {
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n", baseMac[0], baseMac[1],
                  baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
  } else {
    Serial.println("Failed to read MAC address");
  }
}

TaskHandle_t s_wifi_task;
TaskHandle_t s_bopit_task;

bool s_button_flag = false;
bool esp_now_started = false;
uint8_t broadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

esp_now_peer_info_t peerInfo;

void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Transmit Success"
                                                : "Transmit Fail");
}

// variables to keep track of the timing of recent interrupts
unsigned long button_time = 0;
unsigned long last_button_time = 0;
uint32_t pushtimer = 0;
uint32_t pulltimer = 0;
uint32_t twisttimer = 0;

uint8_t led_to_light_up = 0;

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len) {
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.println((const char*)incomingData);

  if (String("PUSH").equals((const char*)incomingData)) {
    digitalWrite(PUSH_PIN, LOW);
    pushtimer = millis();
  } else if (String("PULL").equals((const char*)incomingData)) {
    digitalWrite(PULL_PIN, LOW);
    pulltimer = millis();
  } else if (String("TWIST").equals((const char*)incomingData)) {
    digitalWrite(TWIST_PIN, LOW);
    twisttimer = millis();
  }
}

void espNOWMeshTask(void* t_parameters) {
  // pinMode(32, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.begin();

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);

  memcpy(peerInfo.peer_addr, broadcast, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  // // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  Serial.print("[DEFAULT] ESP32 Board MAC Address: ");
  readMacAddress();
  Serial.println("Starting ESP Now");

  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_started = true;
  for (;;) {
        // if (s_button_flag) {
    //   Serial.println("Button Press Detected");
    //   esp_err_t result =
    //       esp_now_send(s_master_address, (const uint8_t*)"Hello", 6);

    //   if (result == ESP_OK) {
    //     Serial.println("Sent with success");
    //   } else {
    //     Serial.println("Error sending the data");
    //   }
    //   s_button_flag = false;
    // }

    // // Send message via ESP-NOW
    // esp_err_t result =
    //     esp_now_send(broadcastAddress, (const uint8_t*)"Hello", 6);

    // if (result == ESP_OK) {
    //   Serial.println("Sent with success");
    // } else {
    //   Serial.println("Error sending the data");
    // }
    if (LOW == digitalRead(PUSH_PIN)) {
      if (100 <= (millis() - pushtimer)) {
        digitalWrite(PUSH_PIN, HIGH);
        pushtimer = 0;
      }
    }

    if (LOW == digitalRead(PULL_PIN)) {
      if (100 <= (millis() - pulltimer)) {
        digitalWrite(PULL_PIN, HIGH);
        pulltimer = 0;
      }
    }

    if (LOW == digitalRead(TWIST_PIN)) {
      if (100 <= (millis() - twisttimer)) {
        digitalWrite(TWIST_PIN, HIGH);
        twisttimer = 0;
      }
    }

    vTaskDelay(10);
  }
}

void onRequest() {
  // Wire.print(i++);
  // Wire.print(" Packets.");
  Serial.println("onRequest");
  Serial.println();
}

void onReceive(int len) {
  Serial.printf("onReceive[%d]: ", len);
  // while (Wire.available()) {

  //   // Serial.write(Wire.read());
  // }

  if (1 != len) {
    Serial.println("Data error i2c");
    return;
  }
  Wire.readBytes(&led_to_light_up, 1);
  Serial.println("Got light up command");

  // char* a = new char[len];

  // Wire.readBytesUntil('\n', a, len);
  Serial.println();

  if (0 != led_to_light_up) {
    esp_err_t result;
    switch (led_to_light_up) {
      case 1:
        result = esp_now_send(broadcast, (const uint8_t*)"LED_PUSH", 9);
        break;
      case 2:
        result = esp_now_send(broadcast, (const uint8_t*)"LED_PULL", 9);
        break;
      case 3:
        result = esp_now_send(broadcast, (const uint8_t*)"LED_TWIST", 10);
        break;
      case 4:
        result = esp_now_send(broadcast, (const uint8_t*)"LED_PUSH_OFF", 13);
        break;
      case 5:
        result = esp_now_send(broadcast, (const uint8_t*)"LED_PULL_OFF", 13);
        break;
      case 6:
        result = esp_now_send(broadcast, (const uint8_t*)"LED_TWIST_OFF", 14);
        break;
      default:
        Serial.println("Unrecognized LED");
    }

    led_to_light_up = 0;

    if (result == ESP_OK) {
      Serial.println("[LED TASK]Sent with success");
    } else {
      Serial.println("[LED TASK]Error sending the data");
    }
  }
}

void bopItTask(void* t_parameters) {
  Serial.println("Starting BopIt Handler");

  while (!esp_now_started) {
    vTaskDelay(100);
  }

  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);
  Wire.begin((uint8_t)TRANSMITTER_SLAVE_ADDR);

  for (;;) {
    vTaskDelay(100);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Creating tasks");

  pinMode(PUSH_PIN, OUTPUT);
  pinMode(TWIST_PIN, OUTPUT);
  pinMode(PULL_PIN, OUTPUT);

  digitalWrite(PUSH_PIN, HIGH);

  digitalWrite(TWIST_PIN, HIGH);

  digitalWrite(PULL_PIN, HIGH);

  xTaskCreatePinnedToCore(espNOWMeshTask, "ESP NOW Responder", 10000, NULL, 1,
                          &s_wifi_task, 0);
  xTaskCreatePinnedToCore(bopItTask, "BopIt Responder", 10000, NULL, 1,
                          &s_bopit_task, 1);
  Serial.println("Starting scheduler");
}

void loop() {
  // put your main code here, to run repeatedly:
}
