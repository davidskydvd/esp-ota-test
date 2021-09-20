#ifndef __ESP32_MQTT_H__
#define __ESP32_MQTT_H__

#include <Client.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <MQTT.h>
#include <CloudIoTCore.h>
#include <CloudIoTCoreMqtt.h>
#include "ciotc_config.h"         // Update this file with your configuration
#include "ESP32_MailClient.h"
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager

#define CURRENT_VERSION VERSION

//EEPROM
#include <EEPROM.h>
#define EEPROM_SIZE 3

// GMAIL SENDER AND SERVER
#define emailSenderAccount    "ddstmesp32@gmail.com"
#define emailSenderPassword   "esp32ddstm"
#define smtpServer            "smtp.gmail.com"
#define smtpServerPort        465
#define emailSubject          "[ALERT] ESP32 Temperature"

// GMAIL RECEPIENT AND SETPOINTS
String inputMessage = "ddstmwow2@gmail.com";
String enableEmailChecked = "checked";
String inputMessage2 = "true";
// Default Threshold Temperature Value
String inputMessage3 = "25.0";
String inputMessage4 = "80.0";

// Flag variable to keep track if email notification was sent or not
bool emailSent1 = false;
bool emailSent2 = false;

// The Email Sending data object contains config and data to send
SMTPData smtpData;

// OTA
//#include <AsyncTCP.h>
//#include <ESPAsyncWebServer.h>
//AsyncWebServer server(8888);

#include "ESP32_MailClient.h"

// SENSOR
#include <Wire.h>
#include "ClosedCube_HDC1080.h"

ClosedCube_HDC1080 hdc1080;

// Assign IO pin accordingly
const int ledPower = 4;
const int ledNet = 18;
const int ledData = 5;
String ledState;

struct Offsets{
  float offset;
} tempSet, humSet;
String offsetT, offsetH;
float tOffset, hOffset;
float eet, eeh;                 // variables for calling/storing? eeprom values

bool connectnetwork = false;

void messageReceived(String &topic, String &payload){
  Serial.println("incoming: " + topic + " - " + payload);
  int index = payload.indexOf(":");

  if (payload.startsWith("led") == true){
    ledState = payload.substring(index + 1);
    if (ledState == "on"){
      Serial.println("Switch LED on");
      digitalWrite(LED_BUILTIN, HIGH);
    }
    else if (ledState == "off"){
      Serial.println("Switch LED off");
      digitalWrite(LED_BUILTIN, LOW);
    }
    else{
      Serial.println("Default LED off");
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
  else if (payload.startsWith("offsetT") == true){
    offsetT = payload.substring(index+1);
    tOffset = offsetT.toFloat();
    tempSet.offset = tOffset;
    EEPROM.write(0, tOffset);
    EEPROM.commit();
    Serial.println("Temperature Offset State saved in memory.");
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
  }
  else if (payload.startsWith("offsetH") == true){
    offsetH = payload.substring(index+1);
    hOffset = offsetH.toFloat();
    humSet.offset = hOffset;
    EEPROM.write(1, hOffset);
    EEPROM.commit();
    Serial.println("Humidity Offset State saved in memory.");
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
  }
  /*
  else if(payload.startsWith("forcedrecal") == true){
    recalib = payload.substring(index+1);
    forcedCalibval = recalib.toInt();
    airSensor.setForcedRecalibrationFactor(forcedCalibval);
    EEPROM.write(2, forcedCalibval);
    EEPROM.commit();

    if (airSensor.getForcedRecalibration(&forcedCalibval) == true) // Get the setting
    {
      Serial.print("Forced recalibration factor (ppm) is ");
      Serial.println(forcedCalibval);
    }
    else{
      Serial.println("getForcedRecalibration failed!");
    }
  }
  */
  else if ((payload == "getoffset" || "get offset")){
    tOffset = EEPROM.read(0);
    hOffset = EEPROM.read(1);
    Serial.print("EETOffset: "); Serial.println(tOffset);
    Serial.print("EEHOffset: "); Serial.println(hOffset);
  }
  payload = "";
}

// Initialize WiFi and MQTT for this board
Client *netClient;
CloudIoTCoreDevice *device;
CloudIoTCoreMqtt *mqtt;
MQTTClient *mqttClient;
unsigned long iat = 0;
String jwt;

void blinkLED(const int ledPin){
  digitalWrite(ledPin, HIGH);
  delay(200);
  digitalWrite(ledPin, LOW);
  delay(200);
}

void readEEPROM(){
  tOffset = EEPROM.read(0);
  hOffset = EEPROM.read(1);
  Serial.print("EETOffset: "); Serial.println(tOffset);
  Serial.print("EEHOffset: "); Serial.println(hOffset);
}

String getSensor(){
  //Payload Definitions:
  String SYS = "AQMS";
  String DEVICE = "esp32";
  int BOXNUM = 25;
  String TANK = "X";
  String SITE = "Taguig";
  float phFloat = 0;
  float ecFloat = 0;
  float wtFloat = 0;

  const int RACK = 0;
  const float LIGHTLUX = 0;
  const int LIGHTSTATUS = 0;

  float t = hdc1080.readTemperature() + tOffset;
  float h = hdc1080.readHumidity() + hOffset;
  float co2 = -1;

  String loadPay = String("{\"System\":") + ("\"") + SYS  + ("\"") +                      //string
                     String(",\"deviceID\":") + ("\"") + DEVICE + ("\"") +                //string
                     String(",\"Box\":") + ("\"") + BOXNUM + ("\"") +                     //integer
                     String(",\"tank\":") + ("\"") + TANK + ("\"") +                      //string
                     String(",\"site\":") + ("\"") + SITE + ("\"") +                      //string
                     String(",\"rack\":") + ("\"") + RACK + ("\"") +
                     String(",\"timecollected\":") + ("\"") + time(nullptr) + ("\"") +    //timestamp
                     String(",\"pH\":") + ("\"") + phFloat + ("\"") +                     //float
                     String(",\"EC\":") + ("\"") + ecFloat + ("\"") +                     //float
                     String(",\"waterTemp\":") + ("\"") + wtFloat + ("\"") +              //float
                     String(",\"airTemp\":") + ("\"") + String(t) + ("\"") +              //float
                     String(",\"airHum\":") + ("\"") + String(h) + ("\"") +               //float
                     String(",\"CO2\":") + ("\"") + String(co2) + ("\"") +                //float
                     String(",\"lightLux\":") + ("\"") + LIGHTLUX + ("\"") +              //float
                     String(",\"lightStatus\":") + ("\"") + LIGHTSTATUS + ("\"") +        //integer
                     String("}");

  blinkLED(ledData);
  Serial.println(loadPay);
  return loadPay;
}

String getJwt(){
  iat = time(nullptr);
  Serial.println("Refreshing JWT");
  jwt = device->createJWT(iat, jwt_exp_secs);
  return jwt;
}


void setupWifi(){
  unsigned long connectMillis = 0;

  Serial.println("Starting wifi");
  /*
  // Setup Wifi Manager
  String version = String("<p>Current Version - v") + String(CURRENT_VERSION) + String("</p>");
  Serial.println(version);

  WiFiManager wm;
  WiFiManagerParameter versionText(version.c_str());
  wm.addParameter(&versionText);

  if (!wm.autoConnect())
  {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }
  */

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED){
    connectnetwork = false;
    if ((millis() - connectMillis >= 1000) && (connectnetwork == false)){
      connectMillis = millis();
      Serial.print(".");
      digitalWrite(ledNet, !digitalRead(ledNet));
      delay(100);
    }
  }


  connectnetwork = true;
  configTime(0, 0, ntp_primary, ntp_secondary);
  Serial.println("Waiting on time sync...");
  while (time(nullptr) < 1510644967){
    delay(10);
  }
  Serial.print("Connected to: "); Serial.println(ssid);
  Serial.print("IP address: "); Serial.println(WiFi.localIP());
}

void startMQTT(){
  mqttClient->begin("mqtt.googleapis.com", 8883, *netClient);
  mqttClient->onMessage(messageReceived);
}

bool publishTelemetry(String data){
  return mqtt->publishTelemetry(data);
}

bool publishTelemetry(const char *data, int length){
  return mqtt->publishTelemetry(data, length);
}

bool publishTelemetry(String subfolder, String data){
  return mqtt->publishTelemetry(subfolder, data);
}

bool publishTelemetry(String subfolder, const char *data, int length){
  return mqtt->publishTelemetry(subfolder, data, length);
}

void publishState(String data){
  mqttClient->publish(device->getStateTopic(), data);
}

void mqttConnect(){
  Serial.println("Connecting...");
  while(!mqttClient->connect(device->getClientId().c_str(), "unused", getJwt().c_str(), false)){
    Serial.println(mqttClient->lastError());
    Serial.println(mqttClient->returnCode());
    delay(1000);
  }
  Serial.println("Connected.");
  mqttClient->subscribe(device->getConfigTopic());
  mqttClient->subscribe(device->getCommandsTopic());
  publishState("Connected");
}

void connect(){
  unsigned long reconnectMillis = 0;

  Serial.println("Checking WiFi...");
  while(WiFi.status() != WL_CONNECTED){
    connectnetwork = false;
    if (millis() - reconnectMillis >= 1000){
      reconnectMillis = millis();
      Serial.print(".");
      digitalWrite(ledNet, !digitalRead(ledNet));
    }
  }
  delay(1000);
  connectnetwork = true;
  mqttConnect();
}



void setupCloudIoT(){
  device = new CloudIoTCoreDevice(
      project_id, location, registry_id, device_id,
      private_key_str);

  setupWifi();
  netClient = new WiFiClientSecure();
  mqttClient = new MQTTClient(512);
  mqttClient->setOptions(180, true, 1000);                        // keepAlive, cleanSession, timeout
  mqtt = new CloudIoTCoreMqtt(mqttClient, netClient, device);
  mqtt->setUseLts(true);
  startMQTT();
}
#endif //__ESP32_MQTT_H__
