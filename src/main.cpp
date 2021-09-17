#include <Arduino.h>

#include "esp32-mqtt.h"
#include "esp_system.h"

#include <DNSServer.h>

#include <WebServer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#define VARIANT "esp32"

#include <WiFiManager.h>

#define USE_SERIAL Serial

#define CLOUD_FUNCTION_URL "https://asia-east1-farms-arduino-270802.cloudfunctions.net/getDownloadUrl" // CAN BE FOUND UNDER TRIGGER TAB
                            //https://asia-east1-farms-arduino-270802.cloudfunctions.net/getDownloadUrl?version=v1.0.1&variant=esp32
WiFiClient client;
WebServer server(80);

hw_timer_t *watchdogTimer = NULL;
long looptime = 0;

/*
 * Check if needs to update the device and returns the download url.
 */
String getDownloadUrl()
{
  HTTPClient http;
  String downloadUrl;
  USE_SERIAL.print("[HTTP] begin...\n");

  String url = CLOUD_FUNCTION_URL;
  url += String("?version=") + CURRENT_VERSION;
  url += String("&variant=") + VARIANT;
  http.begin(url);

  USE_SERIAL.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();

  // httpCode will be negative on error
  if (httpCode > 0)
  {
    // HTTP header has been send and Server response header has been handled
    USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();
      USE_SERIAL.println(payload);
      downloadUrl = payload;
    }
    else
    {
      USE_SERIAL.println("Device is up to date!");
    }
  }
  else
  {
    USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();

  return downloadUrl;
}

/*
 * Download binary image and use Update library to update the device.
 */
bool downloadUpdate(String url)
{
  HTTPClient http;
  USE_SERIAL.print("[HTTP] Download begin...\n");

  http.begin(url);

  USE_SERIAL.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();
  if (httpCode > 0)
  {
    // HTTP header has been send and Server response header has been handled
    USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK)
    {

      int contentLength = http.getSize();
      USE_SERIAL.println("contentLength : " + String(contentLength));

      if (contentLength > 0)
      {
        bool canBegin = Update.begin(contentLength);
        if (canBegin)
        {
          WiFiClient stream = http.getStream();
          USE_SERIAL.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
          size_t written = Update.writeStream(stream);

          if (written == contentLength)
          {
            USE_SERIAL.println("Written : " + String(written) + " successfully");
          }
          else
          {
            USE_SERIAL.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?");
          }

          if (Update.end())
          {
            USE_SERIAL.println("OTA done!");
            if (Update.isFinished())
            {
              USE_SERIAL.println("Update successfully completed. Rebooting.");
              ESP.restart();
              return true;
            }
            else
            {
              USE_SERIAL.println("Update not finished? Something went wrong!");
              return false;
            }
          }
          else
          {
            USE_SERIAL.println("Error Occurred. Error #: " + String(Update.getError()));
            return false;
          }
        }
        else
        {
          USE_SERIAL.println("Not enough space to begin OTA");
          client.flush();
          return false;
        }
      }
      else
      {
        USE_SERIAL.println("There was no content in the response");
        client.flush();
        return false;
      }
    }
    else
    {
      return false;
    }
  }
  else
  {
    return false;
  }
}

/*
 * Show current device version
 */
void handleRoot()
{
  server.send(200, "text/plain", "v" + String(CURRENT_VERSION));
}

void interruptReboot(){
  Serial.println("Rebooting...");
  esp_restart();
}

void setup()
{
  Serial.begin(115200);
  Wire.begin();
  hdc1080.begin(0x40);
  Serial.setDebugOutput(true);
  delay(3000);
  Serial.println("\n Starting");

  pinMode(ledPower, OUTPUT); digitalWrite(ledPower, HIGH);
  pinMode(ledNet, OUTPUT);
  pinMode(ledData, OUTPUT);

  EEPROM.begin(EEPROM_SIZE);
  readEEPROM();

  watchdogTimer = timerBegin(0, 80, true);                        //timer 0 divisor 80
  timerAlarmWrite(watchdogTimer, 300000 * 1000, false);           // time in uS: 5mins
  timerAttachInterrupt(watchdogTimer, & interruptReboot, true);
  timerAlarmEnable(watchdogTimer);                                // enable interrupt 

  //Setup Wifi Credentials
  setupCloudIoT();

  // Check if we need to download a new version
  String downloadUrl = getDownloadUrl();
  if (downloadUrl.length() > 0)
  {
    bool success = downloadUpdate(downloadUrl);
    if (!success)
    {
      USE_SERIAL.println("Error updating device");
    }
  }

  server.on("/", handleRoot);
  server.begin();
  USE_SERIAL.println("HTTP server started");

  USE_SERIAL.print("IP address: "); USE_SERIAL.println(WiFi.localIP());
  Serial.println("Net connected.");

}

const long interval = 1000;
unsigned long previousMillis = 0;

void loop()
{

  unsigned long currentMillis = millis();

  timerWrite(watchdogTimer, 0);
  mqtt->loop();
  delay(10);                          // <- fixes some issues with WiFi stability

  if (!mqttClient->connected()) {
    connect();
  }

  if (connectnetwork == true){
    digitalWrite(ledNet, HIGH);
  }

  if(currentMillis - previousMillis >= interval){
    previousMillis = currentMillis;
    Serial.println("Hello There!");
  }

  /*
  if (millis() - lastMillis >= 60000) {

    lastMillis = millis();
    Serial.println("Publishing telemetry data:");
    publishTelemetry(getSensor());
  }
  */

  // Just chill
  server.handleClient();
}
