#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <PIDController.h>

// Wifi
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Firebase RTDB
#define DATABASE_URL "https://wexteras-9-default-rtdb.europe-west1.firebasedatabase.app/"
#define API_KEY ""

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

const byte MotorSpeed = 5;
const byte MotorDir = 0;

int oldPercent = 0;
int speed = 0;

bool manual = false;

float temperature = 20.0;
float desiredTemperature = 15.0;

// PID
PIDController pid;

// Convert percentage to analog output
int percentToSpeed(int percent) {
  return percent == 0 ? 0 : map(percent, 0, 100, 185, 255);
}


void setup() {
  Serial.begin(9600);
  Serial.println("Starting");

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.localIP());

  // Start Firebase connection
  config.database_url = DATABASE_URL;
  config.api_key = API_KEY;

  Firebase.signUp(&config, &auth, "", "");
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  pinMode(MotorDir, OUTPUT);
  analogWriteFreq(20000);  // Set PWM frequency to 20 kHz, reduce white noise
  digitalWrite(MotorDir, HIGH);

  pid.begin();
  pid.setpoint(20);
  pid.tune(10, 10, 10);
  pid.limit(0, 100);
}

void loop() {
  if (!Firebase.ready()) {
    return;
  }

  Firebase.RTDB.getFloat(&fbdo, "SensorValues/Temperature/Value", &temperature);
  Firebase.RTDB.getFloat(&fbdo, "SensorValues/Temperature/Desired", &desiredTemperature);
  Firebase.RTDB.getBool(&fbdo, "Overrides/Fan/Manual", &manual);

  int percent = 0;
  if (manual) {
    Serial.println("Manual mode");
    Firebase.RTDB.getInt(&fbdo, "Overrides/Fan/Speed", &percent);

  } else if (temperature < desiredTemperature) {
    pid.setpoint(desiredTemperature);
    percent = pid.compute(temperature);
    Serial.println(percent);

  } else {
    Serial.println("ZERO");
    analogWrite(MotorSpeed, 0);
  }

  speed = percentToSpeed(percent);
  analogWrite(MotorSpeed, speed);

  // If percent has been updated from the previous value, push update to Firebase
  if (percent != oldPercent) {
    Firebase.RTDB.setInt(&fbdo, "MotorValues/FanSpeed", percent);
    oldPercent = percent;
  }
}
