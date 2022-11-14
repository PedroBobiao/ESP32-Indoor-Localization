//This program has multicore-threading, two tasks run on different cores with different priorities

#include "WiFi.h"
#include <HTTPClient.h>

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

String payload;
int httpResponseCode;

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

// Here the "static" part of the json file is initialized. It has several preset values that will be replaced as we scan the wi-fi networks.
String dataGrama = "scanData= { \"tagName\": \"tagPedro\", \"tagBSSID\": \"00:11:22:33:44:55\", \"tagNetwork\": \"eduroam\", \"dataType\": \"Wi-Fi\", \"scanMode\": \"auto\", \"WiFiData\":";

// Here the "volatile" part of the json file is initialized. This part will be changing constantly
String adicionar;

String adicionarCopy;


void scanNetworks() {
  int linhaUm = 1;
  int numberOfNetworks = WiFi.scanNetworks();

  debug("Number of networks found: ");
  debugln(numberOfNetworks);

  // The wi-fi data containing information about all the networks found has to be sent inside brackets
  adicionar += "[";

  for (int i = 0; i < numberOfNetworks; i++) {
    if (linhaUm == 0) {
      adicionar += ", ";
    } else linhaUm = 0;

    adicionar += "{\"bssid\": \"bssid(i)\",      \"rssi\": \"rssi(i)\",      \"ssid\": \"ssid(i)\",      \"encript\": \"encript(i)\"}";


    // The preset value of bssid is replaced by the bssid of the network number i found
    adicionar.replace("bssid(i)", (WiFi.BSSIDstr(i)));


    String rssiStr = String (WiFi.RSSI(i));
    // The preset value of rssi is replaced by the rssi of the network number i found
    adicionar.replace("rssi(i)", rssiStr);


    // The preset value of ssid is replaced by the ssid of the network number i found
    adicionar.replace("ssid(i)", (WiFi.SSID(i)));



    String encryptionTypeDescription = translateEncryptionType(WiFi.encryptionType(i));

    // The preset value of encryption is replaced by the encryption of the network number i found
    adicionar.replace("encript(i)", encryptionTypeDescription);


  }

  // The wi-fi data containing information about all the networks found has to be sent inside brackets
  adicionar += "]}";

  adicionarCopy = adicionar;


  adicionar.remove(0);

  xQueueOverwrite(queue, &adicionarCopy);


}

void connectToNetwork() {
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    debugln("Establishing connection to WiFi..");
  }

  debugln("Connected to network");

}

int connectionCount = 0;

HTTPClient http;

void htmlPost() {

  unsigned long tempo;

  xQueueReceive(queue, &adicionarCopy, portMAX_DELAY);

  if ( adicionarCopy.length() > 0) {


    // Here we concatenate both parts to originate the final datagram to be sent
    String dataGramaFinal = dataGrama + adicionarCopy;
    debugln(dataGramaFinal);
    int tamanho = dataGramaFinal.length();

    //Send an HTTP POST request every 1 second

    //Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;


      // Your Domain name with URL path or IP address with path
      if (connectionCount == 0) {
        http.begin(client, serverName);
      } connectionCount++;



      // Specify content-type header
      http.addHeader("Content-Type", "application/x-www-form-urlencoded", "Content-Length", tamanho);
      //http.addHeader("Content-Length", tamanho);
      http.addHeader("Accept", "*/*");
      http.addHeader("Accept-Encoding", "gzip, deflate, br");
      http.addHeader("Connection", "keep-alive");



      // Send HTTP POST request

      tempo = millis();
      Serial.print("A: ");
      Serial.println(tempo);
      //vTaskSuspend(Task1);
      httpResponseCode = http.POST(dataGramaFinal);
      //vTaskResume(Task1);

      tempo = millis();
      Serial.print("B: ");
      Serial.println(tempo);


      if (httpResponseCode > 0) {
        payload = http.getString();
        Serial.println(payload);
      }

    }
    else {
      debugln("WiFi Disconnected");
    }

  }


}




void codeForTask1( void * parameter ) {
  for (;;) {
    Serial.println("starting task1");
    scanNetworks();
    Serial.println("ending task1");
  }

}

void codeForTask2( void * parameter ) {
  for (;;) {
    Serial.println("starting task2");
    htmlPost();
    Serial.println("ending task1");
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
  dataGrama.replace("00:11:22:33:44:55", (WiFi.macAddress()));



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
  xTaskCreatePinnedToCore(    codeForTask2,    "HTMLPost",    10000,    NULL,    0,    &Task2,    1);
  xTaskCreatePinnedToCore(    codeForTask1,    "ScanNetworks",    10000,    NULL,    0,    &Task1,    0);

}

void loop() {

}
