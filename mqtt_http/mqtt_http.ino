/** Sensing and publishing
 *  Xavier Serpaggi <serpaggi@emse.fr>
 *
 *  Two sensors plugged to an Arduino MKR 1010
 *  - Light : TEMT6000 on Analog pin A0
 *  - Temperature/Humidity : DHT22 or DS1820, data pin on GPIO pin #4
 *  Values are sent to an MQTT broker.
 *  
 * Required libraries (sensing):
 *  - DallasTemperature
 *  - OneWire (Included in the latest versions of DallasTemperature)
 *  - Adafruit Unified Sensor
 *  - DHT Sensor Library (by Adafruit)
 *
 * Required libraries (communication & MQTT):
 *  - WiFiNINA
 *  - MQTT (by Joel Gaehwiler)
 */
 
#include <string.h>

#include <Wire.h>
#include <SPI.h>

#include <WiFiNINA.h>
#include <MQTT.h>

#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>


/* In which pins are my sensors plugged? */
#define LUM A0  // TEMT6000 Signal pin
#define TMP 4   // DHT22 Signal pin

/* We need a DHT object to address the sensor. */
DHT dht(TMP, DHT22) ; // pin: TMP, model: DHT22



/* We need objects to handle:
 *  1. WiFi connectivity
 *  2. MQTT messages
 */
WiFiClient wifi_client ;
MQTTClient mqtt_client ;



/* And associated variables to tell:
 *  1. which WiFi network to connect to
 *  2. what are the MQTT broket IP address and TCP port
 */
const char* wifi_ssid     = "lab-iot-1";
const char* wifi_password = "lab-iot-1";
int status = WL_IDLE_STATUS;     // the WiFi radio's status

IPAddress dns(8, 8, 8, 8);


const char* mqtt_host = "m21.cloudmqtt.com" ;
const char* mqtt_user     = "philips-hue-973";
const char* mqtt_password = "philips-hue-973";
const uint16_t mqtt_port =  11029 ;

unsigned long lastConnectionTime = 0;

/* Some variables to handle measurements. */
int tmp ;
int lum ;
int hmdt ;
boolean first_time ;

boolean enabled = false;

int BUTTON_ALIM = 7;
int BUTTON_READ = 6;

uint32_t t0, t ;


/* 'topic' is the string representing the topic on which messages
 *  will be published.
 *  
 * TODO: You need to adapt it by setting:
 *  1. 'surname' to your surname
 *  2. 'ID' to a unique ID chosen in the class with other students
 */
// String surname = "doe" ; // <--- CHANGE THIS!
String ID = "1" ; // <--- CHANGE THIS!
String topic = "light";

String user_id = "rxUynGdZPYzS5JdzOywJjWrrseVCoYx6DlQvQ5Ca" ;
char server[] = "192.168.1.131";

/* Time between two sensings and values sent. */
#define DELTA_T 5000


/* ################################################################### */
void setup() {
  Serial.begin(9600) ;  // monitoring via Serial is always nice when possible
  delay(100) ;
  Serial.println("Initializing...\n") ;

  pinMode(LED_BUILTIN, OUTPUT) ;
  digitalWrite(LED_BUILTIN, LOW) ;  // switch LED off

  pinMode(BUTTON_ALIM, OUTPUT);
  pinMode(BUTTON_READ, INPUT);
  digitalWrite(BUTTON_ALIM, HIGH);


  status = WiFi.begin(wifi_ssid, wifi_password) ;
  mqtt_client.begin(mqtt_host, mqtt_port, wifi_client) ;
  mqtt_client.onMessage(callback) ;

  // printWiFiData();

  first_time = true ;
  
  // Time begins now!
  t0 = t = millis() ;
}


/* ################################################################### */
void loop() {
  /* We try to connect to the broker and launch the client loop.
   */
  mqtt_client.loop() ;
  if ( ! mqtt_client.connected() ) {
    reconnect() ;
  }

  /* Values are read from sensors at fixed intervals of time.
  */
  // ===================================================
  t = millis() ;
  if ( first_time || (t - t0) >= DELTA_T ) {
    t0 = t ;
    first_time = false ;

    lum = getLum() ;
    tmp = getTmp() ;
    hmdt = getHmdt() ;

    Serial.println("Get values");
    sendValues() ;
  }

  if (digitalRead(BUTTON_READ) == HIGH)
    {
      Serial.println("Switch light");
      
      mqtt_client.publish(String(topic + "/SWITCH").c_str(), String(enabled ? "on" : "off").c_str()) ;

      //delay(50);

      //request("PUT", "{\"on\": " + String(enabled) + "}");
    
      enabled = !enabled;
      
      delay(300);
    }
}

/* ------------------------------------------------------------------- */
double getLum()
{
  double acc = 0 ;
  uint8_t n_val ;

  for ( n_val = 0 ; n_val < 10 ; n_val++ ) {
    acc += analogRead(LUM) ;
    delay(5) ;
  }

  return acc / n_val ;
}


/* ------------------------------------------------------------------- */
double getTmp()
{
  return dht.readTemperature() ;
}


/* ------------------------------------------------------------------- */
double getHmdt()
{
  return dht.readHumidity() ;
}


void printWiFiData() {
  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP address : ");
  Serial.println(ip);

  Serial.print("Subnet mask: ");
  Serial.println((IPAddress)WiFi.subnetMask());

  Serial.print("Gateway IP : ");
  Serial.println((IPAddress)WiFi.gatewayIP());

  // print your MAC address:
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);
}

void printCurrentNet() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print the MAC address of the router you're attached to:
  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print("BSSID: ");
  printMacAddress(bssid);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI): ");
  Serial.println(rssi);

  // print the encryption type:
  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type: ");
  Serial.println(encryption, HEX);
  Serial.println();
}

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}

/* ################################################################### */
/* This function handles the connection/reconnection to
 * the MQTT broker.
 * Each time a connection or reconnection is made, it
 * publishes a message on the topic+"/HELLO" containing 
 * the number of milli seconds since uController start.
 */
void reconnect() {
 
  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(wifi_ssid);
    // Connect to WPA/WPA2 network:
    status = WiFi.begin(wifi_ssid, wifi_password);

    // wait 5 seconds for connection:
    delay(2000);
  }
  
  Serial.print("\n");
  Serial.print("WiFi connected\n");
  Serial.print("IP address: \n");
  Serial.print(WiFi.localIP());
  Serial.print("\n") ;

  printCurrentNet();
  printWiFiData();


  Serial.print("MQTT: trying to connect to host ") ;
  Serial.print(mqtt_host) ;
  Serial.print(" on port ") ;
  Serial.print(mqtt_port) ;
  Serial.println(" ...") ;

  while ( !mqtt_client.connect(ID.c_str(), mqtt_user, mqtt_password) ) {
    digitalWrite(LED_BUILTIN, HIGH) ;
    delay(100);
    digitalWrite(LED_BUILTIN, LOW) ;
    Serial.print(".");
    delay(100);
  }
  Serial.println("MQTT: connected") ;
  Serial.print("MQTT: root topic is \"") ;
  Serial.print(topic) ;
  Serial.println("\"") ;
    
  mqtt_client.publish(String(topic).c_str(), "MQTT connection init after " + String(millis()) + "ms.") ;

  /* If you want to subscribe to topics, you have to do it here. */
  mqtt_client.subscribe(String(topic + "/SWITCH").c_str());
  mqtt_client.subscribe(String(topic + "/COLOR").c_str());
}

/* ################################################################### */
/* This function handles the sending of all the values
 * collected by the sensors.
 * Values are sent on a regular basis (every DELTA_T_SEND_VALUES 
 * milliseconds).
 */

void sendValues() {
  if ( mqtt_client.connected() ) {
    if ( lum != -1 )
      mqtt_client.publish(String(topic + "/LUMI").c_str(), String(lum).c_str()) ;
    if ( tmp != -1 )
      mqtt_client.publish(String(topic + "/TEMP").c_str(), String(tmp).c_str()) ;
    if ( hmdt != -1 )
      mqtt_client.publish(String(topic + "/HMDT").c_str(), String(hmdt).c_str()) ;
  }
}
 

void request(String method, String postData) {
  // close any connection before send a new request.
  // This will free the socket on the Nina module

  // if there's a successful connection:
  if (wifi_client.connect(server, 80)) {
    Serial.println("connecting...");
    // send the HTTP PUT request:
    wifi_client.println(method + " /api/" + user_id + "/lights/9/state HTTP/1.1");
    wifi_client.println("Host: 192.168.1.131");
    wifi_client.println("User-Agent: ArduinoWiFi/1.1");
    wifi_client.println("Connection: close");
    wifi_client.println("Content-Type: application/json;");
    wifi_client.print("Content-Length: ");
    wifi_client.println(postData.length());
    wifi_client.println();
    wifi_client.println(postData);

    Serial.println("Respond:");
    while(wifi_client.available()){
      String line = wifi_client.readStringUntil('\r');
      Serial.print(line);
    }

    // note the time that the connection was made:
    lastConnectionTime = millis();
  } else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
  }
}


/* ################################################################### */
/* The main callback to listen to topics.
 */
void callback(String &intopic, String &payload)
{
  /* There's nothing to do here ... as long as the module
   *  cannot handle messages.
   */
  Serial.println("incoming: " + intopic + " - " + payload);
  if (intopic == String(topic + "/SWITCH").c_str()) {
    Serial.println(payload) ;
    if(payload == "on") {
      //digitalWrite(LED_BUILTIN, HIGH) ;
      request("PUT", "{\"on\": true}") ;
    }
    else if (payload == "off") {
      //digitalWrite(LED_BUILTIN, LOW) ;
      request("PUT", "{\"on\": false}") ;
    }
    else {
      Serial.println("Wrong command");
    }
  };
  if (intopic == String(topic + "/COLOR").c_str()) { 
    Serial.println(payload) ;
    request("PUT", payload) ;
  }
}
