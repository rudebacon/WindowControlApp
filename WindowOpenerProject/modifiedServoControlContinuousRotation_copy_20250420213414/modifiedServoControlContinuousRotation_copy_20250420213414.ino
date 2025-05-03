
#include "DHT.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
// #include <Ping.h> // Install "Ping" library by Blake Foster (if not already)
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <ESPping.h>
#include <Preferences.h> //For remembering last saved targetTemp even in reset/crash

Preferences preferences;


#define DHTPIN 22     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11   // DHT 11
DHT dht(DHTPIN, DHTTYPE);

const char* ssid = "HanFamily";
const char* password = "han2625123";


IPAddress phone_ip (192, 168, 1, 127); // The remote ip to ping (DHCP static ip of my phone set in router settings)



// Location for Open-Meteo
const float LAT = 49.137181;
const float LON = -123.116953;

// Thresholds
const float LIGHT_DRIZZLE_THRESHOLD = 0.2;  // mm/hr
const float WIND_THRESHOLD = 10.0;          // km/h
float targetTemp = 26.0;

// Modes
enum Mode { AUTO, OPEN, CLOSE, STOP };
Mode currentMode = AUTO;

// Function to convert enum to string
String modeToString(Mode mode) {
  switch (mode) {
    case AUTO: return "auto";
    case OPEN: return "open";
    case CLOSE: return "close";
    case STOP: return "stop";
    default: return "";
  }
}


WebServer server(80);


// Motor A connections
int in1 = 17;
int in2 = 16;

//PD implementation
float Kp = 5.0, Kd = 0.2;

float integral = 0;
float lastError = 0;
unsigned long lastUpdate = 0;
const unsigned long interval = 20000;  // 20 seconds


bool motorRunning = false;
unsigned long motorStartTime = 0;
unsigned long motorRunDuration = 0;
int motorDirection = 0; // 1 = opening, -1 = closing, 0 = stopped

// ============ Helpers ============

void connectWiFi() {
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");
}

void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected. Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    connectWiFi();
  }
}

void handleRoot() {
  server.send(200, "text/plain", "ESP32 Window Controller");
}


// Function to ping the phone's IP and check if it's connected //STATIC IP MESSES WITH OTHER APPS SO DON'T USE
bool isPhoneConnected() {
  Serial.println("Pinging phone...");
  bool success = Ping.ping(phone_ip, 4);  // 4 attempts
  
  if (success) {
    return true;  // Phone is online
  } else {
    return false;  // Phone is not responding
  }
}



void startOpeningWindow(unsigned long duration) {
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  motorRunning = true;
  motorStartTime = millis();
  motorRunDuration = duration;
  motorDirection = 1;
}

void startClosingWindow(unsigned long duration) {
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  motorRunning = true;
  motorStartTime = millis();
  motorRunDuration = duration;
  motorDirection = -1;
}


void openWindow() {
  // OPEN WINDOW
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  motorRunning = true;
  motorDirection = 1;
  motorRunDuration = -1;
}
void closeWindow() {
  // CLOSE WINDOW
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  motorRunning = true;
  motorDirection = -1;
  motorRunDuration = -1;
}
void stopWindow() {
  // STOP MOTOR
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  motorRunning = false;
  motorDirection = 0;
}


void fetchWeatherAndAct(float t) {
  if (currentMode != AUTO) return;  // Check immediately at the start

  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(LAT, 6) +
               "&longitude=" + String(LON, 6) + "&current=precipitation,wind_speed_10m";
  http.begin(url);

  if (currentMode != AUTO) {
    http.end();  // Clean up
    return;
  }

  int httpCode = http.GET();

  if (currentMode != AUTO) {
    http.end();
    return;
  }

  if (httpCode == 200) {
    String payload = http.getString();

    if (currentMode != AUTO) {
      http.end();
      return;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (currentMode != AUTO) {
      http.end();
      return;
    }

    if (!error) {
      float precipitation = doc["current"]["precipitation"];
      float windSpeed = doc["current"]["wind_speed"];

      Serial.printf("Rain: %.2f mm/h, Wind: %.2f km/h\n", precipitation, windSpeed);

      if (currentMode != AUTO) {
        http.end();
        return;
      }

      if (precipitation < LIGHT_DRIZZLE_THRESHOLD && windSpeed < WIND_THRESHOLD) {
        Serial.println("Weather OK.");
        float error = targetTemp - t;
        Serial.print("Error: ");
        Serial.println(error);

        if (currentMode != AUTO) {
          http.end();
          return;
        }

        float derivative = (error - lastError) / (interval / 1000.0);

        float output = Kp * error + Kd * derivative;

        const float deadband = 0.5;

        if (currentMode != AUTO) {
          http.end();
          return;
        }
        Serial.println(output);
        Serial.println(deadband);
        if (output > deadband && currentMode == AUTO ) {
          // Too cold → close window slightly
          startClosingWindow(output * 1000);
          Serial.print("PID closing window: ");
          Serial.println(output);

        } else if (output < -deadband && currentMode == AUTO ) {
          // Too hot → open window slightly
          startOpeningWindow(-output * 1000);
          Serial.print("PID opening window: ");
          Serial.println(output);
        }

        lastError = error;

      } else {
        Serial.println("Bad weather. Closing window.");
        closeWindow();
      }
    } else {
      Serial.println("JSON parse error");
    }
  } else {
    Serial.printf("HTTP error: %d\n", httpCode);
  }

  http.end();
}

void saveMode(Mode mode) {
  preferences.begin("my-app", false);
  preferences.putInt("mode", mode);
  preferences.end();
}



void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  connectWiFi();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  preferences.begin("my-app", false); // open preferences once

  // Load saved values
  targetTemp = preferences.getFloat("targetTemp", 26.0);
  currentMode = static_cast<Mode>(preferences.getInt("mode", 0)); // <- fixed line

  preferences.end(); // done reading

  Serial.print("Loaded targetTemp: ");
  Serial.println(targetTemp);
  Serial.print("Loaded currentMode: ");
  Serial.println(currentMode);


  server.on("/", HTTP_GET, handleRoot);
  server.on("/auto", HTTP_GET, []() {
    currentMode = AUTO;
    saveMode(currentMode);
    server.send(200, "text/plain", "Mode: AUTO");
  });

  server.on("/open", HTTP_GET, []() {
    currentMode = OPEN;
    saveMode(currentMode);
    server.send(200, "text/plain", "Mode: OPEN");
  });

  server.on("/close", HTTP_GET, []() {
    currentMode = CLOSE;
    saveMode(currentMode);
    server.send(200, "text/plain", "Mode: CLOSE");
  });

  server.on("/stop", HTTP_GET, []() {
    currentMode = STOP;
    saveMode(currentMode);
    server.send(200, "text/plain", "Mode: STOP");
  });
  server.on("/set-temp", HTTP_GET, []() {
    if (server.hasArg("value")) {
      targetTemp = server.arg("value").toFloat();
      preferences.putFloat("targetTemp", targetTemp); // SAVE to flash!
      server.send(200, "text/plain", "Target temperature set to " + String(targetTemp) + "°C");
    } else {
      server.send(400, "text/plain", "Missing 'value' parameter");
    }
  });
  server.on("/get-temp", HTTP_GET, []() {
    server.send(200, "text/plain", String(targetTemp));
  });
  // Web server route
  server.on("/get-mode", HTTP_GET, []() {
    server.send(200, "text/plain", modeToString(currentMode));
  });
  server.on("/get-room-temp", HTTP_GET, []() {
      float temp = dht.readTemperature(); // make sure this function exists
      server.send(200, "text/plain", String(temp, 1)); // use 'server.send' directly
  });


  server.begin();
  Serial.println("Web server started");

  pinMode(in1, OUTPUT);
	pinMode(in2, OUTPUT);
	
	// Turn off motors - Initial state
	digitalWrite(in1, LOW);
	digitalWrite(in2, LOW);

  // Start temperature sensor
  dht.begin();
}

bool autoModeFirstTime = true;
int lastMode = -1;  // Or use an enum value not possible in currentMode

void loop() {
  reconnectWiFi();
  server.handleClient();

  if (currentMode != lastMode) { //Stopping motor only when AUTO is triggered from something else for the first time
    // Mode just changed
    if (currentMode == AUTO) {
      if (autoModeFirstTime) {
        Serial.println("AUTO mode activated for the first time!");
        // Do the "first-time only" thing here
        stopWindow();
        autoModeFirstTime = false;
      }
    } else {
      // Reset the flag when leaving AUTO mode
      autoModeFirstTime = true;
    }
    lastMode = currentMode;
  }


  if (currentMode == AUTO) {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > interval && currentMode == AUTO) {
      lastCheck = millis();
      if (!isPhoneConnected()) {
        Serial.println("Phone not detected on Wi-Fi. Closing window.");
        // CLOSE WINDOW
        closeWindow();
      } else {
        Serial.println("Phone is connected.");
        // Read temperature as Celsius (the default)
        float t = dht.readTemperature();
        if (isnan(t)) {
          Serial.println(F("Failed to read from DHT sensor!"));
        } else {
          Serial.print(F("Temperature: "));
          Serial.print(t);
          Serial.println(F("°C "));
          Serial.print(F("TARGET Temperature: "));
          Serial.print(targetTemp);
          Serial.println(F("°C "));
          fetchWeatherAndAct(t);
        }
      }
    }
  } else if (currentMode == OPEN) {
      Serial.println("Manual mode: OPEN");
      // OPEN WINDOW
      openWindow();
  } else if (currentMode == CLOSE) {
      Serial.println("Manual mode: CLOSE");
      // CLOSE WINDOW
      closeWindow();
  } else if (currentMode == STOP) {
      Serial.println("Manual mode: STOP");
      // STOP WINDOW
      stopWindow();
  }


  if (motorRunning && millis() - motorStartTime >= motorRunDuration) { // Handle motor timeout 
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    motorRunning = false;
    motorDirection = 0;
    Serial.println("Handled motor timeout.");
  }


}

