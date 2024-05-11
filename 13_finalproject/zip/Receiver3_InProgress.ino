#include <esp_now.h>
#include <WiFi.h>

// Structure to receive countdown data and reset signal
typedef struct struct_countdown {
    unsigned long countdownDuration;
    bool isCountdownStarted;
    bool isResetPressed; // New flag to indicate reset button press
} struct_countdown;

// Create a struct_countdown object
struct_countdown countdownData;

const int ledPin = 27;
unsigned long timeToTurnOff = 0;

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    memcpy(&countdownData, incomingData, sizeof(countdownData));

    if (countdownData.isResetPressed) { // If reset flag is received
        digitalWrite(ledPin, LOW);
        timeToTurnOff = 0;
    } else if (countdownData.isCountdownStarted) {
        digitalWrite(ledPin, HIGH);
        timeToTurnOff = millis() + countdownData.countdownDuration;
    } else {
        digitalWrite(ledPin, LOW);
        timeToTurnOff = 0;
    }
}

void setup() {
    pinMode(ledPin, OUTPUT);
    // Initialize Serial Monitor
    Serial.begin(115200);

    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Once ESPNow is successfully Init, we will register for recv CB to get recv packer info
    esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
    if (millis() >= timeToTurnOff) {
        digitalWrite(ledPin, LOW);
    }
}