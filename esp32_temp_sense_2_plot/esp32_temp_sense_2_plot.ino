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

// Dynamic sensor storage
struct SensorData {
  String celsius;
  String fahrenheit;
  String label;
};

SensorData sensorReadings[MAX_SENSORS];
int sensorCount = 0;
unsigned long lastTime = 0;
const unsigned long timerDelay = 10000;  // 10 seconds

const char* ssid = "";
const char* password = "REPLACE_WITH_YOUR_PASSWORD";

AsyncWebServer server(80);

String readSensorC(int index) {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(index);
  return (tempC == DEVICE_DISCONNECTED_C) ? "--" : String(tempC, 1);
}

String readSensorF(int index) {
  sensors.requestTemperatures();
  float tempF = sensors.getTempFByIndex(index);
  return (tempF == DEVICE_DISCONNECTED_F) ? "--" : String(tempF, 1);
}

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
  const celsiusReq = new XMLHttpRequest();
  celsiusReq.onreadystatechange = function() {
    if (this.readyState === 4 && this.status === 200) {
      document.getElementById(`temp${sensorId}c`).innerHTML = this.responseText;
    }
  };
  celsiusReq.open("GET", `/temperature${sensorId}c`, true);
  celsiusReq.send();

  const fahrenheitReq = new XMLHttpRequest();
  fahrenheitReq.onreadystatechange = function() {
    if (this.readyState === 4 && this.status === 200) {
      document.getElementById(`temp${sensorId}f`).innerHTML = this.responseText;
    }
  };
  fahrenheitReq.open("GET", `/temperature${sensorId}f`, true);
  fahrenheitReq.send();
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

String processor(const String& var) {
  // Not used in dynamic version
  return String();
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
    html.replace("<!-- Sensors dynamically inserted here -->", 
                generateSensorHTML());
    request->send(200, "text/html", html);
  });

  // Dynamic route creation
  for (int i = 0; i < sensorCount; i++) {
    server.on("/temperature" + String(i) + "c", HTTP_GET, [i](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", sensorReadings[i].celsius);
    });
    
    server.on("/temperature" + String(i) + "f", HTTP_GET, [i](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", sensorReadings[i].fahrenheit);
    });
  }

  server.begin();
}

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

void loop() {
  if (millis() - lastTime > timerDelay) {
    for (int i = 0; i < sensorCount; i++) {
      sensorReadings[i].celsius = readSensorC(i);
      sensorReadings[i].fahrenheit = readSensorF(i);
    }
    lastTime = millis();
  }
}
