#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <TinyGPS++.h>

// ─── PASTE YOUR DETAILS HERE ───────────────────────
const char* ssid     = "Realme Narzo";
const char* password = "12345678";
String serverName    = "https://script.google.com/macros/s/AKfycbwgG0UBhxXIwSwrdvz1C714EGyU0XjIhwBOOS2_W7ql9dlkhbUi1VoCeApy0-JL4Fkn/exec";
// ───────────────────────────────────────────────────

// Sensor Pins
#define MQ3_PIN    34
#define MQ135_PIN  35

// Output Pins
#define LED_RED    32
#define LED_GRN    23
#define BUZZER_PIN 25

// Alcohol Threshold
#define MQ3_THRESHOLD   450
#define MQ135_THRESHOLD 250

// MPU6050
const int MPU = 0x68;
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;

TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

float latitude  = 0;
float longitude = 0;

// Previous value for swerving detection
float prevAy = 0;

// ─────────────────────────────────────────
void setup() {

  Serial.begin(115200);

  pinMode(LED_RED,    OUTPUT);
  pinMode(LED_GRN,    OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_GRN, HIGH);

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");

  // GPS
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);

  // MPU6050
  Wire.begin(21,22);

  Wire.beginTransmission(MPU);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  Serial.println("MPU6050 Initialized");
}

// ─────────────────────────────────────────
void loop() {

  // ───── Alcohol Sensors ─────
  int alcohol = analogRead(MQ3_PIN);
  int gas     = analogRead(MQ135_PIN);

  bool alcoholDetected = (alcohol > MQ3_THRESHOLD || gas > MQ135_THRESHOLD);

  // ───── Read MPU6050 ─────
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU,14,true);

  AcX = Wire.read()<<8 | Wire.read();
  AcY = Wire.read()<<8 | Wire.read();
  AcZ = Wire.read()<<8 | Wire.read();
  Tmp = Wire.read()<<8 | Wire.read();
  GyX = Wire.read()<<8 | Wire.read();
  GyY = Wire.read()<<8 | Wire.read();
  GyZ = Wire.read()<<8 | Wire.read();

  // Convert to g force
  float Ax = AcX / 16384.0;
  float Ay = AcY / 16384.0;
  float Az = AcZ / 16384.0;

  String rash = "Normal";

  // Sudden acceleration
  if (Ax > 0.30)
    rash = "Sudden Acceleration";

  // Harsh braking
  if (Ax < -0.40)
    rash = "Harsh Braking";

  // Hard cornering
  if (abs(Ay) > 0.40)
    rash = "Hard Cornering";

  // Collision detection
  if (abs(Ax) > 0.75 || abs(Ay) > 0.75 || abs(Az) > 0.75)
    rash = "Collision";

  // Swerving detection
  if (Ay > 0.30 && prevAy < -0.30)
    rash = "Swerving";

  prevAy = Ay;

  bool rashDetected = (rash != "Normal");

  // ───── GPS ─────
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());

    if (gps.location.isUpdated()) {
      latitude  = gps.location.lat();
      longitude = gps.location.lng();
    }
  }

  // ───── Alerts ─────
  if (alcoholDetected || rashDetected) {

    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GRN, LOW);
    digitalWrite(BUZZER_PIN, HIGH);

    Serial.println("ALERT: " + rash + (alcoholDetected ? " | ALCOHOL DETECTED" : ""));
  }
  else {

    digitalWrite(LED_GRN, HIGH);
    digitalWrite(LED_RED, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }

  // ───── Serial Monitor ─────
  Serial.println("──────────────────────");
  Serial.print("MQ3: "); Serial.print(alcohol);
  Serial.print(" | MQ135: "); Serial.println(gas);

  Serial.print("Ax: "); Serial.print(Ax);
  Serial.print(" Ay: "); Serial.print(Ay);
  Serial.print(" Az: "); Serial.println(Az);

  Serial.print("Rash: "); Serial.println(rash);

  Serial.print("GPS: ");
  Serial.print(latitude,6);
  Serial.print(",");
  Serial.println(longitude,6);

  // ───── Send to Google Sheet ─────
  sendData(alcohol, gas, Ax, Ay, Az, rash, latitude, longitude);

  // ───── Check Drowsiness ─────
  checkDrowsiness();

  delay(3000);
}

// ─────────────────────────────────────────
void sendData(int alcohol, int gas, float ax, float ay, float az,
              String rash, float lat, float lng) {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost");
    return;
  }

  HTTPClient http;

  String url = serverName;
  url += "?type=write";
  url += "&alcohol=" + String(alcohol);
  url += "&gas=" + String(gas);
  url += "&ax=" + String(ax);
  url += "&ay=" + String(ay);
  url += "&az=" + String(az);
  url += "&rash=" + rash;
  url += "&lat=" + String(lat,6);
  url += "&lng=" + String(lng,6);

  http.begin(url);

  int httpCode = http.GET();

  Serial.print("Send status: ");
  Serial.println(httpCode);

  http.end();
}

// ─────────────────────────────────────────
void checkDrowsiness() {

  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;

  http.begin(serverName + "?type=read");

  int httpCode = http.GET();

  if (httpCode > 0) {

    String payload = http.getString();

    Serial.print("Drowsiness: ");
    Serial.println(payload);

    if (payload == "DROWSY") {

      digitalWrite(LED_RED, HIGH);
      digitalWrite(LED_GRN, LOW);
      digitalWrite(BUZZER_PIN, HIGH);
    }
    else {

      digitalWrite(LED_RED, LOW);
      digitalWrite(LED_GRN, HIGH);
      digitalWrite(BUZZER_PIN, LOW);
    }
  }

  http.end();
}