#include <ESP8266WiFi.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#define TRIG 12 //D6, GPIO12
#define ECHO 13 //D7, GPIO13 

const char* mqtt_server_address = "172.105.90.154";

float duration_us, distance_cm;
float filterArray[20];
float distance;

WiFiClient wifiClient;
PubSubClient mqtt_client(wifiClient);

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt_client.connect("ultrasonicsensor")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqtt_client.publish("Goteborg/UltraSonic/status","connected");
      // ... and resubscribe
      //mqtt_client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

float ultrasonicMeasure() {
  // generate 10-microsecond pulse to TRIG pin
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  // measure duration of pulse from ECHO pin
  float duration_us = pulseIn(ECHO, HIGH);

  // calculate the distance
  float distance_cm = 0.017 * duration_us;

  return distance_cm;
}

void filter_measurement() {
// 1. TAKING MULTIPLE MEASUREMENTS AND STORE IN AN ARRAY
  for (int sample = 0; sample < 20; sample++) {
    filterArray[sample] = ultrasonicMeasure();
    delay(30); // to avoid untrasonic interfering
  }

  // 2. SORTING THE ARRAY IN ASCENDING ORDER
  for (int i = 0; i < 19; i++) {
    for (int j = i + 1; j < 20; j++) {
      if (filterArray[i] > filterArray[j]) {
        float swap = filterArray[i];
        filterArray[i] = filterArray[j];
        filterArray[j] = swap;
      }
    }
  }

  // 3. FILTERING NOISE
  // + the five smallest samples are considered as noise -> ignore it
  // + the five biggest  samples are considered as noise -> ignore it
  // ----------------------------------------------------------------
  // => get average of the 10 middle samples (from 5th to 14th)
  double sum = 0;
  for (int sample = 5; sample < 15; sample++) {
    sum += filterArray[sample];
  }

  distance = sum / 10;

  static char cDistance[7];
  dtostrf(distance,6,2,cDistance);
  mqtt_client.publish("Goteborg/UltraSonic/distance",cDistance);
} 

void setup() {
    Serial.begin(57600);

    pinMode(TRIG, HIGH);
    delayMicroseconds(10);
    pinMode(ECHO, INPUT);

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;
    //reset saved settings
    //wifiManager.resetSettings();
    delay(200);
    //set custom ip for portal
    //wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

    //fetches ssid and pass from eeprom and tries to connect
    //if it does not connect it starts an access point with the specified name
    //here  "AutoConnectAP"
    //and goes into a blocking loop awaiting configuration
    wm.autoConnect("ussAP");
    //or use this for auto generated name ESP + ChipID
    //wifiManager.autoConnect();
    delay(200);
    
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeay :)");

    mqtt_client.setServer(mqtt_server_address, 1883);
    delay(500);
}

void loop() {
  if (!mqtt_client.connected()) {
    reconnect();
  }
  mqtt_client.loop();
  filter_measurement();
  delay(10000);  
}