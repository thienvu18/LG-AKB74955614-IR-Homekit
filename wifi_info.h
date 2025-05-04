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
  while (!WiFi.isConnected())
  {
    delay(50);
    Serial.print(".");
  }
  Serial.print("\n");

  Serial.print("Connected to ");
  Serial.println(WiFi.SSID()); // WiFi
  Serial.print("IP :\t");
  Serial.println(WiFi.localIP()); // IP
  Serial.print("MAC :\t");
  Serial.println(WiFi.macAddress());
}
#endif /* _WIFI_INFO_H_ */
