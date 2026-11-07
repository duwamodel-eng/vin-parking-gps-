
#include <WiFi.h>
#include <HTTPClient.h>
#include <TinyGPSPlus.h>

// ==========================================
// 1. CAU HINH CHAN GPS - ESP32-S3 SUPERMINI
// ==========================================

#define RX_PIN 44  // GPS TXD -> ESP32 GPIO44
#define TX_PIN 43  // GPS RXD -> ESP32 GPIO43

// ==========================================
// 2. CAU HINH WIFI
// ==========================================

const char* ssid = "VIN_PARKING_GPS";
const char* password = "12345678";

// Raspberry Pi - Flask API
const char* serverUrl =
  "http://10.42.0.1:5000/api/gps/update";

// ==========================================
// 3. KHOI TAO
// ==========================================

TinyGPSPlus gps;

unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 3000;

// ==========================================
// 4. KET NOI WIFI
// ==========================================

bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  Serial.println();
  Serial.println("Dang ket noi Wi-Fi VIN_PARKING_GPS...");

  WiFi.begin(ssid, password);

  unsigned long startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startAttempt < 20000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WIFI CONNECTED!");

    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    Serial.print("IP ESP32: ");
    Serial.println(WiFi.localIP());

    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());

    Serial.print("BSSID: ");
    Serial.println(WiFi.BSSIDstr());

    return true;
  }

  Serial.println("WIFI CONNECTION FAILED!");

  Serial.print("WiFi status code: ");
  Serial.println(WiFi.status());

  return false;
}

// ==========================================
// 5. SETUP
// ==========================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println(" VIN PARKING - GPS TRACKER");
  Serial.println(" ESP32-S3 SUPERMINI");
  Serial.println("================================");

  // Khoi dong UART GPS
  Serial1.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

  Serial.println("GPS UART da khoi dong!");

  // Ket noi Wi-Fi
  connectWiFi();

  Serial.println("He thong GPS san sang!");
}

// ==========================================
// 6. LOOP
// ==========================================

void loop() {

  // Doc du lieu GPS lien tuc
  while (Serial1.available() > 0) {
    gps.encode(Serial1.read());
  }

  // Gui du lieu moi 3 giay
  if (millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();

    // Neu Wi-Fi mat ket noi, thu ket noi lai
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Wi-Fi mat ket noi. Dang ket noi lai...");

      WiFi.disconnect();
      connectWiFi();

      return;
    }

    int sats = gps.satellites.value();

    // Kiem tra GPS hop le
    if (!gps.location.isValid() ||
        gps.location.age() >= 5000 ||
        sats < 4) {

      Serial.printf(
        "Dang cho GPS Fix: %d | Ve tinh: %d | Tuoi du lieu: %lu ms\n",
        gps.location.isValid(),
        sats,
        gps.location.age()
      );

      return;
    }

    // Lay toc do GPS
    float speed = gps.speed.kmph();

    if (speed < 3.0) {
      speed = 0.0;
    }

    // Tao JSON
    String payload =
      "{\"lat\":" + String(gps.location.lat(), 6) +
      ",\"lng\":" + String(gps.location.lng(), 6) +
      ",\"speed\":" + String(speed, 2) +
      ",\"satellites\":" + String(sats) + "}";

    Serial.println("--------------------------------");
    Serial.println("DANG GUI DU LIEU GPS");
    Serial.print("Payload: ");
    Serial.println(payload);

    // Gui HTTP POST den Raspberry Pi
    HTTPClient http;

    http.setConnectTimeout(5000);
    http.setTimeout(5000);

    if (http.begin(serverUrl)) {

      http.addHeader("Content-Type", "application/json");

      int httpResponseCode = http.POST(payload);

      Serial.print("HTTP Response: ");
      Serial.println(httpResponseCode);

      if (httpResponseCode > 0) {
        String response = http.getString();

        Serial.print("Phan hoi Raspberry Pi: ");
        Serial.println(response);
      } else {
        Serial.print("HTTP Error: ");
        Serial.println(http.errorToString(httpResponseCode));
      }

      http.end();

    } else {
      Serial.println("Khong the khoi tao HTTP connection!");
    }
  }
}
