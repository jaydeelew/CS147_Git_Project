#include <Arduino.h>
#include "config.h"
#include <time.h>
#include <WiFiClientSecure.h>
#include <MQTTClient.h> 
#include <ArduinoJson.h>
#include "WiFi.h"
#include <DFRobot_DHT20.h>

DFRobot_DHT20 dht20;

#define RELAY 13
#define IR 2
// MQTT topics for the device
#define AWS_IOT_PUBLISH_TOPIC   "esp32_ttgo/pub" 
#define AWS_IOT_SUBSCRIBE_TOPIC "esp32_ttgo/sub"

time_t start_time, end_time;
int elapsed_seconds;
char kPath[100];
char temp[50];
char humid[50]; 

WiFiClientSecure wifi_client = WiFiClientSecure();
MQTTClient mqtt_client = MQTTClient(256); //256 indicates the maximum size for packets being published and received.

uint32_t t1;

void incomingMessageHandler(String &topic, String &payload);

void connectAWS()
{
  //Begin WiFi in station mode
  WiFi.mode(WIFI_STA); 
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi");

  //Wait for WiFi connection
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  // Configure wifi_client with the correct certificates and keys
  wifi_client.setCACert(AWS_CERT_CA);
  wifi_client.setCertificate(AWS_CERT_CRT);
  wifi_client.setPrivateKey(AWS_CERT_PRIVATE);

  //Connect to AWS IOT Broker. 8883 is the port used for MQTT
  mqtt_client.begin(AWS_IOT_ENDPOINT, 8883, wifi_client);

  //Set action to be taken on incoming messages
  mqtt_client.onMessage(incomingMessageHandler);

  Serial.print("Connecting to AWS IOT");

  //Wait for connection to AWS IoT
  while (!mqtt_client.connect(THINGNAME)) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  if(!mqtt_client.connected()){
    Serial.println("AWS IoT Timeout!");
    return;
  }

  //Subscribe to a topic
  mqtt_client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

  Serial.println("AWS IoT Connected!");
}

void publishMessage()
{
  //Create a JSON document of size 200 bytes, and populate it
  StaticJsonDocument<200> doc;
  doc["ElapsedSeconds"] = elapsed_seconds;
  doc["Temperature"] = temp;
  doc["Humidity"] = humid;
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to mqtt_client

  //Publish to the topic
  mqtt_client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
  Serial.print("Elapsed Seconds: ");
  Serial.println(elapsed_seconds);
}

void incomingMessageHandler(String &topic, String &payload) {
  Serial.println("Message received!");
  Serial.println("Topic: " + topic);
  Serial.println("Payload: " + payload);
}

float roundTo2(float var) {
    float value = round(var * 100) / 100;
    return value;
}

void setup() {
  Serial.begin(115200);
  //Initialize dht20 sensor
  while(dht20.begin()){
    Serial.println("Initialize sensor failed");
    delay(1000);
  }
  t1 = millis();
  connectAWS();
  pinMode(IR,INPUT);
  pinMode(RELAY,OUTPUT);
}

void loop() {
   if (digitalRead(IR) == 0) {
    time(&start_time);

    while (digitalRead(IR) == 0) {
      digitalWrite(RELAY, HIGH);
      delay(20);
    }

    time(&end_time);
    elapsed_seconds = difftime(end_time, start_time);
    sprintf(temp, "%.2f",(roundTo2(dht20.getTemperature()) * 1.8 + 32)); // convert to Farenheight
    sprintf(humid, "%.2f",roundTo2(dht20.getHumidity() * 100));
    publishMessage();
   }
   else
    digitalWrite(RELAY, LOW);

  mqtt_client.loop(); // needed?
  delay(50);
}