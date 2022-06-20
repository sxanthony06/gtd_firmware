#include <TimeLib.h>
#include <SD.h>
#include <TinyGPS++.h>
#include <PubSubClient.h>
#include <IPAddress.h>
#include <sim5320client.h>
#include <hayesengine.h>
#include <string.h>

#define CLIENT_INTERNAL_BUFFER_SIZE 512
#define DEFAULT_CHAR_INTERVAL 100
//#define SERIAL_RX_BUFFER_SIZE 128

#define INFLUXDB_LP_BUFFER_SIZE 512

#define LP_LOCATION_FLOAT_PRECISION 6
#define LP_LOCATION_FLOAT_MIN_WIDTH 9
#define LP_LOCATION_FLOAT_BUF_SIZE 12

#define LP_HDOP_FLOAT_PRECISION 2
#define LP_HDOP_FLOAT_MIN_WIDTH 3
#define LP_HDOP_FLOAT_BUF_SIZE 6

#define LP_SPEED_FLOAT_PRECISION 2
#define LP_SPEED_FLOAT_MIN_WIDTH 3
#define LP_SPEED_FLOAT_BUF_SIZE 6

#define LP_COURSE_FLOAT_PRECISION 1
#define LP_COURSE_FLOAT_MIN_WIDTH 3
#define LP_COURSE_FLOAT_BUF_SIZE 6

#define LP_ALTITUDE_FLOAT_PRECISION 0
#define LP_ALTITUDE_FLOAT_MIN_WIDTH 2
#define LP_ALTITUDE_FLOAT_BUF_SIZE 6

//#define DUMP_AT_COMMANDS
#define DEBUG_SERIAL_COM Serial
#define GSM_SERIAL_COM Serial1
#define GPS_SERIAL_COM Serial2

#define MQTT_KEEPALIVE_TIME 300
#define MQTT_SOCKETTIMEOUT 60

#define MQTT_CLIENT_RECONNECT_INTERVAL 3000

#define PREFFERED_BAUDRATE 9600

#define mqtt_broker_address "190.112.244.217"

#define TRACKER_ID "000001"
#define VIN "JH4KA3230J0018805"
#define FW_TAG "1.0.0"
#define HW_CONF_TAG "0.0.1"

void smartdelay(unsigned long duration);
void wait_for_valid_gps_data(HardwareSerial *gps_serial_com, TinyGPSPlus *gps_client);
uint32_t get_unix_timestamp(HardwareSerial *gps_serial_com_ptr, TinyGPSPlus *gps_client_ptr);
void set_device_id(uint8_t *const _device_id, size_t size_device_id);

//#ifdef DUMP_AT_COMMANDS
//  #include <StreamDebugger.h>
//  StreamDebugger debugger(GSM_SERIAL_COM, DEBUG_SERIAL_COM);
//  HayesEngine at_engine((HardwareSerial&)debugger, CLIENT_INTERNAL_BUFFER_SIZE, DEFAULT_CHAR_INTERVAL);
//#else
//  HayesEngine at_engine(GSM_SERIAL_COM, CLIENT_INTERNAL_BUFFER_SIZE, DEFAULT_CHAR_INTERVAL);
//#endif

HayesEngine at_engine(GSM_SERIAL_COM, CLIENT_INTERNAL_BUFFER_SIZE, DEFAULT_CHAR_INTERVAL);

TinyGPSPlus gps_client;
Sim5320Client sim_client(at_engine);
PubSubClient mqtt_client(sim_client);
TinyGPSCustom _fix_age(gps_client, "GPGSA", 2); // $GPGSA sentence, 2th element
TinyGPSCustom pdop(gps_client, "GPGSA", 15); // $GPGSA sentence, 15th element
TinyGPSCustom vdop(gps_client, "GPGSA", 17); // $GPGSA sentence, 16th element

uint32_t last_reconnect_attempt = 0;
uint32_t last_published_attempt = 0;

uint32_t time_before_loop = 0;
uint32_t time_after_loop = 0;

char device_id[16] = {0};
char influx_lp[INFLUXDB_LP_BUFFER_SIZE] = {0};

void setup(){ 
  const uint32_t baudrates[4] = {4800, 57600, 115200, PREFFERED_BAUDRATE};

/*  Initialize GSM, GPS pheripherals */   
  GSM_SERIAL_COM.begin(4800);
  GPS_SERIAL_COM.begin(9600);
  DEBUG_SERIAL_COM.begin(57600);
  while (!DEBUG_SERIAL_COM);
  
  delay(5000);
  //flush system info given by GSM module at startup out of rx buffer
  while (GSM_SERIAL_COM.available()){
    GSM_SERIAL_COM.read();
  }
  at_engine.establish_module_comms(baudrates, 4, PREFFERED_BAUDRATE);
  at_engine.execute_at_command("ATE0");  

  DEBUG_SERIAL_COM.println("Waiting for valid gps data..."); 
  
  wait_for_valid_gps_data(&GPS_SERIAL_COM, &gps_client);
  
  sim_client.init();
  mqtt_client.setServer(mqtt_broker_address, 1883);
  mqtt_client.setCallback(callback);
  mqtt_client.setKeepAlive(120);
  mqtt_client.setSocketTimeout(30);
}

void loop(){
  uint8_t _sats = 0;
  float _pdop = 0, _vdop = 0;
  char _lat[LP_LOCATION_FLOAT_BUF_SIZE] = {0}, _lng[LP_LOCATION_FLOAT_BUF_SIZE] = {0};
  char _hdop[LP_LOCATION_FLOAT_BUF_SIZE] = {0};
  char _speed[LP_SPEED_FLOAT_BUF_SIZE] = {0};
  char _course[LP_COURSE_FLOAT_BUF_SIZE] = {0};
  char _alt[LP_ALTITUDE_FLOAT_BUF_SIZE] = {0};
  uint32_t _fix_age = 0, _unix_timestamp = 0;
  
  if(GPS_SERIAL_COM.available())
      gps_client.encode(GPS_SERIAL_COM.read());

  reconnect();
  
  if(millis() - last_published_attempt > 9000){
    memset(influx_lp, '\0', sizeof(influx_lp));
    smartdelay(1000, &GPS_SERIAL_COM, &gps_client); 

    if(gps_client.location.isValid() && gps_client.location.age() < 1000){
      _sats = gps_client.satellites.value();
      dtostrf(gps_client.location.lat(),LP_LOCATION_FLOAT_MIN_WIDTH,LP_LOCATION_FLOAT_PRECISION,_lat);
      dtostrf(gps_client.location.lng(),LP_LOCATION_FLOAT_MIN_WIDTH,LP_LOCATION_FLOAT_PRECISION,_lng);
      _fix_age = gps_client.location.age();
    }

    if(gps_client.date.isValid() && gps_client.date.age() < 1000 && gps_client.time.isValid() && gps_client.time.age() < 1000){
      _unix_timestamp =  get_unix_timestamp(&GPS_SERIAL_COM, &gps_client);
    }

    if(gps_client.course.isValid() && gps_client.course.age() < 1000)
      dtostrf(gps_client.course.deg(),LP_COURSE_FLOAT_MIN_WIDTH,LP_COURSE_FLOAT_PRECISION,_course);
      
    if(gps_client.speed.isValid() && gps_client.speed.age() < 1000)
      dtostrf(gps_client.speed.kmph(),LP_SPEED_FLOAT_MIN_WIDTH,LP_SPEED_FLOAT_PRECISION,_speed);
      
    if(gps_client.altitude.isValid() && gps_client.altitude.age() < 1000)
      dtostrf(gps_client.altitude.meters(),LP_ALTITUDE_FLOAT_MIN_WIDTH,LP_ALTITUDE_FLOAT_PRECISION,_alt);    

    if(gps_client.hdop.isValid() && gps_client.hdop.age() < 1000)
      dtostrf(gps_client.hdop.hdop(),LP_HDOP_FLOAT_MIN_WIDTH,LP_HDOP_FLOAT_PRECISION,_hdop);
        
    snprintf_P((char*)influx_lp, sizeof(influx_lp), PSTR("tracker_gps_telemetry,tracker_id=%s,vin=%s,fw_tag=%s,hw_conf_tag=%s sats=%u,pdop=%s,hdop=%s,vdop=%s,lat=%s,lng=%s,course=%s,speed=%s,alt=%s,fix_age=%lu %lu000000000"), 
    TRACKER_ID, VIN, FW_TAG, HW_CONF_TAG, _sats, pdop.value(),_hdop,vdop.value(),_lat,_lng,_course,_speed,_alt,_fix_age,_unix_timestamp);
    mqtt_client.publish("trackers", influx_lp);
    DEBUG_SERIAL_COM.println((char*)influx_lp);
    last_published_attempt = millis();
  }
  mqtt_client.loop();
}

void wait_for_valid_gps_data(HardwareSerial *gps_serial_com_ptr, TinyGPSPlus *gps_client_ptr){
  bool  valid_sentence = false;
  
  do{
    if(gps_serial_com_ptr->available())
      valid_sentence = gps_client_ptr->encode(gps_serial_com_ptr->read());
  }while(!valid_sentence);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void smartdelay(unsigned long ms, HardwareSerial* gps_serial_com_ptr, TinyGPSPlus* gps_client_ptr){
  unsigned long time_now = millis();
  do
  {
    while (gps_serial_com_ptr->available())
      gps_client_ptr->encode(gps_serial_com_ptr->read());
  } while (millis() - time_now < ms);
}

void reconnect() {  
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    if (mqtt_client.connect((const char*)device_id, "mqttclient", "Kf6F2LesMAdmkP2P")) {
      //mqtt_client.subscribe("$SYS/broker/subscriptions/count");
    } else {
      DEBUG_SERIAL_COM.print("failed, rc=");
      DEBUG_SERIAL_COM.print(mqtt_client.state());
      DEBUG_SERIAL_COM.println(" try again in MQTT_CLIENT_RECONNECT_INTERVAL seconds");
      // Wait before retrying
      delay(MQTT_CLIENT_RECONNECT_INTERVAL);
    }
  }
}

void set_device_id(uint8_t *const _device_id, size_t size_device_id){
  char *token;
  at_engine.execute_at_command("AT+CGSN");
  token = strtok((char*)at_engine.buf_content(), "\r\n");
  strncpy((char*)_device_id, (const char*)token, size_device_id);
}

uint32_t get_unix_timestamp(HardwareSerial *gps_serial_com_ptr, TinyGPSPlus *gps_client_ptr){
  if(gps_client_ptr->time.isValid() && gps_client_ptr->time.age() < 1000 && gps_client_ptr->date.isValid() && gps_client_ptr->date.age() < 1000){
    setTime(gps_client_ptr->time.hour(),gps_client_ptr->time.minute(),gps_client_ptr->time.second(),gps_client_ptr->date.day(),gps_client_ptr->date.month(),gps_client_ptr->date.year());
    adjustTime(0 * SECS_PER_HOUR);
    return now();
  }
  return 0;
}
