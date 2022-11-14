/* In this program two json files are created with all available BLE devices and Wi-Fi networks as they are scanned.
  Then the json files containing all devices and networks found are sent to the server */

#include "WiFi.h"
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

String command;

#define DEBUG 1

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x)
#define debugln(x)
#endif

QueueHandle_t queueWIFI;
QueueHandle_t queueBLE;

TaskHandle_t Task1, Task2, Task3, Task4;

//Counter for BLE devices scanned
int number = 1;

int scanTime = 5; //In seconds
BLEScan* pBLEScan;

int WIFIinterval = 0;
int BLEinterval = 0;

int httpResponseCodeWIFI;
int httpResponseCodeBLE;

const char* ssid = "ZON-C510";
const char* password =  "Boas1234";

// Domain Name with full URL Path for HTTP POST Request
const char* serverName = "http://ils.dsi.uminho.pt/ar-ware/S02/i2a/i2aSamples.php";

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

// Here the "volatile" part of the json file is initialized. This part will be changing constantly
String adicionarBLE;

String adicionarBLECopy;

// Here the "static" part of the json file is initialized. It has several preset values that will be replaced as we scan the wi-fi networks.
String dataGramaWIFI = "scanData= { \"tagName\": \"tagPedro\", \"tagBSSID\": \"00:11:22:33:44:55\", \"tagNetwork\": \"eduroam\", \"dataType\": \"Wi-Fi\", \"scanMode\": \"auto\", \"WiFiData\":";

// Here the "volatile" part of the json file is initialized. This part will be changing constantly
String adicionarWIFI;

String adicionarWIFICopy;

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
  // put your main code here, to run repeatedly:
  // The wi-fi data containing information about all the networks found has to be sent inside brackets
  adicionarBLE += "[";
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  // The wi-fi data containing information about all the networks found has to be sent inside brackets
  adicionarBLE += "]}";
  //debug("[BLE] Devices found: ");
  //debugln(foundDevices.getCount());
  number = 1;
  pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory

  //It is necessary to remove the last comma
  int lastComma = adicionarBLE.lastIndexOf(',');
  adicionarBLE.remove(lastComma , 1);
  adicionarBLECopy = adicionarBLE;
  adicionarBLE.remove(0);
  xQueueOverwrite(queueBLE, &adicionarBLECopy);
}

void scanNetworks() {
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

  //debug("[WI-FI] networks found: ");
  //debugln(numberOfNetworks);
}

void postBLE() {

  unsigned long tempoBLE;

  xQueueReceive(queueBLE, &adicionarBLECopy, portMAX_DELAY);

  if ( adicionarBLECopy.length() > 0) {

    // Send HTTP POST request
    // Here we concatenate both parts to originate the final datagram to be sent
    String dataGramaBLEFinal = dataGramaBLE + adicionarBLECopy;

    int tamanhoBLE = dataGramaBLEFinal.length();

    int connectionCountBLE = 0;

    //Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      HTTPClient http;

      // Your Domain name with URL path or IP address with path
      if (connectionCountBLE == 0) {
        http.begin(client, serverName);
      } connectionCountBLE++;

      // Your Domain name with URL path or IP address with path
      http.begin(client, serverName);

      // Specify content-type header
      http.addHeader("Content-Type", "application/x-www-form-urlencoded", "Content-Length", tamanhoBLE);
      //http.addHeader("Content-Length", tamanhoBLE);
      http.addHeader("Accept", "*/*");
      http.addHeader("Accept-Encoding", "gzip, deflate, br");
      http.addHeader("Connection", "keep-alive");

      tempoBLE = millis();
      debug("[BLE] A: ");
      debugln(tempoBLE);

      int httpResponseCodeBLE = http.POST(dataGramaBLEFinal);

      tempoBLE = millis();
      debug("[BLE] B: ");
      debugln(tempoBLE);

      String responseBLE = http.getString();

      debug("[BLE] HTTP Response code: ");
      debugln(httpResponseCodeBLE);
      debugln(responseBLE);

    }
  }
}



void postWIFI() {

  unsigned long tempoWIFI;

  xQueueReceive(queueWIFI, &adicionarWIFICopy, portMAX_DELAY);

  if ( adicionarWIFICopy.length() > 0) {

    // Here we concatenate both parts to originate the final datagram to be sent
    String dataGramaWIFIFinal = dataGramaWIFI + adicionarWIFICopy;
    int tamanhoWIFI = dataGramaWIFIFinal.length();

    int connectionCountWIFI = 0;

    //Send an HTTP POST request every 1 second

    //Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      HTTPClient http;

      // Your Domain name with URL path or IP address with path
      if (connectionCountWIFI == 0) {
        http.begin(client, serverName);
      } connectionCountWIFI++;

      // Specify content-type header
      http.addHeader("Content-Type", "application/x-www-form-urlencoded", "Content-Length", tamanhoWIFI);
      //http.addHeader("Content-Length", tamanhoWIFI);
      http.addHeader("Accept", "*/*");
      http.addHeader("Accept-Encoding", "gzip, deflate, br");
      http.addHeader("Connection", "keep-alive");

      // Send HTTP POST request

      tempoWIFI = millis();
      debug("[Wi-Fi] A: ");
      debugln(tempoWIFI);

      vTaskSuspend(Task3);
      httpResponseCodeWIFI = http.POST(dataGramaWIFIFinal);
      vTaskResume(Task3);

      tempoWIFI = millis();
      debug("[Wi-Fi] B: ");
      debugln(tempoWIFI);

      String responseWifi = http.getString();

      debug("[Wi-Fi] HTTP Response code: ");
      debugln(httpResponseCodeWIFI);
      debugln(responseWifi);

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
    vTaskDelay(BLEinterval);
  }
}

void codeForTask2( void * parameter ) {
  for (;;) {
    postBLE();
  }
}

void codeForTask3( void * parameter ) {
  for (;;) {
    scanNetworks();
    vTaskDelay(WIFIinterval);
  }

}

void codeForTask4( void * parameter ) {
  for (;;) {
    postWIFI();
  }

}

void setup() {

  Serial.begin(115200);
  Serial.setTimeout(10000); //TO WRITE ON SERIAL

  queueWIFI = xQueueCreate( 1, 20 );
  queueBLE = xQueueCreate( 1, 20 );

  if (queueWIFI == NULL) {
    debugln("Error creating the queue");
  }

  if (queueBLE == NULL) {
    debugln("Error creating the queue");
  }

  //After connecting to a network it's information is printed
  connectToNetwork();

  // In the datagram to be sent it is required to identify which network the device is connected to and it's ssid
  dataGramaWIFI.replace("eduroam", (WiFi.SSID()));

  // In the datagram to be sent it is required to identify which network the device is connected to and it's macAdress
  dataGramaWIFI.replace("00:11:22:33:44:55", (WiFi.macAddress()));

  // In the datagram to be sent it is required to identify which network the device is connected to and it's ssid
  dataGramaBLE.replace("eduroam", (WiFi.SSID()));

  // In the datagram to be sent it is required to identify which network the device is connected to and it's macAdress
  dataGramaBLE.replace("00:11:22:33:44:99", (WiFi.macAddress()));

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
  xTaskCreatePinnedToCore(    codeForTask1,    "scanBLE",    4000,    NULL,    2,    &Task1,    0);
  xTaskCreatePinnedToCore(    codeForTask2,    "postBLE",    4000,    NULL,    2,    &Task2,    0);
  xTaskCreatePinnedToCore(    codeForTask3,    "scanNetworks",    4000,    NULL,    2,    &Task3,    1);
  xTaskCreatePinnedToCore(    codeForTask4,    "postWIFI",    4000,    NULL,   4,    &Task4,    1);
}

//The task which runs setup() and loop() is created on core 1 with priority 1.
void loop() {
  if (Serial.available()) {
    command = Serial.readStringUntil('\n');

    if (command.equals("Change WIFIinterval")) {
      debugln("To how long?");
      command = Serial.readStringUntil('\n');
      if (command.toInt() == -1) {
        vTaskSuspend(Task3);
      }
      else {
        vTaskResume(Task3);
        WIFIinterval = command.toInt() * 1000;
      }
    }

    if (command.equals("Change BLEinterval")) {
      debugln("To how long?");
      command = Serial.readStringUntil('\n');
      if (command.toInt() == -1) {
        vTaskSuspend(Task1);
      }
      else {
        vTaskResume(Task1);
        BLEinterval = command.toInt() * 1000;
      }
    }
  }
  //vTaskDelete(NULL);
}
// to delete that task and free its resources because it isn't planed to use it at all.
