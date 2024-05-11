
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "AiEsp32RotaryEncoder.h"
#include <ESP32Encoder.h>
#include <esp_now.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <UrlEncode.h>

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET 4      // Reset pin for the OLED display
#define OLED_ADDR 0x3C    // I2C address for the OLED display
#define ROTARY_ENCODER_A_PIN 5  // Rotary encoder A pin
#define ROTARY_ENCODER_B_PIN 18 // Rotary encoder B pin
#define ROTARY_ENCODER_BUTTON_PIN 19  // Rotary encoder button pin
#define ROTARY_ENCODER_VCC_PIN -1  // Rotary encoder VCC pin (set to -1 if not used)
#define ROTARY_ENCODER_STEPS 4  // Number of steps for the rotary encoder
#define RESET_BUTTON 4    // Reset button pin

//I'm wiring the extra button to pin D26

const char* ssid = "MAKERSPACE";
const char* password = "12345678";

String phoneNumber = "+12066177115";
String apiKey = "6215313";


Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);

unsigned long targetTime;  // Variable to store the countdown duration
bool isCountdownStarted = false;  // Flag to track if the countdown has started
int menuSelection = 0;    // Initialize menu selection to 0
int prevMenuSelection = 0; // Initialize previous menu selection to 0
bool isMenuOptionSelected = false; // Flag to track if a menu option has been selected
int customCountdownMinutes = 5;  // Initialize custom countdown duration to 5 minutes
int rotaryPosition = 0;    // Initialize rotary encoder position

const char* menuOptions[] = {"Zoom", "Meditate", "Custom", "Sneaky Link"};
const int menuOptionTimes[] = {45, 10, 5, 3}; // Preset times for each menu option in minutes
const int menuOptionCount = sizeof(menuOptions) / sizeof(menuOptions[0]);

// Structure to send countdown data
typedef struct struct_countdown {
    unsigned long countdownDuration;
    bool isCountdownStarted;
    bool isResetPressed; // New flag to indicate reset button press
} struct_countdown;

// Create a struct_countdown object
struct_countdown countdownData;

// REPLACE WITH YOUR RECEIVER MAC Address
uint8_t broadcastAddress[] = {0x08, 0xD1, 0xF9, 0x38, 0x16, 0x74};

esp_now_peer_info_t peerInfo;

void IRAM_ATTR readEncoderISR() {
  rotaryEncoder.readEncoder_ISR();
}

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // Serial.print("\r\nLast Packet Send Status:\t");
    // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {
  Serial.begin(9600);

  // Initialize the OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 allocation failed");
    for (;;)
      ;  // Don't proceed, loop forever
  }

  display.clearDisplay();
  display.setTextSize(1);    // Set text size
  display.setTextColor(WHITE);  // Set text color to white

  // Initialize the rotary encoder
  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  rotaryEncoder.setBoundaries(0, 99, false);  // Set boundaries for the rotary encoder (0 - number of menu options)
  rotaryEncoder.setAcceleration(100);  // Enable acceleration for faster rotation

  pinMode(RESET_BUTTON, INPUT_PULLUP);  // Set reset button pin as input with internal pull-up

  // Display the initial menu option
  displayMenuOption();

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to get the status of Transmitted packet
  esp_now_register_send_cb(OnDataSent);

  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // Initialize countdown data
  countdownData.countdownDuration = 0;
  countdownData.isCountdownStarted = false;
  countdownData.isResetPressed = false;

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
}

void sendWhatsAppMessage(const char* message) {
  WiFiClient client;
  HTTPClient http;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return;
  }

  String encodedMessage = urlEncode(message);
  String url = "https://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&apikey=" + apiKey + "&text=" + urlEncode(message);    
 
  http.begin(url);

  int httpResponseCode = http.GET();
  if (httpResponseCode == 200) {
    Serial.println("WhatsApp message sent successfully");
  } else {
    Serial.println("Error sending WhatsApp message");
    Serial.print("HTTP response code: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}


void loop() {
  if (!isCountdownStarted && !isMenuOptionSelected) {
    int newPosition = rotaryEncoder.readEncoder();
    if (newPosition != menuSelection) {
      prevMenuSelection = menuSelection;
      menuSelection = newPosition;
      Serial.println(menuSelection);
      displayMenuOption();
    }
  } else if (isMenuOptionSelected && menuSelection == 2) { // If "Custom" option is selected
    int newPosition = rotaryEncoder.readEncoder();
    if (newPosition != rotaryPosition) {
      rotaryPosition = newPosition;  // Update rotary position
      customCountdownMinutes = rotaryPosition + 1;  // Set custom countdown duration based on rotary position
      displayCountdownDuration();
      Serial.print("Rotary Position: ");
      Serial.println(customCountdownMinutes);  // Print custom countdown duration in the serial monitor
    }
  }


  // Check if the rotary encoder button is pressed
  if (rotaryEncoder.isEncoderButtonClicked()) {
    delay(200);  // Debounce delay

    if (!isCountdownStarted) {
      if (!isMenuOptionSelected) {
        isMenuOptionSelected = true;
        displayCountdownDuration();
      } else {
        isCountdownStarted = true;

        // Convert minutes to milliseconds and set the targetTime
        unsigned long countdownDuration;
        if (menuSelection == 2) { // If "Custom" option is selected
          countdownDuration = customCountdownMinutes * 60000UL;
        } else {
          countdownDuration = menuOptionTimes[menuSelection] * 60000UL;
        }
        targetTime = millis() + countdownDuration;

        // Send WhatsApp message
        const char* message;
        switch (menuSelection) {
          case 0: // Zoom
            message = "Finn is on a Zoom call. Please keep the noise down for the next 45 mintues.";
            break;
          case 1: // Meditate
            message = "Finn is meditating. Please keep the noise down for the next 10 minutes.";
            break;
          case 2: // Custom
            message = "Finn has activated his BusyButton. You've been warned!";
            break;
          case 3: // Sneaky Link
            message = "Clear the 9.";
            break;
          default:
            message = "Finn has started a countdown timer. Please keep the noise down until the red light goes out.";
            break;
        }
        sendWhatsAppMessage(message);
      }
    }
  }

  // Check if the reset button is pressed
  if (digitalRead(RESET_BUTTON) == LOW) {
    delay(200);  // Debounce delay
    resetCountdown();
  }

  if (isCountdownStarted) {
    unsigned long currentTime = millis();
    long remainingTime = targetTime - currentTime;

    if (remainingTime <= 0) { // If the timer has reached zero
      // Reset variables and display the main menu
      isCountdownStarted = false;
      isMenuOptionSelected = false;
      menuSelection = 0;
      displayMenuOption();
      countdownData.isCountdownStarted = false; // Set countdown flag to false
      countdownData.isResetPressed = false; // Set reset flag to false
    } else {
      // Clear the display and print the remaining time
      display.clearDisplay();
      display.setCursor(10, 10);  // Set cursor position
      display.print(formatTime(remainingTime));
      display.display();

      // Update countdown data
      countdownData.countdownDuration = remainingTime;
      countdownData.isCountdownStarted = true;
      countdownData.isResetPressed = false;

      // Send countdown data to the Receiver
      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&countdownData, sizeof(countdownData));
      if (result != ESP_OK) {
        Serial.println("Error sending data");
      }
    }
  }

  delay(100);  // Short delay to prevent high CPU usage
}


// Function to format the time as "mm:ss"
char* formatTime(long milliseconds) {
  static char formattedTime[9];  // Buffer to store formatted time string
  long seconds = abs(milliseconds) / 1000;
  long hours = seconds / 3600;
  long minutes = (seconds % 3600) / 60;
  long secs = seconds % 60;

  sprintf(formattedTime, "%02ld:%02ld:%02ld", hours, minutes, secs);

  if (milliseconds < 0) {
    formattedTime[0] = '-';  // Add a minus sign for negative values
  }

  return formattedTime;
}

void displayMenuOption() {
  display.clearDisplay();
  for (int i = 0; i < menuOptionCount; i++) {
    display.setCursor(0, i * 8);  // Set cursor position at the beginning of each line
    if (i == menuSelection % menuOptionCount) {
      display.print("* ");  // Print the selector dot
    } else {
      display.print("  ");  // Print empty space for non-selected items
    }
    display.println(menuOptions[i]);  // Print the menu item text
  }
  display.display();
}

void displayCountdownDuration() {
  display.clearDisplay();
  display.setCursor(10, 10);
  if (menuSelection == 2) { // If "Custom" option is selected
    display.print(customCountdownMinutes);
  } else {
    display.print(menuOptionTimes[menuSelection]);
  }
  display.println(" min");
  display.display();
}

void resetCountdown() {
  isCountdownStarted = false;
  isMenuOptionSelected = false;
  menuSelection = 0;
  displayMenuOption();
  countdownData.countdownDuration = 0;
  countdownData.isCountdownStarted = false;
  countdownData.isResetPressed = true; // Set reset flag to true

  // Send countdown data with reset flag to the Receiver
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&countdownData, sizeof(countdownData));
  if (result != ESP_OK) {
    Serial.println("Error sending data");
  }
}
