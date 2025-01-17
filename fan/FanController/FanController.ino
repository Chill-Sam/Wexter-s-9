#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <PIDController.h>

// Wifi
#define WIFI_SSID "Hitachigymnasiet_2.4"
#define WIFI_PASSWORD "mittwifiarsabra"

// Firebase RTDB
#define DATABASE_URL "https://wexteras-9-default-rtdb.europe-west1.firebasedatabase.app/"
#define API_KEY "AIzaSyDK2H0kGevKzy-Y-5rfN8InhAOQAQVO6q8"
#define timerDelay 200

unsigned long sendDataPrevMillis = 0;

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

const byte MotorSpeed = 5;
const byte MotorDir = 0;

double percent = 0;
double oldPercent = 0;
int speed = 0;


bool manual = false;

float temperature = 20.0;
float desiredTemperature = 15.0;

// PID
PIDController pid;

// Get data function
template<typename T>
bool getData(const char* path, T& variable) {
  bool success = false;

  if constexpr (std::is_same<T, bool>::value) {
    success = Firebase.RTDB.getBool(&fbdo, path, &variable);
  } else if constexpr (std::is_same<T, int>::value) {
    success = Firebase.RTDB.getInt(&fbdo, path, &variable);
  } else if constexpr (std::is_same<T, float>::value) {
    success = Firebase.RTDB.getFloat(&fbdo, path, &variable);
  } else {
    Serial.print("Unsupported data type for path: ");
    Serial.println(path);
    return false;
  }

  if (success) {
    Serial.print("Successfully fetched: ");
    Serial.print(path);
    Serial.print(" - Value: ");
    Serial.println(variable);
  } else {
    Serial.print("Failed to fetch: ");
    Serial.print(path);
    Serial.print(" - Error: ");
    Serial.println(fbdo.errorReason().c_str());
  }

  return success;
}

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
  config.token_status_callback = tokenStatusCallback;

  Firebase.signUp(&config, &auth, "", "");
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  pinMode(MotorDir, OUTPUT);
  analogWriteFreq(20000);  // Set PWM frequency to 20 kHz, reduce white noise
  digitalWrite(MotorDir, HIGH);

  pid.begin();
  pid.setpoint(0);
  pid.tune(10.0, 1.0, 1.0);
  pid.limit(0, 100);
}

void loop() {
  if (!(millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    return;
  }

  if (Firebase.ready()) {

    // Get all values
    getData("SensorValues/Temperature/Value", temperature);
    getData("SensorValues/Temperature/Desired", desiredTemperature);
    if (!getData("Overrides/Fan/Manual", manual)) {
      manual = false;
    }

    Serial.println();
    Serial.println(" - - - - ");

    if (manual) {
      Serial.println("Manual mode");
      Firebase.RTDB.getDouble(&fbdo, "Overrides/Fan/Speed", &percent);

    } else {
      percent = pid.compute(desiredTemperature - temperature);
      Serial.print("PID: ");
    }

    Serial.println(percent);

    speed = percentToSpeed(percent);
    analogWrite(MotorSpeed, speed);

    // If percent has been updated from the previous value, push update to Firebase
    if (percent != oldPercent) {
      Firebase.RTDB.setDouble(&fbdo, "MotorValues/FanSpeed", percent);
      oldPercent = percent;
    }
  }
}
