#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// Include the config file
#include "config.h"

// Function prototypes
void connectToWiFi();
String getCurrentDate();
void displayMenu();
void changeMeal(String category);

// NTP client setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);  // Adjust timezone offset as needed

// Initialize the TFT display
TFT_eSPI tft = TFT_eSPI();

// Touchscreen pins (adjust if needed)
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_DO
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// Touchscreen calibration values (adjust as needed)
#define TS_MINX 200
#define TS_MAXX 3800
#define TS_MINY 200
#define TS_MAXY 3800

// Variables
String currentDate;

void setup() {
  Serial.begin(115200);

  // Initialize touchscreen SPI and begin
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(1);  // Adjust rotation as needed

  // Initialize TFT display
  tft.init();
  tft.setRotation(1);  // Landscape mode
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // Connect to Wi-Fi
  connectToWiFi();

  // Start NTP client
  timeClient.begin();
  timeClient.update();

  // Get current date
  currentDate = getCurrentDate();

  // Fetch and display the menu
  displayMenu();
}

void loop() {
  // Handle touchscreen input
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();

    // Map touchscreen coordinates to screen coordinates
    int x = map(p.x, TS_MINX, TS_MAXX, 0, SCREEN_WIDTH);
    int y = map(p.y, TS_MINY, TS_MAXY, 0, SCREEN_HEIGHT);

    // If your touch coordinates are inverted, uncomment the following lines:
    // x = SCREEN_WIDTH - x;
    // y = SCREEN_HEIGHT - y;

    Serial.printf("Touch at x=%d, y=%d\n", x, y);

    // Check which 'Change' button was touched
    if (x > 250 && x < 310) {
      if (y > 40 && y < 65) {
        // Breakfast 'Change' button touched
        Serial.println("Breakfast 'Change' button touched");
        changeMeal("breakfast");
      } else if (y > 80 && y < 105) {
        // Lunch 'Change' button touched
        Serial.println("Lunch 'Change' button touched");
        changeMeal("lunch");
      } else if (y > 120 && y < 145) {
        // Dinner 'Change' button touched
        Serial.println("Dinner 'Change' button touched");
        changeMeal("dinner");
      }
    }

    delay(300);  // Debounce delay
  }
}

void connectToWiFi() {
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Connecting to WiFi...", 5, 10, 2);
  WiFi.begin(ssid, password);

  int maxAttempts = 20;
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi.");
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Connected to WiFi!", 5, 10, 2);
  } else {
    Serial.println("\nFailed to connect to WiFi.");
    tft.fillScreen(TFT_BLACK);
    tft.drawString("WiFi Connection Failed!", 5, 10, 2);
  }
}

String getCurrentDate() {
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();

  // Convert epoch time to date
  struct tm *ptm = gmtime((time_t *)&epochTime);

  int year = ptm->tm_year + 1900;
  int month = ptm->tm_mon + 1;
  int day = ptm->tm_mday;

  char dateString[11];  // YYYY-MM-DD
  snprintf(dateString, sizeof(dateString), "%04d-%02d-%02d", year, month, day);

  return String(dateString);
}

void displayMenu() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = String("http://") + serverIP + ":" + String(serverPort) + "/getMenu?date=" + currentDate;
    // print the url
    Serial.println(url);

    http.begin(url);
    int httpCode = http.GET();
    //print the response code
    Serial.println(httpCode);

    if (httpCode == 200) {
      String payload = http.getString();
      Serial.println("Received payload:");
      Serial.println(payload);

      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        const char* breakfast = doc["breakfast"] | "N/A";
        const char* lunch = doc["lunch"] | "N/A";
        const char* dinner = doc["dinner"] | "N/A";

        // Display the menu on the TFT screen
        tft.fillScreen(TFT_BLACK);
        int y = 10;
        tft.drawCentreString("Menu for " + currentDate, SCREEN_WIDTH / 2, y, 2);
        y += 30;

        // Breakfast
        tft.drawString("Breakfast:", 5, y, 2);
        tft.drawString(breakfast, 110, y, 2);
        // Draw 'Change' button for breakfast
        tft.fillRect(250, y, 60, 25, TFT_BLUE);
        tft.drawRect(250, y, 60, 25, TFT_WHITE);
        tft.setTextColor(TFT_WHITE, TFT_BLUE);
        tft.drawCentreString("Change", 280, y + 4, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        y += 40;

        // Lunch
        tft.drawString("Lunch:", 5, y, 2);
        tft.drawString(lunch, 110, y, 2);
        // Draw 'Change' button for lunch
        tft.fillRect(250, y, 60, 25, TFT_BLUE);
        tft.drawRect(250, y, 60, 25, TFT_WHITE);
        tft.setTextColor(TFT_WHITE, TFT_BLUE);
        tft.drawCentreString("Change", 280, y + 4, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        y += 40;

        // Dinner
        tft.drawString("Dinner:", 5, y, 2);
        tft.drawString(dinner, 110, y, 2);
        // Draw 'Change' button for dinner
        tft.fillRect(250, y, 60, 25, TFT_BLUE);
        tft.drawRect(250, y, 60, 25, TFT_WHITE);
        tft.setTextColor(TFT_WHITE, TFT_BLUE);
        tft.drawCentreString("Change", 280, y + 4, 2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);

      } else {
        Serial.println("Failed to parse JSON");
        tft.fillScreen(TFT_BLACK);
        tft.drawString("Error parsing menu data", 5, 10, 2);
      }
    } else {
      Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
      tft.fillScreen(TFT_BLACK);
      tft.drawString("Failed to fetch menu", 5, 10, 2);
    }

    http.end();
  } else {
    Serial.println("WiFi not connected");
    connectToWiFi();
  }
}

void changeMeal(String category) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = String("http://") + serverIP + ":" + String(serverPort) + "/changeMeal";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    // Prepare the request body
    StaticJsonDocument<200> doc;
    doc["date"] = currentDate;
    doc["category"] = category;
    doc["newMeal"] = "random";  // Indicate that we want a random new meal

    String requestBody;
    serializeJson(doc, requestBody);

    int httpCode = http.POST(requestBody);

    if (httpCode == 200) {
      String response = http.getString();
      Serial.println("Meal changed successfully:");
      Serial.println(response);

      // Provide visual feedback
      tft.fillRect(0, SCREEN_HEIGHT - 20, SCREEN_WIDTH, 20, TFT_BLACK);  // Clear previous message
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.drawCentreString("Meal changed successfully!", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 18, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);

      // Refresh the menu display after a short delay
      delay(1000);
      displayMenu();
    } else {
      Serial.printf("Failed to change meal, error: %s\n", http.errorToString(httpCode).c_str());
      tft.fillRect(0, SCREEN_HEIGHT - 20, SCREEN_WIDTH, 20, TFT_BLACK);  // Clear previous message
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawCentreString("Failed to change meal", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 18, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }

    http.end();
  } else {
    Serial.println("WiFi not connected");
    connectToWiFi();
  }
}
