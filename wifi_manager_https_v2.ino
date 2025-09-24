#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include "esp_system.h" // For detecting ESP32 reset reason

// ---------------- CONFIG ----------------
// Replace with your own server & API credentials
const char* server = "your-server.example.com";    // Your HTTPS server domain
const int httpsPort = 443;                        // Standard HTTPS port
const char* envPath  = "/env_data";
const char* pondPath = "/pond_data";
const char* apiKey   = "YOUR_API_KEY";            // <--- Masked (replace with your API key)

// Timeouts & retry/backoff configuration
const unsigned long WIFI_QUICK_CONNECT_MS   = 8000;  // Attempt to reuse stored Wi-Fi credentials
const unsigned long CONFIG_PORTAL_TIMEOUT_S = 120;   // Captive portal timeout (seconds)
const int           HTTP_MAX_ATTEMPTS       = 4;     // Max POST retries
const unsigned long HTTP_INITIAL_BACKOFF_MS = 1000;  // Initial exponential backoff (ms)
const unsigned long HTTP_READ_TIMEOUT_MS    = 10000; // HTTPS response timeout (ms)

WiFiManager wifiManager;
WiFiClientSecure secureClient;

void setup() {
  Serial.begin(115200);
  delay(10);
  randomSeed(analogRead(0));

  Serial.println("\n=== ESP32 HTTPS Client (Wi-Fi only) ===");

  // Clear Wi-Fi credentials if reset button (EN) was pressed
  if (esp_reset_reason() == ESP_RST_EXT) {
    Serial.println("External reset detected. Clearing stored Wi-Fi credentials...");
    wifiManager.resetSettings();
    WiFi.disconnect(true, true);
    delay(500);
  }

  // Try connecting with previously saved credentials
  Serial.println("Attempting quick Wi-Fi connection...");
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  unsigned long start = millis();
  bool connected = false;
  while (millis() - start < WIFI_QUICK_CONNECT_MS) {
    if (WiFi.status() == WL_CONNECTED) { connected = true; break; }
    delay(200);
  }

  if (connected) {
    Serial.printf("Connected to Wi-Fi: %s\n", WiFi.SSID().c_str());
  } else {
    Serial.println("Quick connect failed. Starting captive portal for manual Wi-Fi setup...");
    wifiManager.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT_S);
    if (wifiManager.startConfigPortal("ESP32_AP")) {
      Serial.printf("Connected via captive portal: %s\n", WiFi.SSID().c_str());
    } else {
      Serial.println("Captive portal timed out. Will retry later.");
    }
  }

  // TLS configuration (INSECURE for testing)
  // IMPORTANT: For production, replace with `secureClient.setCACert(...)`
  secureClient.setInsecure();
}

void loop() {
  if (!ensureWiFiConnectedWithRetries()) {
    Serial.println("Wi-Fi not connected. Retrying later...");
    delay(10000);
    return;
  }

  // Send two separate JSON payloads to the server
  sendEnvData();
  delay(2000);
  sendPondData();

  // Delay between cycles (adjust as needed)
  delay(60000);
}

// Attempt to reconnect Wi-Fi within a short timeout
bool ensureWiFiConnectedWithRetries() {
  if (WiFi.status() == WL_CONNECTED) return true;
  Serial.println("Wi-Fi disconnected. Attempting reconnect...");
  WiFi.reconnect();
  unsigned long start = millis();
  while (millis() - start < WIFI_QUICK_CONNECT_MS) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(200);
  }
  return false;
}

// Perform a secure HTTPS POST with retries, exponential backoff, and status parsing
bool postJsonWithRetries(const char* path, const String &payload) {
  for (int attempt = 1; attempt <= HTTP_MAX_ATTEMPTS; ++attempt) {
    unsigned long backoff = HTTP_INITIAL_BACKOFF_MS * (1UL << (attempt - 1)) + random(0, 500);
    Serial.printf("[POST] Attempt %d/%d -> %s (payload %u bytes)\n", attempt, HTTP_MAX_ATTEMPTS, path, (unsigned)payload.length());

    if (!secureClient.connect(server, httpsPort)) {
      Serial.println("  -> TLS connection failed");
      secureClient.stop();
    } else {
      secureClient.print(String("POST ") + path + " HTTP/1.1\r\n");
      secureClient.print(String("Host: ") + server + "\r\n");
      secureClient.print("User-Agent: ESP32\r\n");
      secureClient.print("Content-Type: application/json\r\n");
      secureClient.print("Content-Length: " + String(payload.length()) + "\r\n");
      secureClient.print("Connection: close\r\n\r\n");
      secureClient.print(payload);

      unsigned long tstart = millis();
      String statusLine;
      while (millis() - tstart < HTTP_READ_TIMEOUT_MS) {
        if (secureClient.available()) {
          statusLine = secureClient.readStringUntil('\n');
          break;
        }
        delay(5);
      }

      if (statusLine.length() == 0) {
        Serial.println("  -> No response (timeout).");
      } else {
        statusLine.trim();
        Serial.printf("  -> Status line: %s\n", statusLine.c_str());
        int code = 0;
        int firstSpace = statusLine.indexOf(' ');
        if (firstSpace > 0) {
          int secondSpace = statusLine.indexOf(' ', firstSpace + 1);
          String codeStr = (secondSpace > 0) ?
            statusLine.substring(firstSpace + 1, secondSpace) :
            statusLine.substring(firstSpace + 1);
          code = codeStr.toInt();
        }

        // Dump response headers/body
        unsigned long readStart = millis();
        while (millis() - readStart < 1000 && secureClient.available()) {
          Serial.println(secureClient.readStringUntil('\n'));
        }

        secureClient.stop();
        if (code >= 200 && code < 300) {
          Serial.printf("  -> Success (HTTP %d)\n", code);
          return true;
        } else {
          Serial.printf("  -> Server returned HTTP %d\n", code);
        }
      }
    }

    if (attempt < HTTP_MAX_ATTEMPTS) {
      Serial.printf("  -> Backing off %lums before retry\n", backoff);
      delay(backoff);
    } else {
      Serial.println("  -> Max attempts reached, giving up.");
    }
  }
  return false;
}

// Send environmental sensor data (replace hardcoded values with real sensor readings)
void sendEnvData() {
  float temperature = 22.3;
  float humidity    = 55.2;

  StaticJsonDocument<256> doc;
  doc["api_key"]    = apiKey;
  doc["sensor"]     = "SHT20";
  doc["Temperature"] = temperature;
  doc["Humidity"]    = humidity;
  doc["Pressure"]    = 0;

  String postData;
  serializeJson(doc, postData);
  Serial.println("Env JSON: " + postData);
  Serial.println(postJsonWithRetries(envPath, postData) ? "Env: sent OK" : "Env: send FAILED");
}

// Send pond water quality data (replace hardcoded values with sensor inputs)
void sendPondData() {
  float nitrogen = 0.0, ammonia = 0.0, phosphorus = 0.0, bod = 0.0, cod = 0.0, tds = 0.0;
  float do_val = 7.2, water_temp = 26.4, water_ph = 7.1;

  StaticJsonDocument<512> doc;
  doc["api_key"]   = apiKey;
  doc["nitrogen"]  = nitrogen;
  doc["ammonia"]   = ammonia;
  doc["phosphorus"]= phosphorus;
  doc["do"]        = do_val;
  doc["bod"]       = bod;
  doc["cod"]       = cod;
  doc["waterPH"]   = water_ph;
  doc["tds"]       = tds;
  doc["waterTemp"] = water_temp;

  String postData;
  serializeJson(doc, postData);
  Serial.println("Pond JSON: " + postData);
  Serial.println(postJsonWithRetries(pondPath, postData) ? "Pond: sent OK" : "Pond: send FAILED");
}
