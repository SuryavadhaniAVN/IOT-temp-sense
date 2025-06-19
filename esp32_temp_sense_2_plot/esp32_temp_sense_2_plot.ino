#ifdef ESP32
  #include <WiFi.h>
  #include <ESPAsyncWebServer.h>
#else
  #error "This code is for ESP32 only"
#endif
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 4
#define MAX_SENSORS 8  // Maximum supported sensors

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

struct SensorData {
  String celsius;
  String fahrenheit;
  String label;
};

SensorData sensorReadings[MAX_SENSORS];
int sensorCount = 0;
unsigned long lastTime = 0;
const unsigned long timerDelay = 10000;  // 10 seconds

const char* ssid = "Suryavadhani's S20 FE";
const char* password = "avadhani";

AsyncWebServer server(80);

// Function to read temperature in Celsius for a sensor
String readSensorC(int index) {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(index);
  return (tempC == DEVICE_DISCONNECTED_C) ? "--" : String(tempC, 1);
}

// Function to read temperature in Fahrenheit for a sensor
String readSensorF(int index) {
  sensors.requestTemperatures();
  float tempF = sensors.getTempFByIndex(index);
  return (tempF == DEVICE_DISCONNECTED_F) ? "--" : String(tempF, 1);
}

// Generate HTML for the dashboard (with sensor placeholders)
String generateHTML() {
  String html = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css">
  <style>
    html {font-family: Arial; display: inline-block; margin: 0px auto; text-align: center;}
    h2 {font-size: 3.0rem;}
    .sensor-container {margin: 2rem 0; font-size: 2.0rem;}
    .ds-labels {font-size: 1.5rem; vertical-align: middle;}
    .units {font-size: 1.2rem;}
  </style>
</head>
<body>
  <h2>ESP32 DS18B20 Dashboard</h2>
  <div id="sensors-container">
    <!-- Sensors dynamically inserted here -->
  </div>
</body>
<script>
function updateSensor(sensorId) {
  fetch(`/temperature?sensor=${sensorId}&unit=c`)
    .then(response => response.text())
    .then(data => {
      document.getElementById(`temp${sensorId}c`).innerHTML = data;
    });
  
  fetch(`/temperature?sensor=${sensorId}&unit=f`)
    .then(response => response.text())
    .then(data => {
      document.getElementById(`temp${sensorId}f`).innerHTML = data;
    });
}

function updateAllSensors() {
  const sensors = document.querySelectorAll('.sensor-container');
  sensors.forEach((_, index) => updateSensor(index));
}

setInterval(updateAllSensors, 10000);
</script>
</html>
)rawliteral";
  return html;
}

// Generate HTML for all sensors
String generateSensorHTML() {
  String html = "";
  for (int i = 0; i < sensorCount; i++) {
    html += "<div class='sensor-container'>";
    html += "<p><i class='fas fa-thermometer-half' style='color:#059e8a;'></i> ";
    html += "<span class='ds-labels'>" + sensorReadings[i].label + ": </span>";
    html += "<span id='temp" + String(i) + "c'>" + sensorReadings[i].celsius + "</span>";
    html += "<sup class='units'>&deg;C</sup> / ";
    html += "<span id='temp" + String(i) + "f'>" + sensorReadings[i].fahrenheit + "</span>";
    html += "<sup class='units'>&deg;F</sup>";
    html += "</p></div>";
  }
  return html;
}

void setup() {
  Serial.begin(115200);
  sensors.begin();
  
  // Detect connected sensors
  sensorCount = sensors.getDeviceCount();
  if (sensorCount > MAX_SENSORS) sensorCount = MAX_SENSORS;

  // Initialize sensor data
  for (int i = 0; i < sensorCount; i++) {
    sensorReadings[i] = {
      .celsius = readSensorC(i),
      .fahrenheit = readSensorF(i),
      .label = "Sensor " + String(i+1)
    };
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected: " + WiFi.localIP().toString());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = generateHTML();
    html.replace("<!-- Sensors dynamically inserted here -->", generateSensorHTML());
    request->send(200, "text/html", html);
  });

  // Unified temperature endpoint with parameters
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(request->hasParam("sensor") && request->hasParam("unit")) {
      int sensorIndex = request->getParam("sensor")->value().toInt();
      String unit = request->getParam("unit")->value();
      
      if(sensorIndex >= 0 && sensorIndex < sensorCount) {
        if(unit == "c") {
          request->send(200, "text/plain", sensorReadings[sensorIndex].celsius);
        } else if(unit == "f") {
          request->send(200, "text/plain", sensorReadings[sensorIndex].fahrenheit);
        } else {
          request->send(400, "text/plain", "Invalid unit");
        }
      } else {
        request->send(404, "text/plain", "Sensor not found");
      }
    } else {
      request->send(400, "text/plain", "Missing parameters");
    }
  });

  server.begin();
}

void loop() {
  if (millis() - lastTime > timerDelay) {
    for (int i = 0; i < sensorCount; i++) {
      sensorReadings[i].celsius = readSensorC(i);
      sensorReadings[i].fahrenheit = readSensorF(i);
    }
    lastTime = millis();
  }
}
