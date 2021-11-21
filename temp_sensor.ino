#include <WiFiClient.h>
#include <WiFi.h>

#include <ArduinoJson.h>
//https://arduinojson.org/
//6.18.4
#include <PubSubClient.h>
//https://pubsubclient.knolleary.net/
//V 2.8.0
#include <NTPClient.h>
// https://github.com/arduino-libraries/NTPClient
// v 3.2.0
#include "uptime.h"
//https://github.com/YiannisBourkelis/Uptime-Library
// V 1.0.0

#include "version.h"
#include "config.h"


//Stuff TODO:
//TODO: Add checking to each sensor_start function that reports a failed sensor start.
//TODO: In the case of a failed sensor, their should be a report channel in mqtt.
//TODO: Ideally also a way to remotely stop and restart the sensor to try again.


struct data_frame {
  #ifdef BME_enable
    float bme_temp;
    float bme_pres;
    float bme_humd;
  #endif
  #ifdef BMP_enable
    float bmp_temp;
    float bmp_pres;
  #endif
  #ifdef AHTx_enable
    float AHTt;
    float AHTh;
  #endif
  #ifdef DS18_enable
    float DS18_temp;
  #endif
};

#ifdef AHTx_enable
  #include <Adafruit_AHTX0.h>
  Adafruit_AHTX0 aht;
  bool aht_found;
  bool aht_started;
  float temp_storage = 0;
  float pressure_storage = 0;

  int start_aht(){
    aht_found = aht.begin();
    if (!aht_found) {
      Serial.println("Could not find a valid AHTX0 sensor, check wiring!");
      return 0;
    }else{
      Serial.println("AHTX0 started.");
      return 1;
    }
  }

  void read_AHT(struct data_frame &dataframe){
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);
    dataframe.AHTt = temp.temperature;
    dataframe.AHTh = humidity.relative_humidity;
  }


#endif

#ifdef BME_enable
  #define SEALEVELPRESSURE_HPA (1013.25)
  // #include <Adafruit_BME280.h>
  #include <BME280I2C.h>
  //https://github.com/finitespace/BME280#methods
  Adafruit_BME280 bme;
  bool bme_found;
  bool bme_started;

  void read_BME(struct data_frame &data_frame){
    data_frame.bme_temp = bme.readHumidity();
    data_frame.bme_humd = bme.readTemperature();
    data_frame.bme_pres = bme.readPressure();
  }

  int start_bme(){
    Serial.println("Attempting to start BME");
    bme_found = bme.begin(0x77);
    if (bme_found == 1) {
        Serial.println("I2C BME Started on address 0x77");
        return 1;
    }else{
      Serial.println("Starting BME Failed on 0x77, trying 0x76");
      bme_found = bme.begin(0x76);
      if (bme_found == 1) {
        Serial.println("I2C BME Started on address 0x76");
        return 1;
      }else{
        Serial.println("Failed to find BME at common I2C address");
        return 0;
      }
    }
  }

#endif

#ifdef BMP_enable
  #include <Adafruit_BMP280.h>
  #define SEALEVELPRESSURE_HPA (1013.25)
  Adafruit_BMP280 bmp;
  bool bmp_found;
  bool bmp_started;

  void read_BMP(struct data_frame &data_frame){
    data_frame.bmp_temp = bmp.readTemperature();
    data_frame.bmp_humd = bmp.readPressure();
   }

  int start_bmp(){
    bmp_found = bmp.begin(0x76,0x58);
    if (!bmp_found) {
        Serial.println("Could not find a valid BMP280 sensor, check wiring!");
        return 0;
      }else{
        Serial.println("BMP280 started.");
        return 1;
      }
  }

#endif

#ifdef DS18_enable
  // 01:59:04.593 -> Model: DS18B20
  // 01:59:04.593 -> Address: 40 113 211 117 208 1 60 72
  // 01:59:04.626 -> Resolution: 12
  // 01:59:04.626 -> Power Mode: External
  // 01:59:04.626 -> Temperature: 18.25 C / 64.96 F
  /// Family D2 (Clone w/o parasitic power).
  // https://github.com/cpetrich/counterfeit_DS18B20
  #include <Wire.h>
  #include <OneWire.h>
  #include <DallasTemperature.h>
  OneWire oneWire(23);
  DallasTemperature ds18b20(&oneWire);
  float read_DS18(){
    ds18b20.requestTemperatures();
    float DS18 = ds18b20.getTempCByIndex(0);
    return DS18;
  }
#endif



void send_data(struct data_frame d_frame){
  timeClient.update();
  int epochDate = timeClient.getEpochTime();
  DynamicJsonDocument doc(1024);
  doc["t"] = epochDate;

  #ifdef AHTx_enable
    char AHTt[15];
    char AHTh[15];
    sprintf(AHTt, "%.1f", d_frame.AHTt);
    sprintf(AHTh, "%.1f", d_frame.AHTh);
    doc["aht_t"] = AHTt;
    doc["aht_h"] = AHTh;
  #endif
  #ifdef BME_enable
    char bmeT[15];
    char bmeH[15];
    char bmeP[15];
    sprintf(bmeT, "%.1f", d_frame.bme_temp);
    sprintf(bmeH, "%.1f", d_frame.bme_pres);
    sprintf(bmeP, "%.1f", d_frame.bme_humd);
    doc["bme_temp"] = bmeT;
    doc["bme_pres"] = bmeH;
    doc["bme_humd"] = bmeP;
  #endif
  #ifdef BMP_enable
    char bmpT[15];
    char bmpP[15];
    sprintf(bmpT, "%.1f", d_frame.bmp_temp);
    sprintf(bmpP, "%.1f", d_frame.bmp_pres);
    doc["bmp_temp"] = bmpT;
    doc["bmp_pres"] = bmpP;
  #endif
  #ifdef DS18_enable
    char DS18t[15];
    sprintf(DS18t, "%.1f", d_frame.DS18_temp);
    doc["DS18_temp"] = DS18t;
  #endif

  char buffer[1024];
  size_t n = serializeJson(doc, buffer);
  mqtt_client.publish(mqtt_json_data_topic, buffer, n);
  mqtt_client.loop();
}

void setup() {
  Serial.begin(115200);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected!");
  timeClient.begin();
  Serial.println("Time Client Started");
  Serial.println("WiFi connected:");
  IPAddress ip = WiFi.localIP();
  Serial.print(ip);
  Serial.println("/");

  // Generate some info once for info topic
  sprintf(ip_char, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

  #ifdef BME_enable
    bme_started = start_bme();
  #endif

  #ifdef BMP_enable
    bmp_started = start_bmp();
  #endif

  #ifdef AHTx_enable
    aht_started = start_aht();
  #endif

  #ifdef DS18_enable
    Wire.begin();
    ds18b20.begin();
  #endif

  mqtt_client.setServer(mqttServerIp, mqttServerPort);
  mqtt_client.setCallback(callback);
 	delay(500);

  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER){
    woke_from_sleep = 1;
  }


}

void callback(char *topic, byte *payload, unsigned int length) {
  //Should figure out max payload size and do memory allocation correctly.
  StaticJsonDocument<1024> r_doc;
  deserializeJson(r_doc, payload, length);
  timeClient.update();
  int mqttRxTime = timeClient.getEpochTime();
  // start a mqtt message buffer
  char mqtt_log_message[256];
  sprintf(mqtt_log_message, "RX at %d ", mqttRxTime);
  if (r_doc["set_polling"]) {
    if (r_doc["set_polling"].as<int>() != 0){
        polling_rate = r_doc["set_polling"].as<int>()*1000;
        size_t offset = strlen(mqtt_log_message);
        sprintf(&(mqtt_log_message[offset]), " Setting the polling rate at %d seconds" , polling_rate/1000); //38+10
      }
    }
  if (r_doc["set_info"]) {
    if (r_doc["set_info"].as<int>() != 0){
      send_info_rate = r_doc["set_info"].as<int>()*1000;
      size_t offset = strlen(mqtt_log_message);
      sprintf(&(mqtt_log_message[offset]), " Setting the info send rate at %d seconds" , send_info_rate/1000);
    }
  }
  mqttLog(mqtt_log_message);
}
void mqttLog(const char* str) {
  if (mqtt_client.connected()){
    DynamicJsonDocument doc(256);
    timeClient.update();
    int mqtt_log_time = timeClient.getEpochTime();
    doc["time"] = mqtt_log_time;
    doc["msg"] = str;
    char buffer[256];
    size_t n = serializeJson(doc, buffer);
    mqtt_client.publish(mqtt_log_topic, buffer, n);
  }else{
    // print to serial
    // also figure out a way to store.
  }
}
void mqttConnect() {
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt_client.connect(mqttClientName, mqttUsername, mqttPassword, willTopic, willQoS, willRetain, willMessage)) {
      Serial.println("Connection Complete");
      Serial.println(willTopic);
      delay(1000);
      mqtt_client.publish(willTopic, "online", true);
      // ...
      // Subcribe here.
      mqtt_client.subscribe(mqtt_command_topic);
      } else {
      Serial.print("failed, rc = ");
      Serial.print(mqtt_client.state());
      Serial.println(" Trying again in 5 seconds");
      delay(5000);
    }
  }
}
void send_info(){
  timeClient.update();
  int epochDate = timeClient.getEpochTime();
  DynamicJsonDocument doc(512);
  doc["t"] = epochDate; //1+10
  doc["Wifi IP"] = ip_char; //7+16
  doc["Code Version"] = _VERSION; //12+21
  doc["polling_rate"] = polling_rate/1000; //12+8 1000 days is 8 digits in seconds
  doc["info_rate"] = send_info_rate/1000; //9+8
  // doc["info_rate_seconds"] = polling_rate*send_info_rate; //17+16 max # digits of product of two 8 digit
  uptime::calculateUptime();
  char readable_time[12];
  sprintf(readable_time, "%02d:%02d:%02d:%02d", uptime::getDays(),uptime::getHours(),uptime::getMinutes(),uptime::getSeconds());
  doc["Uptime"] = readable_time; //6+12
  //155
  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  mqtt_client.publish(mqtt_info_topic, buffer, n);
  mqtt_client.loop();
}

void loop() {

  if (!mqtt_client.connected()) {
   mqttConnect();
  }
  mqtt_client.loop();
  currentMillis = millis();

  if (currentMillis - last_info_Millis >= polling_rate ) {
    last_info_Millis = currentMillis;
    struct data_frame dframe;

    #ifdef DS18_enable
      dframe.DS18_temp = read_DS18();
    #endif

    #ifdef AHTx_enable
      if (aht_started){
        read_AHT(dframe);
      }else{
      }
    #endif

    #ifdef BME_enable
      if (bme_started){
        read_BME(dframe);
      }else{
      }
    #endif

    #ifdef BMP_enable
      if (bmp_started){
        read_BMP(dframe);
      }else{
      }
    #endif

    send_data(dframe);

  }

  if (currentMillis - last_poll_Millis >= send_info_rate){
    last_poll_Millis = currentMillis;
    send_info();
  }

  if (woke_from_sleep == 1){
      woke_from_sleep = 0;
      timeClient.update();
      DynamicJsonDocument doc(64);
      doc["tx_time"] = timeClient.getEpochTime();
      doc["msg"] = "Woke up from sleep";
      char buffer[64];
      size_t n = serializeJson(doc, buffer);
      mqtt_client.publish(mqtt_log_topic, buffer, n);
      mqtt_client.loop();
  }
}
