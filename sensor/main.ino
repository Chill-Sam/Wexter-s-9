// Library includes
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <AM2320.h>
#include <Wire.h>
#include <TimeLib.h>

// Token generation process info
#include "addons/TokenHelper.h"
// RTDB payload printing info
#include "addons/RTDBHelper.h"

// Defines
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

#define NTP_OFFSET 3600         // In seconds
#define NTP_INTERVAL 60 * 1000  // In miliseconds
#define NTP_ADDRESS "europe.pool.ntp.org"

// Firebase access
#define URL ""
#define API_KEY ""
#define TIME_LIMIT 10000
#define LT_TIME_LIMIT 300000

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

FirebaseJson json;
FirebaseJson json_lt;

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

// Paths
String tempPath = "/temperature";
String humPath = "/humidity";
String timePath = "/timestamp";

// Sensor
AM2320 sensor;

unsigned long sendDataPrevMillis = 0;
unsigned long sendDataLTPrevMillis = 0;
bool signupOK = false;

void setup() {
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Init sensor
  Wire.begin(14, 12);


  // Assign API key and URL
  config.api_key = API_KEY;
  config.database_url = URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  } else {
    Serial.println(config.signer.signupError.message.c_str());
  }

  // Callback function
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  timeClient.begin();
}

void loop() {
  // Get current timestamp
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();

  setTime(epochTime);  // Set the time using the adjusted timestamp

  // Extract and format the date
  String str_year = String(year(epochTime));
  String str_month = String(month(epochTime));
  String str_day = String(day(epochTime));

  String date = str_year + "-" + str_month + "-" + str_day;

  // Precheck before writing to server
  unsigned long isReady = (millis() - sendDataPrevMillis > TIME_LIMIT || sendDataPrevMillis == 0);  // Has been more than 10s or is first send
  if (signupOK && Firebase.ready() && isReady) {
    sendDataPrevMillis = millis();

    if (sensor.measure()) {  // Sensor error handling
      // Write JSON data
      json.set(tempPath.c_str(), String(sensor.getTemperature()));
      json.set(humPath.c_str(), String(sensor.getHumidity()));

      Firebase.RTDB.setJSON(&fbdo, "latest/", &json);

      if (millis() - sendDataLTPrevMillis > LT_TIME_LIMIT || sendDataLTPrevMillis == 0) {  // Update every 5 min
        sendDataLTPrevMillis = millis();
        json_lt.set(tempPath.c_str(), String(sensor.getTemperature()));
        json_lt.set(humPath.c_str(), String(sensor.getHumidity()));
        json_lt.set(timePath.c_str(), epochTime);

        if (Firebase.RTDB.setJSON(&fbdo, date + "/" + String(epochTime), &json_lt)) {
          Serial.println("Data sent successfully to Firebase.");
        } else {
          Serial.printf("Failed to send data: %s\n", fbdo.errorReason().c_str());
        }
      }

    } else {  // error has occured
      int errorCode = sensor.getErrorCode();
      switch (errorCode) {
        case 1: Serial.println("ERR: Sensor is offline"); break;
        case 2: Serial.println("ERR: CRC validation failed."); break;
      }
    }
  }
}
