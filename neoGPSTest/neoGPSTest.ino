/*
 * Uno pins 8 9 for RX TX
 * MKR 1010 13 14 for RX TX

    push data to an MQTT server - uses library from https://pubsubclient.knolleary.net

    Duncan Wilson
    
    Oct 2020

    Board: Arduino MKR WIFI 1010
    GPS:
    Water Temp sensorW: 
*/

#include <SPI.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>

#include <NMEAGPS.h>
#include <GPSport.h>
#include <OneWire.h>
#include <DS18B20.h>

NMEAGPS  GPS; // This parses the GPS characters
gps_fix  fix; // This holds on to the latest values
uint8_t fixCount = 0;
const static uint8_t interval = 60;
#define water 10
OneWire oneWireW(water);
DS18B20 sensorW(&oneWireW);
#define air 9
OneWire oneWireA(air);
DS18B20 sensorA(&oneWireA);

// Wifi
#include "arduino_secrets.h" 
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char password[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;
WiFiClient wificlient;

// MQTT
const char* mqtt_server = "bats.cetools.org";
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;


void setup()
{
  DEBUG_PORT.begin(9600);

  DEBUG_PORT.print( F("-----------------Starting\n") );

  startWifi();
  // start MQTT server
  client.setServer(mqtt_server, 1883);
  //client.setCallback(callback);
    
  gpsPort.begin(9600);
  sensorW.begin();
  sensorA.begin();

}

//--------------------------

void loop()
{
  //readWaterTemp();
  
  while (GPS.available( gpsPort )) {
    fix = GPS.read();

    // Once every "interval" seconds...    
    if (++fixCount >= interval) {
      gpsDisplay();
      fixCount = 0;

      readWaterTemp();

      readAirTemp();

      sendDataWeb();
      
    }   
  }
}

void startWifi() {

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }
  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }
    
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // check to see if connected and wait until you are
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

}


float readWaterTemp(){
  sensorW.requestTemperatures();
  while (!sensorW.isConversionComplete());  // wait until sensorW is ready 
  DEBUG_PORT.print( F("Water Temp: ") );
  DEBUG_PORT.println(sensorW.getTempC());
  return sensorW.getTempC();
}

float readAirTemp(){
  sensorA.requestTemperatures();
  while (!sensorA.isConversionComplete());  // wait until sensorA is ready 
  DEBUG_PORT.print( F("Air Temp: ") );
  DEBUG_PORT.println(sensorA.getTempC());
  return sensorA.getTempC();
}

void sendDataWeb(){
  DEBUG_PORT.print( F("sendDataWeb: ") );

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  DEBUG_PORT.println("");
  DEBUG_PORT.println("MQTT data:");
  
  
  if (fix.valid.location) {
    snprintf (msg, 50, "%.4f", fix.latitude());
    DEBUG_PORT.print("lat: ");
    DEBUG_PORT.println(msg);
    client.publish("zoar/latitude/", msg);
  
    snprintf (msg, 50, "%.4f", fix.longitude());
    DEBUG_PORT.print("lon: ");
    DEBUG_PORT.println(msg);
    client.publish("zoar/longitude/", msg);
  }

  if (fix.valid.altitude){
    snprintf (msg, 50, "%.0f", fix.altitude());
    DEBUG_PORT.print("alt: ");
    DEBUG_PORT.println(msg);
    client.publish("zoar/altitude/", msg);
  }

  char mydatetime[50];
  getISOdatetime().toCharArray(mydatetime, 50);
  snprintf (msg, 50, "%s", mydatetime);
  DEBUG_PORT.print("iso: ");
  DEBUG_PORT.println(msg);
  client.publish("zoar/isodate/", msg);
  
  snprintf (msg, 50, "%.2f", readWaterTemp());
  DEBUG_PORT.print("water: ");
  DEBUG_PORT.println(msg);
  client.publish("zoar/temperature/water/", msg);

  snprintf (msg, 50, "%.2f", readAirTemp());
  DEBUG_PORT.print("air: ");
  DEBUG_PORT.println(msg);
  client.publish("zoar/temperature/air/", msg);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ZOARClient-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("zoar/hi", "hello world");
      // ... and resubscribe
      //client.subscribe("plant/minty/inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}



void gpsDisplay(){
  DEBUG_PORT.print( F("Location: ") );
  if (fix.valid.location) {
    DEBUG_PORT.print( fix.latitude(), 6 );
    DEBUG_PORT.print( ',' );
    DEBUG_PORT.print( fix.longitude(), 6 );
  }
  
  if (fix.valid.altitude){
    DEBUG_PORT.print( F(", Altitude: ") );
    DEBUG_PORT.print( fix.altitude() );    
  }
  
  DEBUG_PORT.print( F(", ISO: ") );
  DEBUG_PORT.print( getISOdatetime() );

  DEBUG_PORT.println();
}

String getISOdatetime(){
  // Shift the date/time to local time
  NeoGPS::clock_t localSeconds;
  NeoGPS::time_t  localTime;
  if (fix.valid.date && fix.valid.time) {
    using namespace NeoGPS; // save a little typing below...

    localSeconds = (clock_t) fix.dateTime; // convert structure to a second count
    localTime = localSeconds;              // convert back to a structure
  }
   
  if (fix.valid.date && fix.valid.time){
    return getGPSdateISOformat(localTime);
  }
  else{
    return "0";
  }
  
}

String getGPSdateISOformat(NeoGPS::time_t localTime){
  //2020-10-23T15:40:31+00:00 ISO 8601
  String mydate = "";
  mydate += localTime.year;  
  mydate += ("-");
  mydate += localTime.month;
  mydate += ("-");
  mydate += localTime.date;
  mydate += ("T");

  mydate += localTime.hours;
  mydate += (":");
  if (localTime.minutes < 10) mydate += ("0");
  mydate += localTime.minutes;
  mydate += (":");
  if (localTime.seconds < 10) mydate += ("0");
  mydate += localTime.seconds;

  return mydate;
}
