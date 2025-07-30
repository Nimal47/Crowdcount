#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include "time.h"
#include "credentials.h"

// --- Network Configuration ---
IPAddress dns(8, 8, 8, 8); // Google's public DNS

// --- NTP Time Configuration ---
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800; // IST UTC+5:30
const int daylightOffset_sec = 0;

// --- Firebase Objects ---
FirebaseData fbdo;
FirebaseConfig config;
FirebaseAuth auth;

// --- Sensor Pins ---
const int trig_pin_1 = D1;
const int echo_pin_1 = D2;
const int trig_pin_2 = D5;
const int echo_pin_2 = D6;

// --- Detection Parameters ---
const int DETECTION_THRESHOLD = 20;
const unsigned long MIN_SEQUENCE_TIME = 30;
const unsigned long MAX_SEQUENCE_TIME = 1300;

// --- People Counting Variables ---
int crowd = 0;
int peopleIn = 0;
int peopleOut = 0;

// --- Hourly Tracking ---
int lastPeopleIn = 0;
int lastPeopleOut = 0;
int peopleInLastHour = 0;
int peopleOutLastHour = 0;
int lastHourChecked = -1;
bool timeInitialized = false;

// --- Sensor States ---
bool sensor1_active = false;
bool sensor2_active = false;
unsigned long sensor1_time = 0;
unsigned long sensor2_time = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Crowd Counter...");

  pinMode(echo_pin_1, INPUT);
  pinMode(trig_pin_1, OUTPUT);
  pinMode(echo_pin_2, INPUT);
  pinMode(trig_pin_2, OUTPUT);

  // Wi-Fi Connect
  WiFi.config(IPAddress(0,0,0,0), IPAddress(0,0,0,0), IPAddress(0,0,0,0), dns);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  delay(1000);

  // --- Firebase Setup (Fixed to match working version) ---
  Serial.println("Initializing Firebase...");
  
  // Clean the host URL
  String host = FIREBASE_HOST;
  if (host.startsWith("https://")) {
    host = host.substring(8);
  }
  if (host.endsWith("/")) {
    host = host.substring(0, host.length() - 1);
  }
  
  // Use the working configuration from your old code
  config.host = host.c_str();
  config.database_url = host.c_str();
  config.signer.tokens.legacy_token = FIREBASE_AUTH; // Use only legacy token
  
  // Empty auth credentials (legacy token handles authentication)
  auth.user.email = "";
  auth.user.password = "";

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(1024);
  
  Serial.println("Firebase initialized.");
  
  // Initialize time (non-blocking approach)
  Serial.println("Initializing time...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Don't block on time sync - we'll check later
  delay(2000);
  
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    Serial.println("Time synced successfully.");
    timeInitialized = true;
    lastHourChecked = timeinfo.tm_hour;
  } else {
    Serial.println("Time sync failed, will retry later.");
    timeInitialized = false;
  }
}

float readDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  float timing = pulseIn(echoPin, HIGH, 25000);
  if (timing == 0) return 999;
  return (timing * 0.034) / 2;
}

void updateFirebaseLive() {
  String basePath = "/peoplecrowd";
  Serial.println("Updating Firebase...");

  // Remove Firebase.ready() check that was causing issues
  if (Firebase.setInt(fbdo, basePath + "/crowd", crowd)) {
    Serial.println("Crowd updated successfully.");
  } else {
    Serial.print("Firebase Error (crowd): ");
    Serial.println(fbdo.errorReason());
  }

  if (Firebase.setInt(fbdo, basePath + "/peopleIn", peopleIn)) {
    Serial.println("PeopleIn updated successfully.");
  } else {
    Serial.print("Firebase Error (peopleIn): ");
    Serial.println(fbdo.errorReason());
  }

  if (Firebase.setInt(fbdo, basePath + "/peopleOut", peopleOut)) {
    Serial.println("PeopleOut updated successfully.");
  } else {
    Serial.print("Firebase Error (peopleOut): ");
    Serial.println(fbdo.errorReason());
  }

  if (Firebase.setInt(fbdo, basePath + "/peopleInLastHour", peopleInLastHour)) {
    Serial.println("PeopleInLastHour updated successfully.");
  } else {
    Serial.print("Firebase Error (peopleInLastHour): ");
    Serial.println(fbdo.errorReason());
  }

  if (Firebase.setInt(fbdo, basePath + "/peopleOutLastHour", peopleOutLastHour)) {
    Serial.println("PeopleOutLastHour updated successfully.");
  } else {
    Serial.print("Firebase Error (peopleOutLastHour): ");
    Serial.println(fbdo.errorReason());
  }

  // Use millis() for timestamp instead of Firebase.setTimestamp
  if (Firebase.setInt(fbdo, basePath + "/lastUpdate", millis())) {
    Serial.println("Timestamp updated successfully.");
  } else {
    Serial.print("Firebase Error (timestamp): ");
    Serial.println(fbdo.errorReason());
  }
}

void checkHourlyWindow() {
  // Check if time is initialized first
  if (!timeInitialized) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      timeInitialized = true;
      lastHourChecked = timeinfo.tm_hour;
      Serial.println("Time sync completed in main loop.");
      return;
    } else {
      // Skip hourly check if time isn't available yet
      return;
    }
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Time fetch failed.");
    return;
  }

  if (timeinfo.tm_hour != lastHourChecked) {
    Serial.printf("New hour: %d → %d\n", lastHourChecked, timeinfo.tm_hour);
    
    peopleInLastHour = peopleIn - lastPeopleIn;
    peopleOutLastHour = peopleOut - lastPeopleOut;

    time_t now;
    time(&now);
    now -= 3600; // Get previous hour
    struct tm* prevHour = localtime(&now);

    char datePath[11]; // YYYY-MM-DD
    strftime(datePath, sizeof(datePath), "%Y-%m-%d", prevHour);

    char hourPath[3]; // HH
    strftime(hourPath, sizeof(hourPath), "%H", prevHour);

    String firebasePath = "/hourlyData/" + String(datePath) + "/" + String(hourPath);
    Serial.println("Saving hourly data to: " + firebasePath);

    FirebaseJson json;
    json.set("in", peopleInLastHour);
    json.set("out", peopleOutLastHour);
    json.set("net", crowd);

    if (Firebase.set(fbdo, firebasePath, json)) {
      Serial.println("Hourly data saved successfully.");
    } else {
      Serial.print("Hourly data save failed: ");
      Serial.println(fbdo.errorReason());
    }

    lastPeopleIn = peopleIn;
    lastPeopleOut = peopleOut;
    lastHourChecked = timeinfo.tm_hour;

    updateFirebaseLive();
  }
}

void loop() {
  checkHourlyWindow();

  float distance_1 = readDistance(trig_pin_1, echo_pin_1);
  float distance_2 = readDistance(trig_pin_2, echo_pin_2);

  // Add debug output like your working version
  Serial.print("S1: ");
  Serial.print(distance_1);
  Serial.print("cm | S2: ");
  Serial.print(distance_2);
  Serial.print("cm | Crowd: ");
  Serial.print(crowd);
  Serial.print(" | In: ");
  Serial.print(peopleIn);
  Serial.print(" | Out: ");
  Serial.println(peopleOut);

  if (distance_1 <= DETECTION_THRESHOLD && !sensor1_active) {
    sensor1_active = true;
    sensor1_time = millis();
    Serial.println("Sensor 1 triggered");
  }

  if (distance_2 <= DETECTION_THRESHOLD && !sensor2_active) {
    sensor2_active = true;
    sensor2_time = millis();
    Serial.println("Sensor 2 triggered");
  }

  if (sensor1_active && sensor2_active) {
    unsigned long time_diff = abs((long)sensor2_time - (long)sensor1_time);

    if (time_diff >= MIN_SEQUENCE_TIME && time_diff <= MAX_SEQUENCE_TIME) {
      if (sensor1_time < sensor2_time) {
        crowd++;
        peopleIn++;
        Serial.println("Valid sequence 1->2 (Person entered)");
      } else {
        crowd--;
        peopleOut++;
        Serial.println("Valid sequence 2->1 (Person exited)");
      }

      updateFirebaseLive();
      sensor1_active = false;
      sensor2_active = false;
    } else if (time_diff > MAX_SEQUENCE_TIME) {
      if (sensor1_time < sensor2_time) sensor1_active = false;
      else sensor2_active = false;
      Serial.println("Sequence timeout - resetting earlier sensor");
    }
  }

  // Reset sensors when no longer detecting
  if (sensor1_active && distance_1 > DETECTION_THRESHOLD) {
    sensor1_active = false;
    Serial.println("Sensor 1 reset - no detection");
  }
  
  if (sensor2_active && distance_2 > DETECTION_THRESHOLD) {
    sensor2_active = false;
    Serial.println("Sensor 2 reset - no detection");
  }

  // Timeout reset
  if (sensor1_active && (millis() - sensor1_time) > MAX_SEQUENCE_TIME) {
    sensor1_active = false;
    Serial.println("Sensor 1 timeout");
  }
  
  if (sensor2_active && (millis() - sensor2_time) > MAX_SEQUENCE_TIME) {
    sensor2_active = false;
    Serial.println("Sensor 2 timeout");
  }

  delay(100); // Use same delay as working version
}