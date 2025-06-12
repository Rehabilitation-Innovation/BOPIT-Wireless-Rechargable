#include <Arduino.h>
#include <WiFi.h>
#include <esp_event.h>
#include <esp_mac.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#define SLEEP_TIMEOUT 5 * 60 * 1000  // 5 mins * 60s/min * 1000ms/s

#define BOPIT_ROLE "PULL" // PUSH, PULL, TWIST [Change appropriatly]
#define INPUT_PIN 32
#define LED_PIN 4

uint8_t s_master_address[] = { 0x90, 0x15, 0x06, 0x7a, 0xda, 0x9c };

void readMacAddress() {
    uint8_t baseMac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
    if (ret == ESP_OK) {
        Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n", baseMac[0], baseMac[1],
            baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
    }
    else {
        Serial.println("Failed to read MAC address");
    }
}

TaskHandle_t s_wifi_task;
TaskHandle_t s_bopit_task;

bool s_button_flag = false;

esp_now_peer_info_t peerInfo;

void OnDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Transmit Success"
        : "Transmit Fail");
}

// variables to keep track of the timing of recent interrupts
unsigned long button_time = 0;
unsigned long last_button_time = 0;

void IRAM_ATTR ISR_ButtonPress() {

    button_time = millis();
    if ((button_time - last_button_time) > 500) {

        s_button_flag = true;
        Serial.println("Button Pressed");
        last_button_time = button_time;
    }

}

unsigned long awaken_time = 0;
unsigned long last_awaken_time = millis();

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t* mac, const uint8_t* incomingData, int len) {
    Serial.print("Bytes received: ");
    Serial.println(len);
    Serial.println((const char*)incomingData);

    if (String("LED_" BOPIT_ROLE).equals((const char*)incomingData)) {
        // last_awaken_time = millis();
        // digitalWrite(4, HIGH);
        analogWrite(LED_PIN, 20);
        Serial.println("Got Light up");
    }

    if (String("LED_" BOPIT_ROLE "_OFF").equals((const char*)incomingData)) {
        // last_awaken_time = millis();
        // digitalWrite(4, LOW);
        analogWrite(LED_PIN, 0);
        Serial.println("Got Light down");
    }
}

void espNOWMeshTask(void* t_parameters) {
    pinMode(INPUT_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);

    WiFi.mode(WIFI_STA);
    WiFi.begin();

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    esp_now_register_send_cb(OnDataSent);

    memcpy(peerInfo.peer_addr, s_master_address, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    // Add peer
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
    attachInterrupt(INPUT_PIN, ISR_ButtonPress, FALLING);
    for (;;) {
        if (s_button_flag && digitalRead(INPUT_PIN) == LOW) {
            Serial.println("Button Press Detected");
            esp_err_t result =
                esp_now_send(s_master_address, (const uint8_t*)BOPIT_ROLE,
                    6);  // change this per box

            if (result == ESP_OK) {
                Serial.println("Sent with success");
            }
            else {
                Serial.println("Error sending the data");
            }
            s_button_flag = false;
            // digitalWrite(4, LOW);
            analogWrite(LED_PIN, 0);
        }

        // // Send message via ESP-NOW
        // esp_err_t result =
        //     esp_now_send(broadcastAddress, (const uint8_t*)"Hello", 6);

        // if (result == ESP_OK) {
        //   Serial.println("Sent with success");
        // } else {
        //   Serial.println("Error sending the data");
        // }

        vTaskDelay(5);
    }
}

void bopItTask(void* t_parameters) {
    Serial.println("Starting BopIt Handler");

    for (;;) {
        // if ((millis() - last_awaken_time) >= SLEEP_TIMEOUT) {
        //   esp_deep_sleep_start();
        // }
        vTaskDelay(1000);
    }
}

void setup() {
    // Start a listen process and see whp talks

    Serial.begin(115200);
    Serial.println("Creating tasks");
    xTaskCreatePinnedToCore(espNOWMeshTask, "ESP NOW Responder", 10000, NULL, 1,
        &s_wifi_task, 0);
    xTaskCreatePinnedToCore(bopItTask, "BopIt Responder", 10000, NULL, 1,
        &s_bopit_task, 1);
    Serial.println("Starting scheduler");
}

void loop() {
    // put your main code here, to run repeatedly:
}
