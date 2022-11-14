//https://randomnerdtutorials.com/esp32-esp8266-input-data-html-form/

/*

  This program starts by displaying an Input Form to the user.
  When the user fulfills the information, it is then saved in the permanent storage of the ESP32.
  Afterwards, the "permanent" file of the ESP32 is acessed to retrieve the information.
  Then, two json files are created with all available BLE devices and Wi-Fi networks as they are scanned.
  Finally, the json files containing all devices and networks found are sent to the ILS Server.

*/

#include <Arduino.h>
#include <AsyncTCP.h>
#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEUtils.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <WiFi.h>

// https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/
//An instance of the Preferences library
Preferences preferences;

// https://www.youtube.com/watch?v=--KxxMaiwSE
#define DEBUG 1

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#define debugf(x , y) Serial.printf(x , y)
#else
#define debug(x)
#define debugln(x)
#define debugf(x , y)
#endif

AsyncWebServer server(80);

// REPLACE WITH YOUR NETWORK CREDENTIALS
const char* ssidC = "ZON-C510";
const char* passwordC = "Boas1234";

const char* PARAM_SSID = "inputSsid";
const char* PARAM_PASSW = "inputPassw";
const char* PARAM_SERVER = "inputServer";
const char* PARAM_TAG = "inputTag";
const char* PARAM_WIFI = "inputWifi";
const char* PARAM_BLE = "inputBle";
const char* PARAM_URL = "inputUrl";
const char* PARAM_STATE = "inputState";

String ssid;
String password;
String serverName;
String tagName;
String wifiInterval;
String bleInterval;
String jsonURL;
String state = "1";

// https://techtutorialsx.com/2017/08/20/esp32-arduino-freertos-queues/
QueueHandle_t queueWIFI;
QueueHandle_t queueBLE;

TaskHandle_t Task1, Task2, Task3, Task4, Task5;

//Counter for BLE devices scanned
int number = 1;

unsigned long totalScanWifi;

//Can be changed, in seconds
int scanTimeBle = 3;

BLEScan* pBLEScan;

String translateEncryptionType(wifi_auth_mode_t encryptionType) {

  switch (encryptionType) {
    case (WIFI_AUTH_OPEN):
      return "Open";
    case (WIFI_AUTH_WEP):
      return "WEP";
    case (WIFI_AUTH_WPA_PSK):
      return "WPA_PSK";
    case (WIFI_AUTH_WPA2_PSK):
      return "WPA2_PSK";
    case (WIFI_AUTH_WPA_WPA2_PSK):
      return "WPA_WPA2_PSK";
    case (WIFI_AUTH_WPA2_ENTERPRISE):
      return "WPA2_ENTERPRISE";
  }
}

// Here the "static" part of the json file is initialized. It has several preset values that will be replaced as we scan the bluetooth networks.
String dataGramaBLE = "scanData= {  \"tagName\": \"tagPedro\",  \"tagBSSID\": \"00:11:22:33:44:99\",  \"tagNetwork\": \"eduroam\",  \"dataType\": \"BLE\",  \"scanMode\": \"auto\", \"BLEData\":";

// Here the "volatile" part of the json file is initialized. This part will be changing constantly.
String adicionarBLE;

String adicionarBLECopy;

// Here the "static" part of the json file is initialized. It has several preset values that will be replaced as we scan the wi-fi networks.
String dataGramaWIFI = "scanData= { \"tagName\": \"tagPedro\", \"tagBSSID\": \"00:11:22:33:44:55\", \"tagNetwork\": \"eduroam\", \"dataType\": \"Wi-Fi\", \"scanMode\": \"auto\", \"WiFiData\":";

String adicionarWIFICopy;

void replaceAll() {
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  serverName = preferences.getString("serverName", "");
  tagName = preferences.getString("tagName", "");
  wifiInterval = preferences.getString("WIFIinterval", "");
  bleInterval = preferences.getString("BLEinterval", "");
  jsonURL = preferences.getString("jsonURL", "");

  // In the datagram to be sent it is required to identify which network the device is connected to and it's ssid
  dataGramaWIFI.replace("eduroam", (WiFi.SSID()));

  // In the datagram to be sent it is required to identify which macAdress the device is connected to
  dataGramaWIFI.replace("00:11:22:33:44:55", (WiFi.macAddress()));

  // In the datagram to be sent it is required to identify the tag's Name
  dataGramaWIFI.replace("tagPedro", tagName);

  // In the datagram to be sent it is required to identify which network the device is connected to and it's ssid
  dataGramaBLE.replace("eduroam", (WiFi.SSID()));

  // In the datagram to be sent it is required to identify which macAdress the device is connected to
  dataGramaBLE.replace("00:11:22:33:44:99", (WiFi.macAddress()));

  // In the datagram to be sent it is required to identify the tag's Name
  dataGramaBLE.replace("tagPedro", tagName);
}

void connectToNetwork() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssidC, passwordC);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    debugln("WiFi Failed!");
    return;
  }
}

// HTML web page to handle 7 input fields
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP32 Input Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <script>
    function submitMessage() {
      alert("Saved values to ESP32 SPIFFS");
      setTimeout(function(){ document.location.reload(false); }, 500);   
    }
    function myFunction() {
      document.getElementById("demo").innerHTML = "Click on ILS Server to visualize the data";
    }
  </script></head><body>
  <h2>ESP32 Input Form</h2>
  <p>Clicking on the "Update" button will send the form-data to the ESP32 SPIFFS System.</p>
  <form action="/get" target="hidden-form">
    inputSsid: <input type="text" name="inputSsid" value=%inputSsid%>
    <br><br>
    inputPassw: <input type="text" name="inputPassw" value=%inputPassw%>
    <br><br>
    inputServer: <input type="text" name="inputServer" value=%inputServer%>
    <br><br>
    inputTag: <input type="text" name="inputTag" value=%inputTag%>
    <br><br>
    inputWifi: <input type="number " name="inputWifi" value=%inputWifi%>
    <br><br>
    inputBle: <input type="number " name="inputBle" value=%inputBle%>
    <br><br>
    inputUrl: <input type="text" name="inputUrl" value=%inputUrl%>
    <br><br>
    <input type="submit" value="Update" onclick="submitMessage()">
  </form><br>
  <p>Clicking on the "Start" button will start the scans and posts of data to the ILS Server.</p>
  <form action="/get" target="hidden-form">
    <input type="submit" value="Start" name="inputState" onclick="myFunction()">
  </form>
  <p id="demo"></p>
  <br>
  <form action="http://ils.dsi.uminho.pt/viewData/">
    <input type="submit" value="ILS Server">
  </form>
  <iframe style="display:none" name="hidden-form"></iframe>
</body></html>)rawliteral";

//When requesting an invalid URL
void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

//A file's content is read using readFile().
String readFile(fs::FS &fs, const char * path) {
  //debugf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {
    //debugln("- empty file or failed to open file");
    return String();
  }
  //debugln("- read from file:");
  String fileContent;
  while (file.available()) {
    fileContent += String((char)file.read());
  }
  file.close();
  //debugln(fileContent);
  return fileContent;
}

//The writeFile() writes content to a file:
void writeFile(fs::FS &fs, const char * path, const char * message) {
  //debugf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if (!file) {
    //debugln("- failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    //debugln("- file written");
  } else {
    //debugln("- write failed");
  }
  file.close();
}

// Replaces placeholder with stored values
String processor(const String& var) {
  //debugln(var);
  if (var == "inputSsid") {
    return readFile(SPIFFS, "/inputSsid.txt");
  }
  if (var == "inputPassw") {
    return readFile(SPIFFS, "/inputPassw.txt");
  }
  if (var == "inputServer") {
    return readFile(SPIFFS, "/inputServer.txt");
  }
  if (var == "inputTag") {
    return readFile(SPIFFS, "/inputTag.txt");
  }
  if (var == "inputWifi") {
    return readFile(SPIFFS, "/inputWifi.txt");
  }
  if (var == "inputBle") {
    return readFile(SPIFFS, "/inputBle.txt");
  }
  if (var == "inputUrl") {
    return readFile(SPIFFS, "/inputUrl.txt");
  }
  if (var == "inputState") {
    return readFile(SPIFFS, "/inputState.txt");
  }
  return String();
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {

    void onResult(BLEAdvertisedDevice advertisedDevice) {

      // The bluetooth data containing information about all the networks found has to be sent inside brackets
      adicionarBLE += "{\"bssid\": \"bssid(i)\",      \"rssi\": \"rssi(i)\",      \"ssid\": \"ssid(i)\"}";

      // The preset value of bssid is replaced by the bssid of the device found
      adicionarBLE.replace("bssid(i)", (advertisedDevice.getAddress().toString().c_str()));
      String rssiStr = String (advertisedDevice.getRSSI());
      // The preset value of rssi is replaced by the rssi of the device found
      adicionarBLE.replace("rssi(i)", rssiStr);
      String nome = advertisedDevice.getName().c_str();
      // The preset value of ssid is replaced by the ssid of the device found
      adicionarBLE.replace("ssid(i)", nome);
      number ++;
      adicionarBLE += ", ";
    }
};

void scanBLE() {

  // The BLE data containing information about all the networks found has to be sent inside brackets
  adicionarBLE += "[";
  BLEScanResults foundDevices = pBLEScan->start(scanTimeBle, false);
  // The BLE data containing information about all the networks found has to be sent inside brackets
  adicionarBLE += "]}";
  number = 1;
  pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory

  //It is necessary to remove the last comma
  int lastComma = adicionarBLE.lastIndexOf(',');
  adicionarBLE.remove(lastComma , 1);
  adicionarBLECopy = adicionarBLE;
  adicionarBLE.remove(0);
  xQueueOverwrite(queueBLE, &adicionarBLECopy);
}

void scanWIFI() {

  unsigned long scanWifiA = millis();
  // Here the "volatile" part of the json file is initialized. This part will be changing constantly
  String adicionarWIFI;
  int linhaUm = 1;
  int numberOfNetworks = WiFi.scanNetworks();

  // The wi-fi data containing information about all the networks found has to be sent inside brackets
  adicionarWIFI += "[";

  for (int i = 0; i < numberOfNetworks; i++) {
    if (linhaUm == 0) {
      adicionarWIFI += ", ";
    } else linhaUm = 0;

    adicionarWIFI += "{\"bssid\": \"bssid(i)\",      \"rssi\": \"rssi(i)\",      \"ssid\": \"ssid(i)\",      \"encript\": \"encript(i)\"}";

    // The preset value of bssid is replaced by the bssid of the network number i found
    adicionarWIFI.replace("bssid(i)", (WiFi.BSSIDstr(i)));

    String rssiStr = String (WiFi.RSSI(i));
    // The preset value of rssi is replaced by the rssi of the network number i found
    adicionarWIFI.replace("rssi(i)", rssiStr);

    // The preset value of ssid is replaced by the ssid of the network number i found
    adicionarWIFI.replace("ssid(i)", (WiFi.SSID(i)));

    String encryptionTypeDescription = translateEncryptionType(WiFi.encryptionType(i));

    // The preset value of encryption is replaced by the encryption of the network number i found
    adicionarWIFI.replace("encript(i)", encryptionTypeDescription);

  }

  // The wi-fi data containing information about all the networks found has to be sent inside brackets
  adicionarWIFI += "]}";

  adicionarWIFICopy = adicionarWIFI;

  adicionarWIFI.remove(0);

  xQueueOverwrite(queueWIFI, &adicionarWIFICopy);

  unsigned long scanWifiB = millis();
  totalScanWifi = scanWifiB - scanWifiA;
}

//https://randomnerdtutorials.com/esp32-http-get-post-arduino/#http-post

void postBLE() {

  xQueueReceive(queueBLE, &adicionarBLECopy, portMAX_DELAY);

  if ( adicionarBLECopy.length() > 0) {

    // Send HTTP POST request
    // Here we concatenate both parts to originate the final datagram to be sent
    String dataGramaBLEFinal = dataGramaBLE + adicionarBLECopy;

    int tamanhoBLE = dataGramaBLEFinal.length();

    int connectionCountBLE = 0;

    //Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient clientBle;
      HTTPClient httpBle;

      // Your Domain name with URL path or IP address with path
      if (connectionCountBLE == 0) {
        httpBle.begin(clientBle, serverName);
      } connectionCountBLE++;

      // Specify content-type header
      httpBle.addHeader("Cache-Control", "no-cache");
      httpBle.addHeader("Content-Type", "application/x-www-form-urlencoded", "Content-Length", tamanhoBLE);
      //http.addHeader("Content-Length", tamanhoBLE);
      httpBle.addHeader("Accept", "*/*");
      httpBle.addHeader("Accept-Encoding", "gzip, deflate, br");
      httpBle.addHeader("Connection", "keep-alive");

      int httpResponseCodeBLE = httpBle.POST(dataGramaBLEFinal);

      String responseBLE = httpBle.getString();

      debug("[BLE] HTTP Response code: ");
      debugln(httpResponseCodeBLE);
      debugln(responseBLE);
      debugln("------------------------------------------------------------");


      //httpBle.end();
    }
  }
}

//https://randomnerdtutorials.com/esp32-http-get-post-arduino/#http-post

void postWIFI() {

  xQueueReceive(queueWIFI, &adicionarWIFICopy, portMAX_DELAY);

  if ( adicionarWIFICopy.length() > 0) {

    // Send HTTP POST request
    // Here we concatenate both parts to originate the final datagram to be sent
    String dataGramaWIFIFinal = dataGramaWIFI + adicionarWIFICopy;

    int tamanhoWIFI = dataGramaWIFIFinal.length();

    int connectionCountWIFI = 0;

    //Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient clientWifi;
      HTTPClient httpWifi;

      // Your Domain name with URL path or IP address with path
      if (connectionCountWIFI == 0) {
        httpWifi.begin(clientWifi, serverName);
      } connectionCountWIFI++;

      // Specify content-type header
      httpWifi.addHeader("Cache-Control", "no-cache");
      httpWifi.addHeader("Content-Type", "application/x-www-form-urlencoded", "Content-Length", tamanhoWIFI);
      //http.addHeader("Content-Length", tamanhoWIFI);
      httpWifi.addHeader("Accept", "*/*");
      httpWifi.addHeader("Accept-Encoding", "gzip, deflate, br");
      httpWifi.addHeader("Connection", "keep-alive");

      int httpResponseCodeWIFI = httpWifi.POST(dataGramaWIFIFinal);

      String responseWifi = httpWifi.getString();


      debug("[Wi-Fi] HTTP Response code: ");
      debugln(httpResponseCodeWIFI);
      debugln(responseWifi);
      debugln("------------------------------------------------------------");

      //httpWifi.end();
    }
  }
}

void serverForm() {

  // Send web page with input fields to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  // Send a GET request to <ESP_IP>/get?input=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest * request) {
    String inputMessage;
    // GET inputSsid value on <ESP_IP>/get?inputSsid=<inputMessage>
    if (request->hasParam(PARAM_SSID)) {
      inputMessage = request->getParam(PARAM_SSID)->value();
      writeFile(SPIFFS, "/inputSsid.txt", inputMessage.c_str());
      preferences.putString("ssid", inputMessage.c_str());
    }
    // GET inputPassw value on <ESP_IP>/get?inputPassw=<inputMessage>
    if (request->hasParam(PARAM_PASSW)) {
      inputMessage = request->getParam(PARAM_PASSW)->value();
      writeFile(SPIFFS, "/inputPassw.txt", inputMessage.c_str());
      preferences.putString("password", inputMessage.c_str());
    }
    // GET inputServer value on <ESP_IP>/get?inputServer=<inputMessage>
    if (request->hasParam(PARAM_SERVER)) {
      inputMessage = request->getParam(PARAM_SERVER)->value();
      writeFile(SPIFFS, "/inputServer.txt", inputMessage.c_str());
      preferences.putString("serverName", inputMessage.c_str());
    }
    // GET inputTag value on <ESP_IP>/get?inputTag=<inputMessage>
    if (request->hasParam(PARAM_TAG)) {
      inputMessage = request->getParam(PARAM_TAG)->value();
      writeFile(SPIFFS, "/inputTag.txt", inputMessage.c_str());
      preferences.putString("tagName", inputMessage.c_str());
    }
    // GET inputWifi value on <ESP_IP>/get?inputWifi=<inputMessage>
    if (request->hasParam(PARAM_WIFI)) {
      inputMessage = request->getParam(PARAM_WIFI)->value();
      writeFile(SPIFFS, "/inputWifi.txt", inputMessage.c_str());
      preferences.putString("WIFIinterval", inputMessage.c_str());
    }
    // GET inputBle value on <ESP_IP>/get?inputBle=<inputMessage>
    if (request->hasParam(PARAM_BLE)) {
      inputMessage = request->getParam(PARAM_BLE)->value();
      writeFile(SPIFFS, "/inputBle.txt", inputMessage.c_str());
      preferences.putString("BLEinterval", inputMessage.c_str());
    }
    // GET inputUrl value on <ESP_IP>/get?inputUrl=<inputMessage>
    if (request->hasParam(PARAM_URL)) {
      inputMessage = request->getParam(PARAM_URL)->value();
      writeFile(SPIFFS, "/inputUrl.txt", inputMessage.c_str());
      preferences.putString("jsonURL", inputMessage.c_str());
    }
    // GET inputState value on <ESP_IP>/get?inputState=<inputMessage>
    if (request->hasParam(PARAM_STATE)) {
      inputMessage = request->getParam(PARAM_STATE)->value();
      writeFile(SPIFFS, "/inputState.txt", "0");
      preferences.putString("state", "0");
    }
    else {
      inputMessage = "No message sent";
    }
    request->send(200, "text/text", inputMessage);
  });

  server.onNotFound(notFound);

  //To handle clients
  server.begin();

  delay(100);
}

void codeForTask1( void * parameter ) {
  //timer for 1 minute
  int secondTimer;
  for (;;) {
    state = preferences.getString("state", "");
    if ((state == "0") || (secondTimer > 60000)) {
      replaceAll();
      server.end();
      SPIFFS.end();
      preferences.end();
      xTaskCreatePinnedToCore(    codeForTask2,    "scanBLE",    8000,    NULL,   1,    &Task2,    0);
      delay(100);
      xTaskCreatePinnedToCore(    codeForTask3,    "scanWIFI",    8000,    NULL,    1,    &Task3,    1);
      delay(100);
      xTaskCreatePinnedToCore(    codeForTask4,    "postBLE",    8000,    NULL,   1,    &Task4,    0);
      delay(100);
      xTaskCreatePinnedToCore(    codeForTask5,    "postWIFI",    8000,    NULL,   1,    &Task5,    1);
      delay(100);
      vTaskDelete(Task1);
    }
    else {
      delay(100);
      secondTimer = millis();
    }
  }
}


void codeForTask2( void * parameter ) {
  for (;;) {
    if (bleInterval.toInt() >= 0) {
      scanBLE();

      int waitBle;
      waitBle = bleInterval.toInt() - (scanTimeBle * 1000);

      if (waitBle >= 0 && waitBle <= bleInterval.toInt()) {
        delay(waitBle);
      }
      else {
        delay(100);
      }
    }

    else {
      debugln("BLE Disabled");
      debugln("------------------------------------------------------------");
      vTaskDelete(Task4);
      vTaskDelete(Task2);
    }
  }
}

void codeForTask3( void * parameter ) {
  for (;;) {
    if (wifiInterval.toInt() >= 0) {
      scanWIFI();

      unsigned long waitWifi;
      waitWifi = wifiInterval.toInt() - totalScanWifi;

      if (waitWifi >= 0 && waitWifi <= wifiInterval.toInt()) {
        delay(waitWifi);
      }
      else {
        delay(100);
      }
    }

    else {
      debugln("Wi-Fi Disabled");
      debugln("------------------------------------------------------------");
      vTaskDelete(Task5);
      vTaskDelete(Task3);
    }
  }
}

void codeForTask4( void * parameter ) {
  for (;;) {
    postBLE();
    //https://stackoverflow.com/questions/66278271/task-watchdog-got-triggered-the-tasks-did-not-reset-the-watchdog-in-time
    delay(100);
  }
}

void codeForTask5( void * parameter ) {
  for (;;) {
    postWIFI();
    //https://stackoverflow.com/questions/66278271/task-watchdog-got-triggered-the-tasks-did-not-reset-the-watchdog-in-time
    delay(100);
  }
}


void setup() {
  Serial.begin(115200);

  connectToNetwork();

  preferences.begin("credentials", false);
  preferences.putString("state", "1");

  if (!SPIFFS.begin(true)) {
    debugln("An Error has occurred while mounting SPIFFS");
    return;
  }

  writeFile(SPIFFS, "/inputState.txt", "1");

  serverForm();

  //Two queues that can hold a maximum of 1 element
  queueWIFI = xQueueCreate( 1, 20 );
  queueBLE = xQueueCreate( 1, 20 );

  if (queueWIFI == NULL) {
    debugln("Error creating the queue");
    debugln("------------------------------------------------------------");
  }

  if (queueBLE == NULL) {
    debugln("Error creating the queue");
    debugln("------------------------------------------------------------");
  }

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); // create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); // active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value

  //https://randomnerdtutorials.com/esp32-dual-core-arduino-ide/
  /* create Mutex */
  /*Syntax for assigning task to a core:
    xTaskCreatePinnedToCore(
                    TaskFunc,     // Function to implement the task
                    "TaskLabel",  // Name of the task
                    10000,        // Stack size in bytes
                    NULL,         // Task input parameter
                    0,            // Priority of the task
                    &TaskHandle,  // Task handle.
                    TaskCore);    // Core where the task should run
  */
  //Both tasks run on the same core so different priorities are attributed to each
  xTaskCreatePinnedToCore(    codeForTask1,    "serverForm",    8000,    NULL,    1,    &Task1,    1);
  delay(100);

}

void loop() {
  //The task which runs setup() and loop() is created on core 1 with priority 1.
  vTaskDelete(NULL);
  // to delete that task and free its resources because it isn't planed to use it at all.
}
