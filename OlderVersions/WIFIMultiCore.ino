//This program has multi-threading, two tasks run on the same core but with different priorities

#include "WiFi.h"
#include <HTTPClient.h>


QueueHandle_t queue;

const char* ssid = "ola12345";
const char* password =  "olaola123";

// Domain Name with full URL Path for HTTP POST Request
const char* serverName = "http://ils.dsi.uminho.pt/ar-ware/S02/i2a/i2aSamples.php";


// THE DEFAULT TIMER IS SET TO 1 SECOND FOR TESTING PURPOSES
// For a final application, check the API call limits per hour/minute to avoid getting blocked/banned
unsigned long lastTime = 0;
// Set timer to 10 minutes (600000)
//unsigned long timerDelay = 600000;
// Timer set to 10 seconds (10000)
unsigned long timerDelay = 1000;



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

  Serial.print("Number of networks found: ");
  Serial.println(numberOfNetworks);

  // The wi-fi data containing information about all the networks found has to be sent inside brackets
  adicionar += "[";

  for (int i = 0; i < numberOfNetworks; i++) {
    if (linhaUm == 0) {
      adicionar += ", ";
    } else linhaUm = 0;
    Serial.print(i + 1);
    Serial.print(") ");

    adicionar += "{\"bssid\": \"bssid(i)\",      \"rssi\": \"rssi(i)\",      \"ssid\": \"ssid(i)\",      \"encript\": \"encript(i)\"}";

    Serial.print("MAC address: ");
    Serial.println(WiFi.BSSIDstr(i));
    // The preset value of bssid is replaced by the bssid of the network number i found
    adicionar.replace("bssid(i)", (WiFi.BSSIDstr(i)));

    Serial.print("Signal strength: ");
    Serial.println(WiFi.RSSI(i));
    String rssiStr = String (WiFi.RSSI(i));
    // The preset value of rssi is replaced by the rssi of the network number i found
    adicionar.replace("rssi(i)", rssiStr);

    Serial.print("Network name: ");
    Serial.println(WiFi.SSID(i));
    // The preset value of ssid is replaced by the ssid of the network number i found
    adicionar.replace("ssid(i)", (WiFi.SSID(i)));


    Serial.print("Encryption type: ");
    String encryptionTypeDescription = translateEncryptionType(WiFi.encryptionType(i));
    Serial.println(encryptionTypeDescription);
    // The preset value of encryption is replaced by the encryption of the network number i found
    adicionar.replace("encript(i)", encryptionTypeDescription);
    Serial.println("-----------------------");

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
    Serial.println("Establishing connection to WiFi..");
  }

  Serial.println("Connected to network");

}

void htmlPost() {

  xQueueReceive(queue, &adicionarCopy, portMAX_DELAY);

  // Here we concatenate both parts to originate the final datagram to be sent
  String dataGramaFinal = dataGrama + adicionarCopy;
  Serial.println(dataGramaFinal);

  //Send an HTTP POST request every 1 second
  if ((millis() - lastTime) > timerDelay) {
    //Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      HTTPClient http;

      // Your Domain name with URL path or IP address with path
      http.begin(client, serverName);

      // Specify content-type header
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");



      // Send HTTP POST request

      int httpResponseCode = http.POST(dataGramaFinal);
      String response = http.getString();

      Serial.print("                                                            ");

      Serial.print("                                                            HTTP Response code: ");
      Serial.println(httpResponseCode);
      Serial.print("                                                            ");
      Serial.println(response);
      //We clear the content of the file so it can receive new data
      dataGramaFinal.remove(0);
      // Free resources
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis();
  }

  //We clear the content of the file so it can receive new data
  adicionarCopy.remove(0);

}

void codeForTask1( void * parameter ) {
  for (;;) {

    Serial.print("Code is running on Core: "); Serial.println( xPortGetCoreID());

    scanNetworks();

    Serial.println("#################################################################################################");

  }

}

void codeForTask2( void * parameter ) {
  for (;;) {

    Serial.print("                                                            Code is running on Core: "); Serial.println( xPortGetCoreID());

    htmlPost();
    delay(1000);
    Serial.println("                                                            #################################################################################################");

  }

}

TaskHandle_t Task1, Task2;

void setup() {

  Serial.begin(115200);

  queue = xQueueCreate( 1, 20 );

  if (queue == NULL) {
    Serial.println("Error creating the queue");
  }

  //After connecting to a network it's information is printed
  connectToNetwork();
  Serial.println(WiFi.localIP());
  // In the datagram to be sent it is required to identify which network the device is connected to and it's ssid
  dataGrama.replace("eduroam", (WiFi.SSID()));
  Serial.println(WiFi.macAddress());
  Serial.println(WiFi.BSSIDstr());
  // In the datagram to be sent it is required to identify which network the device is connected to and it's macAdress
  dataGrama.replace("00:11:22:33:44:55", (WiFi.macAddress()));

  Serial.println("Timer set to 1 second (timerDelay variable), it will take 1 second before publishing the first reading.");

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
  xTaskCreatePinnedToCore(    codeForTask1,    "ScanNetworks",    5000,    NULL,    4,    &Task1,    0);
  delay(500);  // needed to start-up task1
  xTaskCreatePinnedToCore(    codeForTask2,    "HTMLPost",    5000,    NULL,    2,    &Task2,    1);

}

void loop() {

}
