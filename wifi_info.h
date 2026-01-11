#ifndef _WIFI_INFO_H_
#define _WIFI_INFO_H_

#include <Arduino.h>
#include <stdio.h>
#include <ESP8266WiFi.h>

// Import PRINT_DEBUG macro from main file
#ifndef PRINT_DEBUG
#define PRINT_DEBUG 1
#endif

const char *ssid = "SSID";
const char *password = "********";

void wifi_connect()
{
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  #if PRINT_DEBUG
  Serial.println("WiFi connecting...");
  #endif
  
  int attempts = 0;
  while (!WiFi.isConnected() && attempts < 200)
  {
    delay(50);
    #if PRINT_DEBUG
    Serial.print(".");
    #endif
    attempts++;
    yield(); // Feed watchdog
  }
  
  #if PRINT_DEBUG
  Serial.println();
  #endif

  if (!WiFi.isConnected())
  {
    #if PRINT_DEBUG
    Serial.println("WiFi failed to connect, restarting...");
    #endif
    delay(1000);
    ESP.restart();
  }

  // Reduce String allocations - only print IP (doesn't allocate String)
  #if PRINT_DEBUG
  Serial.println("WiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP()); // IPAddress is safe, doesn't allocate String
  #endif
  // Avoid WiFi.SSID() and WiFi.macAddress() - they create String objects causing memory fragmentation
}
#endif /* _WIFI_INFO_H_ */
