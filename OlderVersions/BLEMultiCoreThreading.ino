/* In this program a json file is created with all available BLE devices as they are scanned.
  Then the json file containing all devices found is sent to the server */

#include "WiFi.h"
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define DEBUG 1

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x)
#define debugln(x)
#endif


QueueHandle_t queue;

TaskHandle_t Task1, Task2;

//Counter for BLE devices scanned
int number = 1;

int scanTime = 5; //In seconds
BLEScan* pBLEScan;

const char* ssid = "ZON-C510";
const char* password =  "Boas1234";

// Domain Name with full URL Path for HTTP POST Request
const char* serverName = "http://ils.dsi.uminho.pt/ar-ware/S02/i2a/i2aSamples.php";


unsigned long tempo;


// Here the "static" part of the json file is initialized. It has several preset values that will be replaced as we scan the bluetooth networks.
String dataGrama = "scanData= {	\"tagName\": \"tagPedro\",	\"tagBSSID\": \"00:11:22:33:44:99\",	\"tagNetwork\": \"eduroam\",	\"dataType\": \"BLE\",	\"scanMode\": \"auto\",	\"BLEData\":";

// Here the "volatile" part of the json file is initialized. This part will be changing constantly
String adicionar;

String adicionarCopy;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {



    void onResult(BLEAdvertisedDevice advertisedDevice) {

      // The bluetooth data containing information about all the networks found has to be sent inside brackets
      adicionar += "{\"bssid\": \"bssid(i)\",      \"rssi\": \"rssi(i)\",      \"ssid\": \"ssid(i)\"}";

      // The preset value of bssid is replaced by the bssid of the device found
      adicionar.replace("bssid(i)", (advertisedDevice.getAddress().toString().c_str()));
      String rssiStr = String (advertisedDevice.getRSSI());
      // The preset value of rssi is replaced by the rssi of the device found
      adicionar.replace("rssi(i)", rssiStr);
      String nome = advertisedDevice.getName().c_str();
      // The preset value of ssid is replaced by the ssid of the device found
      adicionar.replace("ssid(i)", nome);
      number ++;
      adicionar += ", ";
    }
};


void scanBLE() {
  // put your main code here, to run repeatedly:
  // The wi-fi data containing information about all the networks found has to be sent inside brackets
  adicionar += "[";
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  // The wi-fi data containing information about all the networks found has to be sent inside brackets
  adicionar += "]}";
  debug("Devices found: ");
  debugln(foundDevices.getCount());
  number = 1;
  pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory



  //It is necessary to remove the last comma
  int lastComma = adicionar.lastIndexOf(',');
  adicionar.remove(lastComma , 1);
  adicionarCopy = adicionar;
  adicionar.remove(0);
  xQueueOverwrite(queue, &adicionarCopy);
}

void postBLE() {

  xQueueReceive(queue, &adicionarCopy, portMAX_DELAY);

  if ( adicionarCopy.length() > 0) {

  
  // Send HTTP POST request
  // Here we concatenate both parts to originate the final datagram to be sent
  String dataGramaFinal = dataGrama + adicionarCopy;

  int tamanho = dataGramaFinal.length();

  int connectionCount = 0;

  //Check WiFi connection status
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    // Your Domain name with URL path or IP address with path
    if (connectionCount == 0) {
      http.begin(client, serverName);
    } connectionCount++;

    // Your Domain name with URL path or IP address with path
    http.begin(client, serverName);

    // Specify content-type header
    http.addHeader("Content-Type", "application/x-www-form-urlencoded", "Content-Length", tamanho);
    //http.addHeader("Content-Length", tamanho);
    http.addHeader("Accept", "*/*");
    http.addHeader("Accept-Encoding", "gzip, deflate, br");
    http.addHeader("Connection", "keep-alive");

    tempo = millis();
    debug("A: ");
    debugln(tempo);

    int httpResponseCode = http.POST(dataGramaFinal);

    tempo = millis();
    debug("B: ");
    debugln(tempo);

    String response = http.getString();

    debug("HTTP Response code: ");
    debugln(httpResponseCode);
    debugln(response);


    // Free resources
    http.end();
  }
  else {
    debugln("WiFi Disconnected");
  }




}

}


void connectToNetwork() {
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    debugln("Establishing connection to WiFi..");
  }

  debugln("Connected to network");

}


void codeForTask1( void * parameter ) {
  for (;;) {
    scanBLE();
  }

}

void codeForTask2( void * parameter ) {
  for (;;) {
    postBLE();
  }

}

void setup() {
  Serial.begin(115200);

  queue = xQueueCreate( 1, 20 );

  if (queue == NULL) {
    debugln("Error creating the queue");
  }

  //After connecting to a network it's information is printed
  connectToNetwork();

  // In the datagram to be sent it is required to identify which network the device is connected to and it's ssid
  dataGrama.replace("eduroam", (WiFi.SSID()));

  // In the datagram to be sent it is required to identify which network the device is connected to and it's macAdress
  dataGrama.replace("00:11:22:33:44:99", (WiFi.macAddress()));


  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value


  pinMode(D9, OUTPUT);
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
  xTaskCreatePinnedToCore(    codeForTask1,    "scanBLE",    4000,    NULL,    0,    &Task1,    0);
  xTaskCreatePinnedToCore(    codeForTask2,    "postBLE",    4000,    NULL,    20,    &Task2,    1);
}


void loop() {
vTaskDelete(NULL);
}
