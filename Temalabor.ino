#include <M5StickC.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Ticker.h>
#include<WebSocketsServer.h>
Ticker timer;

// Floats for the IMU 
float accX = 0.0F;
float accY = 0.0F;
float accZ = 0.0F;

float gyroX = 0.0F;
float gyroY = 0.0F;
float gyroZ = 0.0F;

float pitch = 0.0F;
float roll  = 0.0F;
float yaw   = 0.0F;

// For the exponential moving average 
static float alpha = 0.97;
static float acc_x_out_exp = 0;
static float acc_y_out_exp = 0;
static float acc_z_out_exp = 0;

// For the step count

// Connecting to the internet
//const char* ssid = "UPC1471152"; 
//const char* password = "h8bAzcmsmzuj"; 
const char* ssid = "Revregi";
const char* password = "bacooon22";

// Creating the server
WebServer server(80);

// Adding a websocket to the server
WebSocketsServer webSocket = WebSocketsServer(81);

// Root handling
void handleRoot() {
  server.send(200, "text/plain", "Henlo from M5Stack Temalabor!");
}

// Exception handling
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

// Serving a webpage
char webpage[] PROGMEM = R"=====(
<html>
<!-- Adding a data chart using Chart.js -->
<head>
  <script src='https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.5.0/Chart.min.js'></script>
  <meta charset="UTF-8"/>
</head>
<body onload="init()">
<!-- Adding a slider for controlling data rate -->
<div>
  <input type= "range" min="1" value="5" id="dataRateSlider" oninput ="sendDataRate()" />
  <label for="dataRateSlider" id="dataRateLabel">Rate: 0.2Hz</label>
</div>
<hr />
<div>
  <canvas id="line-chart" width="800" height="450"></canvas>
</div>
<!-- Adding a websocket to the client (webpage) -->
<script>
  var webSocket;
var dataPlot;
var maxDataPoints = 100;

function RemoveData() {
  dataPlot.data.labels.shift();
  dataPlot.data.datasets[0].data.shift();
}

function addData(label, data) {
  if (dataPlot.data.labels.length > maxDataPoints) {
    removeData();
  }
  dataPlot.data.labels.push(label);
  dataPlot.data.datasets[0].data.push(data.accX);
  dataPlot.data.datasets[1].data.push(data.accY);
  dataPlot.data.datasets[2].data.push(data.accZ);
  dataPlot.update();
}

function init() {
  document.body.style.backgroundColor = "#f2f5ab";
  webSocket = new WebSocket('ws://' + window.location.hostname + ':81/');
  dataPlot = new Chart(document.getElementById("line-chart"), {
    type: 'line',
    data: {
      labels: [],
      datasets: [{
        data: [],
        label: "accX: ",
        borderColor: "#2ecdc6",
        fill: false
      },
      {
        data: [],
        label: "accY: ",
        borderColor: "#eb34b7",
        fill: false
      },
            {
        data: [],
        label: "accZ: ",
        borderColor: "#34eb46",
        fill: false
      }]
    }
  });
  webSocket.onmessage = (event) => {
    var data = JSON.parse(event.data);
    //console.log(data);
    var today = new Date();
    var t = today.getHours() + ":" + today.getMinutes() + ":" + today.getSeconds();
    addData(t, data);
  }
}

function sendDataRate() {
  var dataRate = document.getElementById("dataRateSlider").value;
  webSocket.send(dataRate);
  //dataRate = 1.0 / dataRate;
  document.getElementById("dataRateLabel").innerHTML = "Rate: " + dataRate + " várakozás/1 másodperc"; //.toFixed(2); // + "HZ";
}
</script>
</body>
</html>
)=====";


// SETUP
void setup() {
  m5.begin();
  M5.IMU.Init();
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
    
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp32")) {
  Serial.println("MDNS responder started");
  }

  //server.on("/", handleRoot);

  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });

  server.on("/",[](){
    server.send_P(200, "text/html", webpage);
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

  webSocket.begin();
  Serial.println("Websocket started");
  webSocket.onEvent(webSocketEvent);

  timer.attach(5, getData);

  // Step count
}

// LOOP
void loop() {

  // Read sensor data
  float temp = 0;
  M5.IMU.getGyroData(&gyroX,&gyroY,&gyroZ);
  M5.IMU.getAccelData(&accX,&accY,&accZ);
  M5.IMU.getAhrsData(&pitch,&roll,&yaw);
  M5.IMU.getTempData(&temp);

  //Serial.printf("%f %f %f\n\r", gyroX, gyroY, gyroZ);

  //M5.Lcd.setCursor(0, 20);
  //M5.Lcd.printf("%6.2f  %6.2f  %6.2f o/s\n", gyroX, gyroY, gyroZ);

  // Websocket things
  webSocket.loop();
  server.handleClient();
  delay(2);//allow the cpu to switch to other tasks

  // Use the expo filter
  float acc_x = accX;
  acc_x_out_exp = alpha * acc_x_out_exp + (1 - alpha) * acc_x;
  float acc_y = accY;
  acc_y_out_exp = alpha * acc_y_out_exp + (1 - alpha) * acc_y;
  float acc_z = accZ;
  acc_z_out_exp = alpha * acc_z_out_exp + (1 - alpha) * acc_z;

  Serial.printf("%lf %lf\n\r", acc_x, acc_x_out_exp);
  
  //M5.Lcd.fillScreen(GREEN);
  M5.Lcd.setCursor(0, 30);
  M5.Lcd.setTextSize(2);
  //M5.Lcd.println("Henlo:");
  M5.Lcd.println(accX);
  M5.Lcd.println(acc_x_out_exp);
}

void getData(){
  String json = "{\"accX\":";
  json += accX;
  json += ",\"accY\":";
  json += accY;
  json += ",\"accZ\":";
  json += accZ;
  json += "}";
  webSocket.broadcastTXT(json.c_str(), json.length());
  Serial.printf("%6.2f \n", accX);
  Serial.printf("HEEEEEENLO\n");
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length){
  // Do something with data
  if(type == WStype_TEXT){
    float dataRate = (float)atof((const char*) &payload[0]);
    timer.detach();
    timer.attach(dataRate, getData);
  }
}
