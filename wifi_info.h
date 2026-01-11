#ifndef _WIFI_INFO_H_
#define _WIFI_INFO_H_

#include <Arduino.h>
#include <stdio.h>
#include <ESP8266WiFi.h>

const char *ssid = "SSID";
const char *password = "********";

void wifi_connect()
{
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  Serial.println("WiFi connecting...");
  int attempts = 0;
  while (!WiFi.isConnected() && attempts < 200)
  {
    delay(50);
    Serial.print(".");
    attempts++;
    yield(); // Feed watchdog
  }
  Serial.print("\n");

  if (!WiFi.isConnected())
  {
    Serial.println("WiFi failed to connect, restarting...");
    delay(1000);
    ESP.restart();
  }

  Serial.print("Connected to ");
  Serial.println(WiFi.SSID()); // WiFi
  Serial.print("IP :\t");
  Serial.println(WiFi.localIP()); // IP
  Serial.print("MAC :\t");
  Serial.println(WiFi.macAddress());
}
#endif /* _WIFI_INFO_H_ */
