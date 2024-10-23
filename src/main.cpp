#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

// Include the config file
#include "config.h"

// Function prototypes
void connectToWiFi();
String getCurrentDate();
void displayMenu();
void changeMeal(String category);
void update_time(lv_timer_t * timer);

// NTP client setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET * 3600, 60000);  // Adjust timezone offset as needed

// Touchscreen pins
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// Screen dimensions
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

// Touchscreen calibration values (adjust as needed)
#define TS_MINX 200
#define TS_MAXX 3700
#define TS_MINY 240
#define TS_MAXY 3800

// Variables
String currentDate;

// LVGL related
#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10)
uint32_t draw_buf[DRAW_BUF_SIZE / 4]; // Adjusted to match the working example

// Time label
lv_obj_t *time_label;

// Forward declarations
void touchscreen_read(lv_indev_t * indev, lv_indev_data_t * data);

void setup() {
  Serial.begin(115200);
  Serial.println("Setup started");

  // Initialize LVGL
  lv_init();
  lv_log_register_print_cb([](lv_log_level_t level, const char * buf) {
    Serial.println(buf);
  });

  // Start the SPI for the touchscreen and init the touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(1);  // Adjust as needed

  // Create a display object
  lv_disp_t * disp;
  disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
  lv_disp_set_rotation(disp, LV_DISP_ROTATION_90);

  // Initialize an LVGL input device object (Touchscreen)
  lv_indev_t * indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchscreen_read);

  // Connect to Wi-Fi
  connectToWiFi();

  // Start NTP client
  timeClient.begin();
  timeClient.update();

  // Get current date
  currentDate = getCurrentDate();

  // Fetch and display the menu
  displayMenu();

  // Create a timer to update the time every second
  lv_timer_create(update_time, 1000, NULL);
}

void loop() {
  lv_task_handler();  // Let the GUI do its work
  lv_tick_inc(5);     // Tell LVGL how much time has passed
  delay(5);           // Let this time pass
}

// Updated touchscreen_read function
void touchscreen_read(lv_indev_t * indev, lv_indev_data_t * data) {
  if(touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();

    // Adjust touch mapping for display rotation
    int16_t x = map(p.y, TS_MINY, TS_MAXY, 0, SCREEN_WIDTH);
    int16_t y = SCREEN_HEIGHT - map(p.x, TS_MINX, TS_MAXX, 0, SCREEN_HEIGHT);

    Serial.printf("Touch coordinates: x=%d, y=%d\n", x, y);

    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void connectToWiFi() {
  // Show a message using LVGL
  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, "Connecting to WiFi...");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
  lv_task_handler();
  delay(100);

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

    lv_label_set_text(label, "Connected to WiFi!");
  } else {
    Serial.println("\nFailed to connect to WiFi.");
    lv_label_set_text(label, "WiFi Connection Failed!");
  }

  lv_task_handler();
  delay(1000);
  lv_obj_clean(lv_scr_act()); // Clear the screen
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

// Function to update the time label
void update_time(lv_timer_t * timer) {
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  struct tm *ptm = localtime((time_t *)&epochTime);

  char timeString[9];  // HH:MM
  snprintf(timeString, sizeof(timeString), "%02d:%02d", ptm->tm_hour, ptm->tm_min);

  lv_label_set_text(time_label, timeString);
}

void displayMenu() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = String("http://") + serverIP + ":" + String(serverPort) + "/getMenu?date=" + currentDate;
    // Print the url
    Serial.println(url);

    http.begin(url);
    int httpCode = http.GET();
    // Print the response code
    Serial.println(httpCode);

    if (httpCode == 200) {
      String payload = http.getString();
      Serial.println("Received payload:");
      Serial.println(payload);

      DynamicJsonDocument doc(512);
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        const char* breakfast = doc["breakfast"] | "N/A";
        const char* lunch = doc["lunch"] | "N/A";
        const char* dinner = doc["dinner"] | "N/A";

        // Display the menu using LVGL
        lv_obj_clean(lv_scr_act()); // Clear the screen

        // Create the time label and position it at the top right
        time_label = lv_label_create(lv_scr_act());
        lv_label_set_text(time_label, "00:00:00");
        lv_obj_align(time_label, LV_ALIGN_TOP_RIGHT, -10, 10);

        lv_obj_t *title_label = lv_label_create(lv_scr_act());
        lv_label_set_text_fmt(title_label, "Menu for %s", currentDate.c_str());
        lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
        lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 10, 10);

        int y_offset = 50;

        // Define layout parameters
        int left_margin = 10;
        int right_margin = 10;
        int button_width = 40;
        int spacing = 10;
        int meal_label_width = SCREEN_WIDTH - left_margin - right_margin - button_width - spacing * 2;

        // Breakfast
        lv_obj_t *breakfast_label = lv_label_create(lv_scr_act());
        lv_label_set_text(breakfast_label, "Breakfast:");
        lv_obj_align(breakfast_label, LV_ALIGN_TOP_LEFT, left_margin, y_offset);

        lv_obj_t *breakfast_meal_label = lv_label_create(lv_scr_act());
        lv_label_set_long_mode(breakfast_meal_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(breakfast_meal_label, meal_label_width);
        lv_obj_set_style_text_line_space(breakfast_meal_label, 2, 0); // Add 2 pixels line spacing
        lv_label_set_text(breakfast_meal_label, breakfast);
        lv_obj_align_to(breakfast_meal_label, breakfast_label, LV_ALIGN_OUT_RIGHT_TOP, spacing, 0);

        // Create 'Change' button with symbol for breakfast
        lv_obj_t *breakfast_btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(breakfast_btn, button_width, 40);
        lv_obj_align(breakfast_btn, LV_ALIGN_TOP_RIGHT, -right_margin, y_offset - 10);
        lv_obj_add_event_cb(breakfast_btn, [](lv_event_t *e){
          lv_event_code_t code = lv_event_get_code(e);
          if(code == LV_EVENT_CLICKED) {
            Serial.println("Breakfast button clicked");
            changeMeal("breakfast");
          }
        }, LV_EVENT_ALL, NULL);

        lv_obj_t *breakfast_btn_label = lv_label_create(breakfast_btn);
        lv_label_set_text(breakfast_btn_label, LV_SYMBOL_REFRESH);
        lv_obj_center(breakfast_btn_label);

        y_offset += breakfast_meal_label->coords.y2 - breakfast_meal_label->coords.y1 + 20;

        // Lunch
        lv_obj_t *lunch_label = lv_label_create(lv_scr_act());
        lv_label_set_text(lunch_label, "Lunch:");
        lv_obj_align(lunch_label, LV_ALIGN_TOP_LEFT, left_margin, y_offset);

        lv_obj_t *lunch_meal_label = lv_label_create(lv_scr_act());
        lv_label_set_long_mode(lunch_meal_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lunch_meal_label, meal_label_width);
        lv_obj_set_style_text_line_space(lunch_meal_label, 2, 0); // Add 2 pixels line spacing
        lv_label_set_text(lunch_meal_label, lunch);
        lv_obj_align_to(lunch_meal_label, lunch_label, LV_ALIGN_OUT_RIGHT_TOP, spacing, 0);

        // Create 'Change' button with symbol for lunch
        lv_obj_t *lunch_btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(lunch_btn, button_width, 40);
        lv_obj_align(lunch_btn, LV_ALIGN_TOP_RIGHT, -right_margin, y_offset - 10);
        lv_obj_add_event_cb(lunch_btn, [](lv_event_t *e){
          lv_event_code_t code = lv_event_get_code(e);
          if(code == LV_EVENT_CLICKED) {
            Serial.println("Lunch button clicked");
            changeMeal("lunch");
          }
        }, LV_EVENT_ALL, NULL);

        lv_obj_t *lunch_btn_label = lv_label_create(lunch_btn);
        lv_label_set_text(lunch_btn_label, LV_SYMBOL_REFRESH);
        lv_obj_center(lunch_btn_label);

        y_offset += lunch_meal_label->coords.y2 - lunch_meal_label->coords.y1 + 20;

        // Dinner
        lv_obj_t *dinner_label = lv_label_create(lv_scr_act());
        lv_label_set_text(dinner_label, "Dinner:");
        lv_obj_align(dinner_label, LV_ALIGN_TOP_LEFT, left_margin, y_offset);

        lv_obj_t *dinner_meal_label = lv_label_create(lv_scr_act());
        lv_label_set_long_mode(dinner_meal_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(dinner_meal_label, meal_label_width);
        lv_obj_set_style_text_line_space(dinner_meal_label, 2, 0); // Add 2 pixels line spacing
        lv_label_set_text(dinner_meal_label, dinner);
        lv_obj_align_to(dinner_meal_label, dinner_label, LV_ALIGN_OUT_RIGHT_TOP, spacing, 0);

        // Create 'Change' button with symbol for dinner
        lv_obj_t *dinner_btn = lv_btn_create(lv_scr_act());
        lv_obj_set_size(dinner_btn, button_width, 40);
        lv_obj_align(dinner_btn, LV_ALIGN_TOP_RIGHT, -right_margin, y_offset - 10);
        lv_obj_add_event_cb(dinner_btn, [](lv_event_t *e){
          lv_event_code_t code = lv_event_get_code(e);
          if(code == LV_EVENT_CLICKED) {
            Serial.println("Dinner button clicked");
            changeMeal("dinner");
          }
        }, LV_EVENT_ALL, NULL);

        lv_obj_t *dinner_btn_label = lv_label_create(dinner_btn);
        lv_label_set_text(dinner_btn_label, LV_SYMBOL_REFRESH);
        lv_obj_center(dinner_btn_label);

      } else {
        String errorMsg = String("Error parsing menu data: ") + error.c_str();
        Serial.println(errorMsg);
        lv_obj_t *label = lv_label_create(lv_scr_act());
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_label_set_text_fmt(label, "%s", errorMsg.c_str());
        lv_obj_set_width(label, SCREEN_WIDTH - 20);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
      }
    } else {
      String errorMsg = "HTTP GET failed, error: " + http.errorToString(httpCode);
      Serial.println(errorMsg);
      lv_obj_t *label = lv_label_create(lv_scr_act());
      lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
      lv_label_set_text_fmt(label, "%s", errorMsg.c_str());
      lv_obj_set_width(label, SCREEN_WIDTH - 20);
      lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
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
    DynamicJsonDocument doc(200);
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
      lv_obj_t *msg_label = lv_label_create(lv_scr_act());
      lv_label_set_text(msg_label, "Meal changed successfully!");
      lv_obj_align(msg_label, LV_ALIGN_BOTTOM_MID, 0, -10);
      lv_task_handler();
      delay(1000);
      lv_obj_del(msg_label);

      // Refresh the menu display after a short delay
      displayMenu();
    } else {
      String errorMsg = "Failed to change meal, error: " + http.errorToString(httpCode);
      Serial.println(errorMsg);
      lv_obj_t *msg_label = lv_label_create(lv_scr_act());
      lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
      lv_label_set_text_fmt(msg_label, "%s", errorMsg.c_str());
      lv_obj_set_width(msg_label, SCREEN_WIDTH - 20);
      lv_obj_align(msg_label, LV_ALIGN_CENTER, 0, 0);
      lv_task_handler();
      delay(3000);
      lv_obj_del(msg_label);
    }

    http.end();
  } else {
    Serial.println("WiFi not connected");
    connectToWiFi();
  }
}
