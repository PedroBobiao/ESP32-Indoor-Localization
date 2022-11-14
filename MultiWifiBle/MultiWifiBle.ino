/* This program starts by acessing a webserver and withdrawing it's information. Then the information is parsed and stored in the permanent storage of the ESP32. Then, the "permanent" file of the ESP32 is acessed to retrieve the information. After that two json files are created with all available BLE devices and Wi-Fi networks as they are scanned. Afterwards the json files containing all devices and networks found are sent to the webserver */

#include "WiFi.h"
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <Preferences.h>
#include <BLEAdvertisedDevice.h>

// https://www.youtube.com/watch?v=--KxxMaiwSE
#define DEBUG 1

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x)
#define debugln(x)
#endif

// https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/
Preferences preferences;

const char* ssidC = "ZON-C510";
const char* passwordC =  "Boas1234";

String ssidGot;
String passwordGot;
String serverNameGot;
String tagNameGot;
String wifiIntervalGot;
String bleIntervalGot;

String ssid;
String password;
String serverName;
String tagName;
String wifiInterval;
String bleInterval;

// https://techtutorialsx.com/2017/08/20/esp32-arduino-freertos-queues/
QueueHandle_t queueWIFI;
QueueHandle_t queueBLE;

TaskHandle_t Task1, Task2, Task3, Task4;

//Counter for BLE devices scanned
int number = 1;

unsigned long totalScanWifi;

//Can be changed, in seconds
int scanTimeBle = 1;

BLEScan* pBLEScan;

int response200Wifi;
int response200Ble;
int responseOtherWifi;
int responseOtherBle;

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

String adicionarWIFICopy;

void connectToNetwork() {
  WiFi.begin(ssidC, passwordC);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    debugln("Establishing connection to WiFi..");
  }

  debugln("Connected to network");

}

//Get the Information from the server
void getInfo() {

  if ((WiFi.status() == WL_CONNECTED)) { //Check the current connection status

    HTTPClient http;

    http.begin("http://192.168.1.3/"); //Specify the URL (https://serverfault.com/questions/209150/how-to-access-localhost-by-ip-address#:~:text=To%20access%20the%20server%20from,)%20by%20running%20hostname%20%2DI%20.)
    int httpCode = http.GET();                                        //Make the request

    if (httpCode > 0) { //Check for the returning code

      String payload = http.getString();
      String indice = payload;

      int indice1;
      int indice2;
      int indice3;
      int indice4;
      int indice5;
      int indice6;
      int indice7;
      int indice8;
      int indice9;
      int indice10;
      int indice11;
      int indice12;

      indice1 = (indice.indexOf("ssid") + 8);
      indice2 = (indice.indexOf("password") - 6);

      indice3 = (indice.indexOf("password") + 12);
      indice4 = (indice.indexOf("serverName") - 6);

      indice5 = (indice.indexOf("serverName") + 14);
      indice6 = (indice.indexOf("tagName") - 6);

      indice7 = (indice.indexOf("tagName") + 11);
      indice8 = (indice.indexOf("WIFIinterval") - 6);

      indice9 = (indice.indexOf("WIFIinterval") + 15);
      indice10 = (indice.indexOf("BLEinterval") - 5);

      indice11 = (indice.indexOf("BLEinterval") + 14);
      indice12 = (indice.lastIndexOf("}"));

      ssidGot = payload.substring(indice1, indice2);
      debugln(ssidGot);

      passwordGot = payload.substring(indice3, indice4);
      debugln(passwordGot);

      serverNameGot = payload.substring(indice5, indice6);
      debugln(serverNameGot);

      tagNameGot = payload.substring(indice7, indice8);
      debugln(tagNameGot);

      wifiIntervalGot = payload.substring(indice9, indice10);
      debugln(wifiIntervalGot);

      bleIntervalGot = payload.substring(indice11, indice12);
      debugln(bleIntervalGot);
    }

    else {
      debugln("Error on HTTP request");
    }

    http.end(); //Free the resources
  }
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

  debugln("Scan BLE");
  debugln("------------------------------------------------------------");
  // put your main code here, to run repeatedly:
  // The wi-fi data containing information about all the networks found has to be sent inside brackets
  adicionarBLE += "[";
  BLEScanResults foundDevices = pBLEScan->start(scanTimeBle, false);
  // The wi-fi data containing information about all the networks found has to be sent inside brackets
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

  debugln("Scan Wi-Fi");
  debugln("------------------------------------------------------------");
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
      WiFiClient client;
      HTTPClient http;

      // Your Domain name with URL path or IP address with path
      if (connectionCountBLE == 0) {
        http.begin(client, serverName);
      } connectionCountBLE++;

      // Specify content-type header
      http.addHeader("Content-Type", "application/x-www-form-urlencoded", "Content-Length", tamanhoBLE);
      http.addHeader("Accept", "*/*");
      http.addHeader("Accept-Encoding", "gzip, deflate, br");
      http.addHeader("Connection", "keep-alive");

      if (wifiInterval.toInt() >= 0) {
        //if no intervals are used it is needed to suspend wifiScans in order for the posts to be successful
        //vTaskSuspend(Task3);
      }
      int httpResponseCodeBLE = http.POST(dataGramaBLEFinal);
      if (wifiInterval.toInt() >= 0) {
        //if no intervals are used it is needed to suspend wifiScans in order for the posts to be successful
        //vTaskResume(Task3);
      }

      String responseBLE = http.getString();

      if (httpResponseCodeBLE == 200) {
        response200Ble++;
      }
      else {
        debug("[BLE] HTTP Response code: ");
        debugln(httpResponseCodeBLE);
        debugln(responseBLE);
        debugln("------------------------------------------------------------");
        responseOtherBle++;
      }

    }
  }
}


//https://randomnerdtutorials.com/esp32-http-get-post-arduino/#http-post

void postWIFI() {

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
      http.addHeader("Accept", "*/*");
      http.addHeader("Accept-Encoding", "gzip, deflate, br");
      http.addHeader("Connection", "keep-alive");

      // Send HTTP POST request
      if (wifiInterval.toInt() >= 0) {
        //if no intervals are used it is needed to suspend wifiScans in order for the posts to be successful
        //vTaskSuspend(Task3);
      }
      int httpResponseCodeWIFI = http.POST(dataGramaWIFIFinal);
      if (wifiInterval.toInt() >= 0) {
        //if no intervals are used it is needed to suspend wifiScans in order for the posts to be successful
        //vTaskResume(Task3);
      }

      String responseWifi = http.getString();

      if (httpResponseCodeWIFI == 200) {
        response200Wifi++;
      }
      else {
        debug("[Wi-Fi] HTTP Response code: ");
        debugln(httpResponseCodeWIFI);
        debugln(responseWifi);
        debugln("------------------------------------------------------------");
        responseOtherWifi++;
      }

    }
  }
}

void codeForTask1( void * parameter ) {
  for (;;) {
    if (bleInterval.toInt() >= 0) {
      scanBLE();

      int waitBle;
      waitBle = bleInterval.toInt() - (scanTimeBle * 1000);

      if (waitBle >= 0 && waitBle <= bleInterval.toInt()) {
        delay(waitBle);
      }
      else {
        delay(500);
      }
    }

    else {
      debugln("BLE Disabled");
      debugln("------------------------------------------------------------");
      vTaskDelete(Task1);
      vTaskDelete(Task2);
    }
  }
}

void codeForTask2( void * parameter ) {
  for (;;) {
    postBLE();
    //https://stackoverflow.com/questions/66278271/task-watchdog-got-triggered-the-tasks-did-not-reset-the-watchdog-in-time
    delay(500);
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
        //if no intervals are used it is needed to suspend wifiScans in order for the posts to be successful but there can also be concurring suspends and resumes
      }
      else {
        delay(500);
      }
    }

    else {
      debugln("Wi-Fi Disabled");
      debugln("------------------------------------------------------------");
      vTaskDelete(Task3);
      vTaskDelete(Task4);
    }
  }
}

void codeForTask4( void * parameter ) {
  for (;;) {
    postWIFI();
    //https://stackoverflow.com/questions/66278271/task-watchdog-got-triggered-the-tasks-did-not-reset-the-watchdog-in-time
    delay(500);
  }

}

void setup() {

  Serial.begin(115200);

  //After connecting to a network it's information is printed
  connectToNetwork();

  //Get the Information from the server
  getInfo();

  preferences.begin("credentials", false);
  preferences.putString("ssid", ssidGot);
  preferences.putString("password", passwordGot);
  preferences.putString("serverName", serverNameGot);
  preferences.putString("tagName", tagNameGot);
  preferences.putString("WIFIinterval", wifiIntervalGot);
  preferences.putString("BLEinterval", bleIntervalGot);

  debugln("Credentials Saved using Preferences");

  preferences.end();


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

  preferences.begin("credentials", false);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  serverName = preferences.getString("serverName", "");
  tagName = preferences.getString("tagName", "");
  wifiInterval = preferences.getString("WIFIinterval", "");
  bleInterval = preferences.getString("BLEinterval", "");

  if (ssid == "" || password == "" || serverName == "" || tagName == "" || wifiInterval == "" || bleInterval == "") {
    debugln("No values saved for credentials");
  }
  else {
    // Connect to Wi-Fi
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    debug("Connecting to Wi-Fi ..");
    while (WiFi.status() != WL_CONNECTED) {
      debug('.');
      delay(1000);
    }
    debugln(" Connected to network");
    debugln("------------------------------------------------------------");
  }

  // In the datagram to be sent it is required to identify which network the device is connected to and it's ssid
  dataGramaWIFI.replace("eduroam", (WiFi.SSID()));

  // In the datagram to be sent it is required to identify which network the device is connected to and it's macAdress
  dataGramaWIFI.replace("00:11:22:33:44:55", (WiFi.macAddress()));

  // In the datagram to be sent it is required to identify which network the device is connected to and it's macAdress
  dataGramaWIFI.replace("tagPedro", tagName);

  // In the datagram to be sent it is required to identify which network the device is connected to and it's ssid
  dataGramaBLE.replace("eduroam", (WiFi.SSID()));

  // In the datagram to be sent it is required to identify which network the device is connected to and it's macAdress
  dataGramaBLE.replace("00:11:22:33:44:99", (WiFi.macAddress()));

  // In the datagram to be sent it is required to identify which network the device is connected to and it's macAdress
  dataGramaBLE.replace("tagPedro", tagName);

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); // create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); // active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value

  //https://randomnerdtutorials.com/esp32-dual-core-arduino-ide/
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
  xTaskCreatePinnedToCore(    codeForTask1,    "scanBLE",    10000,    NULL,    1,    &Task1,    0);
  delay(500);
  xTaskCreatePinnedToCore(    codeForTask2,    "postBLE",    10000,    NULL,    1,    &Task2,    0);
  delay(500);
  xTaskCreatePinnedToCore(    codeForTask3,    "scanWIFI",    10000,    NULL,    1,    &Task3,    1);
  delay(500);
  xTaskCreatePinnedToCore(    codeForTask4,    "postWIFI",    10000,    NULL,   1,    &Task4,    1);
  delay(500);
}

//The task which runs setup() and loop() is created on core 1 with priority 1.
void loop() {
  /*
    // https://www.norwegiancreations.com/2017/12/arduino-tutorial-serial-inputs/
    String command;

    if (Serial.available()) {
      command = Serial.readStringUntil('\n');

      if (command.equals("Results")) {
        debug("200 Wi-Fi Responses: ");
        debugln(response200Wifi);
        debug("Other Wi-Fi Responses: ");
        debugln(responseOtherWifi);
        debug("200 BLE Responses: ");
        debugln(response200Ble);
        debug("Other BLE Responses: ");
        debugln(responseOtherBle);
        debugln("------------------------------------------------------------");
      }

      else {
        debugln("Invalid command");
        debugln("------------------------------------------------------------");
      }
    }

    delay(1000);
  */
  vTaskDelete(NULL);
}
// to delete that task and free its resources because it isn't planed to use it at all.
