/* This is the first section of the program. It starts by acessing a server and withdrawing it's information. Then the information is parsed and stored in the permanent storage of the ESP32  */

#include "WiFi.h"
#include <HTTPClient.h>
#include <Preferences.h>

#define DEBUG 1

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#else
#define debug(x)
#define debugln(x)
#endif

Preferences preferences;

const char* ssid = "ZON-C510";
const char* password =  "Boas1234";

String ssidGot;
String passwordGot;
String serverNameGot;
String tagNameGot;
String wifiIntervalGot;
String bleIntervalGot;

void connectToNetwork() {
  WiFi.begin(ssid, password);

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

    http.begin("http://192.168.1.6/"); //Specify the URL (https://serverfault.com/questions/209150/how-to-access-localhost-by-ip-address#:~:text=To%20access%20the%20server%20from,)%20by%20running%20hostname%20%2DI%20.)
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

}

//The task which runs setup() and loop() is created on core 1 with priority 1.
void loop() {}
