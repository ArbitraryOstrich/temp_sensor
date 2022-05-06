#include <WiFiUdp.h>
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

#include <WiFiClient.h>

#include "version.h"
#include "config.h"


//Remember these are in config.h so they must be below.
#ifdef disable_brownouts
  #include "soc/soc.h"
  #include "soc/rtc_cntl_reg.h"
#endif



#ifdef esp8266_enable
  #include <ESP8266WiFi.h>
#endif
#ifdef esp32_enable
  #include <WiFi.h> 
  #include <Arduino.h>
#endif


//Stuff TODO:
//TODO: Add checking to each sensor_start function that reports a failed sensor start.
//TODO: In the case of a failed sensor, their should be a report channel in mqtt.
//TODO: Ideally also a way to remotely stop and restart the sensor to try again.
//TODO: Polling rate limits based on sensor
//TODO: Fix polling rate thing for multiple sensors?
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
  #ifdef sgp40_enable
    float SGP40_voc;
  #endif
};


#ifdef sgp40_enable
  #include <sensirion_arch_config.h>
  #include <DFRobot_SGP40.h>
  #include <sensirion_voc_algorithm.h>
  //https://github.com/DFRobot/DFRobot_SGP40
  //V1.0.2
  bool SGP40_found; 
  bool SGP40_started;

  DFRobot_SGP40    mySgp40;
  bool start_sgp40(){
    DBUG.println("Starting SGP40:");
    DBUG.println("- init time takes 10 seconds");
    DBUG.println("- quickest reading is 1 second");
    DBUG.println("- reading is 24 hour relative");
    // DBUG.println("- loops init if fail");
    /* 
     * VOC index can directly indicate the condition of air quality. The larger the value, the worse the air quality
     *    0-100，no need to ventilate,purify
     *    100-200，no need to ventilate,purify
     *    200-400，ventilate,purify
     *    400-500，ventilate,purify intensely
     * Return VOC index, range: 0-500
     */
    // while(mySgp40.begin(/*duration = */10000) !=true){
    //   DBUG.println("failed to init chip, please check if the chip connection is fine");
    //   delay(1000);
    // }
    if(mySgp40.begin(/*duration = */10000) !=true){
      mqttLog("Failed to init SGP40");
      return 0;
    }else{
      mqttLog("SGP40 started");
      return 1;
    }
  }

  void read_sgp40(struct data_frame &dataframe){
      dataframe.SGP40_voc = mySgp40.getVoclndex();
      DBUG.print("vocIndex = ");
      DBUG.println(dataframe.SGP40_voc);
  }
#endif

#ifdef AHTx_enable
  #include <Adafruit_AHTX0.h>
  //https://github.com/adafruit/Adafruit_AHTX0
  // V 2.0.1
  Adafruit_AHTX0 aht;
  bool aht_found;
  bool aht_started;

  bool start_aht(){
    aht_found = aht.begin();
    if (!aht_found) {
      mqttLog("Could not find a valid AHTX0 sensor, check wiring!");
      return 0;
    }else{
      mqttLog("AHTX0 started.");
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
    DBUG.println("Attempting to start BME");
    bme_found = bme.begin(0x77);
    if (bme_found == 1) {
        DBUG.println("I2C BME Started on address 0x77");
        return 1;
    }else{
      DBUG.println("Starting BME Failed on 0x77, trying 0x76");
      bme_found = bme.begin(0x76);
      if (bme_found == 1) {
        DBUG.println("I2C BME Started on address 0x76");
        return 1;
      }else{
        DBUG.println("Failed to find BME at common I2C address");
        return 0;
      }
    }
  }
#endif

#ifdef BMP_enable
  #include <Adafruit_BMP280.h>
  // https://github.com/adafruit/Adafruit_BMP280_Library
  // V 2.4.2
  #define SEALEVELPRESSURE_HPA (1013.25)
  Adafruit_BMP280 bmp;
  bool bmp_found;
  bool bmp_started;

  void read_BMP(struct data_frame &data_frame){
    data_frame.bmp_temp = bmp.readTemperature();
    data_frame.bmp_pres = bmp.readPressure();
   }

  int start_bmp(){
    bmp_found = bmp.begin(0x76,0x58);
    if (!bmp_found) {
        DBUG.println("Could not find a valid BMP280 sensor, check wiring!");
        return 0;
      }else{
        DBUG.println("BMP280 started.");
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
  //https://www.pjrc.com/teensy/td_libs_OneWire.html
  #include <DallasTemperature.h>
  //https://github.com/milesburton/Arduino-Temperature-Control-Library
  bool ds18b20_started;
  int start_DS18(){
    Wire.begin();
    ds18b20.begin();
    OneWire oneWire(23);
    DallasTemperature ds18b20(&oneWire);
    return ds18b20,isConnected()
  }

  float read_DS18(){
    ds18b20.requestTemperatures();
    float DS18 = ds18b20.getTempCByIndex(0);
    return DS18;
  }
#endif



bool start_sensor(){
  // Doing it this way means no multiple sensors.
  // Fix this by using our already made start_x functions.
  #ifdef sgp40_enable
    SGP40_started = start_sgp40();
    // return SGP40_started;
  #endif
  #ifdef BME_enable
    bme_started = start_bme();
    // return bme_started;
  #endif

  #ifdef BMP_enable
    bmp_started = start_bmp();
    // return bmp_started;
  #endif

  #ifdef AHTx_enable
    aht_started = start_aht();
    // return aht_started;
  #endif

  #ifdef DS18_enable
    ds18b20_started = start_DS18()
    // return ds18b20_started;
  #endif

}



void send_data(struct data_frame d_frame){
  timeClient.update();
  int epochDate = timeClient.getEpochTime();
  DynamicJsonDocument doc(1024);
  doc["t"] = epochDate;
  #ifdef sgp40_enable
    char sgp40VOC[15];
    sprintf(sgp40VOC, "%.1f", d_frame.SGP40_voc);    
    doc["SGP40_voc"] = sgp40VOC;
  #endif
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
  #ifdef DS18_enable
    char DS18t[15];
    sprintf(DS18t, "%.1f", d_frame.DS18_temp);
    doc["DS18_temp"] = DS18t;
  #endif


  char buffer[1024];
  size_t n = serializeJson(doc, buffer);
  mqtt_client.publish(json_data_topic, buffer, n);
  mqtt_client.loop();
}

void setup() {
  DBUG.begin(115200);

  #ifdef disable_brownouts
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable detector
  #endif

  DBUG.print("Connecting to ");
  DBUG.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DBUG.print(".");
  }
  WiFi.setHostname(wifiHostname); //define hostname

  DBUG.println("Connected!");
  timeClient.begin();
  DBUG.println("Time Client Started");
  DBUG.println("WiFi connected:");
  IPAddress ip = WiFi.localIP();
  DBUG.print(ip);
  DBUG.println("/");

  // Generate some info once for info topic
  sprintf(ip_char, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  mqtt_client.setServer(mqttServerIp, mqttServerPort);
  mqtt_client.setCallback(callback);
  start_sensor();
 	delay(500);


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


  DynamicJsonDocument doc(1024);
  doc["rx_t"] = timeClient.getEpochTime();
  doc["msg"] = "Received Command but didn't understand";


  if (r_doc["set_polling"]) {
    if (r_doc["set_polling"].as<int>() != 0){
        doc.remove("msg");
        polling_rate = r_doc["set_polling"].as<int>()*1000;
        doc["set_polling"] = polling_rate/1000;
      }
    }
  if (r_doc["set_info"]) {
    if (r_doc["set_info"].as<int>() != 0){
      doc.remove("msg");
      send_info_rate = r_doc["set_info"].as<int>()*1000;
      doc["set_info"] = send_info_rate/1000;
    }
  }
  if (r_doc["start_sensor"]) {
    if (r_doc["start_sensor"].as<int>() == 1){
      doc.remove("msg");
      start_sensor();
      #ifdef BME_enable
        doc["BME"] = bme_started;
      #endif

      #ifdef BMP_enable
        doc["BMP"] = bmp_started;
      #endif

      #ifdef AHTx_enable
        doc["AHT"] =  aht_started;
      #endif

      #ifdef DS18_enable
        doc["DS18"] = ds18b20_started;
      #endif
    }
  }
  // mqttLog(mqtt_log_message);
  char buffer[1024];
  size_t n = serializeJson(doc, buffer);
  mqtt_client.publish(mqtt_log_topic, buffer, n);
  mqtt_client.loop();
}
void mqttLog(const char* str) {
  // This needs some sort of buffer, would be nice if it built up multi line messages and chuncked the message to the server so that we don't erase our own logs on serv when many things happen at once. 
  DBUG.println(str);
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
    DBUG.print("Attempting MQTT connection...");
    if (mqtt_client.connect(mqttClientName, mqttUsername, mqttPassword, willTopic, willQoS, willRetain, willMessage)) {
      DBUG.println("Connection Complete");
      DBUG.println(willTopic);
      delay(1000);
      mqtt_client.publish(willTopic, "online", true);
      // ...
      // Subcribe here.
      mqtt_client.subscribe(json_command_topic);
      } else {
      DBUG.print("failed, rc = ");
      DBUG.print(mqtt_client.state());
      DBUG.println(" Trying again in 5 seconds");
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
  doc["Hostname"] = wifiHostname;
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

  if (currentMillis - last_poll_Millis >= polling_rate ) {
    last_poll_Millis = currentMillis;
    struct data_frame dframe;

    #ifdef sgp40_enable
      if (SGP40_started){
        read_sgp40(dframe);
      }
    #endif

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

  if (currentMillis - last_info_Millis >= send_info_rate){
    last_info_Millis = currentMillis;
    send_info();
  }

}
