#include <WiFi.h>

const char* wifi_network_ssid     = "ZON-C510";
const char* wifi_network_password =  "Boas1234";

const char *soft_ap_ssid          = "ESP32_AP";
const char *soft_ap_password      = NULL;

void setup()
{
    Serial.begin(115200);
    WiFi.mode(WIFI_AP_STA);

    Serial.println("\n[*] Creating ESP32 AP");
    WiFi.softAP(soft_ap_ssid, soft_ap_password);
    Serial.print("[+] AP Created with IP Gateway ");
    Serial.println(WiFi.softAPIP());

    WiFi.begin(wifi_network_ssid, wifi_network_password);
    Serial.println("\n[*] Connecting to WiFi Network");

    while(WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(100);
    }

    Serial.print("\n[+] Connected to the WiFi network with local IP : ");
    Serial.println(WiFi.localIP());
}

void loop() {}
