#include <TimeLib.h>
#include <SD.h>
#include <TinyGPS++.h>
#include <PubSubClient.h>
#include <IPAddress.h>
#include <sim5320client.h>
#include <hayesengine.h>
#include <string.h>

#define CLIENT_INTERNAL_BUFFER_SIZE 275
#define SERIAL_RX_BUFFER_SIZE 128
#define INFLUXDB_LP_BUFFER_SIZE 275

//#define DUMP_AT_COMMANDS
#define DEBUG_SERIAL_COM Serial
#define GSM_SERIAL_COM Serial1
#define GPS_SERIAL_COM Serial2

#define MQTT_KEEPALIVE_TIME 300
#define MQTT_SOCKETTIMEOUT 60

#define PREFFERED_BAUDRATE 57600

#define mqtt_broker_address "190.112.244.217"

#define TRACKER_ID "000001"
#define VIN "JH4KA3230J0018805"
#define FW_TAG "0.0.1"
#define HW_CONF_TAG "0.0.1"

void smartdelay(unsigned long duration);
void wait_for_valid_gps_data(HardwareSerial *gps_serial_com, TinyGPSPlus *gps_client);
uint32_t get_unix_timestamp(HardwareSerial *gps_serial_com_ptr, TinyGPSPlus *gps_client_ptr);
void set_device_id(uint8_t *const _device_id, size_t size_device_id);

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(GSM_SERIAL_COM, DEBUG_SERIAL_COM);
  HayesEngine at_engine(debugger, CLIENT_INTERNAL_BUFFER_SIZE);
#else
  HayesEngine at_engine(GSM_SERIAL_COM, CLIENT_INTERNAL_BUFFER_SIZE);
#endif
TinyGPSPlus gps_client;
Sim5320Client sim_client(at_engine);
PubSubClient mqtt_client(sim_client);
TinyGPSCustom _fix_age(gps_client, "GPGSA", 2); // $GPGSA sentence, 2th element
TinyGPSCustom pdop(gps_client, "GPGSA", 15); // $GPGSA sentence, 15th element
TinyGPSCustom vdop(gps_client, "GPGSA", 17); // $GPGSA sentence, 16th element
uint32_t lastReconnectAttempt = 0;
uint32_t start_loop = 0;
unsigned char device_id[16] = {0};


void setup(){ 
  const uint32_t baudrates[4] = {4800, 9600, 115200, PREFFERED_BAUDRATE};

/*  Initialize GSM, GPS pheripherals */   
  GSM_SERIAL_COM.begin(4800);
  GPS_SERIAL_COM.begin(9600);
#ifdef DEBUG_MODE
  DEBUG_SERIAL_COM.begin(57600);
  while (!DEBUG_SERIAL_COM);
#endif
  delay(5000);
//flush system info given by GSM module at startup out of rx buffer
  while (GSM_SERIAL_COM.available()){
    GSM_SERIAL_COM.read();
  }
  at_engine.establish_module_comms(baudrates, 4, PREFFERED_BAUDRATE);
  at_engine.execute_at_command("ATE0", 100);  

 #ifdef DEBUG_MODE
  DEBUG_SERIAL_COM.println("Waiting for valid gps data..."); 
#endif
  
  wait_for_valid_gps_data(&GPS_SERIAL_COM, &gps_client);
  
  sim_client.init();
  mqtt_client.setServer(mqtt_broker_address, 1883);
  mqtt_client.setCallback(callback);
  mqtt_client.setKeepAlive(180);
  mqtt_client.setSocketTimeout(75);

  reconnect();
}

void loop(){
  unsigned char message[INFLUXDB_LP_BUFFER_SIZE] = {0};
  unsigned char custom_at_cmd[20] = {0};

  uint32_t current_timestamp = 0;
  
  uint8_t _sats;
  float _pdop;
  float _hdop;
  float _vdop;
  float _lat;
  float _lng;
  uint16_t _dir;
  uint8_t _speed;
  uint16_t _alt;
  uint16_t _fix_age;
  uint32_t _unix_timestamp;

  start_loop = millis();
  
  if (!mqtt_client.connected()) {
    uint32_t current_timestamp = millis();
    if (current_timestamp - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = current_timestamp;
      reconnect();
    }
  } else {
    mqtt_client.loop();
    DEBUG_SERIAL_COM.println("Valid GPS data received..."); 

    _sats = gps_client.satellites.value();
    _hdop = gps_client.hdop.hdop();
    _lat = gps_client.location.lat();
    _lng = gps_client.location.lng();
    _dir = gps_client.course.deg();
    _speed = gps_client.speed.kmph();
    _alt = gps_client.altitude.meters();
    _fix_age = gps_client.location.age();
    _unix_timestamp =  get_unix_timestamp(&GPS_SERIAL_COM, &gps_client);
    snprintf_P((char*)message, sizeof(message), PSTR("tracker_gps_telemetry,tracker_id=%s,vin=%s,fw_tag=%s,hw_conf_tag=%s "
    "sats=%u,pdop=%s,hdop=%.2f,vdop=%s,lat=%2.6f,lng=%2.6f,dir=%hu,speed=%u,alt=%hu,fix_age=%hu %lu000000000"), TRACKER_ID,
    VIN, FW_TAG, HW_CONF_TAG,_sats,pdop.value(),_hdop,vdop.value(),_lat,_lng,_dir,_speed,_alt,_fix_age,_unix_timestamp);
    
    DEBUG_SERIAL_COM.print("GPS data: ");DEBUG_SERIAL_COM.println((char*)message); 
    
    smartdelay(5000, &GPS_SERIAL_COM, &gps_client);
  }
}


void smartdelay(unsigned long ms, HardwareSerial *gps_serial_com_ptr, TinyGPSPlus *gps_client_ptr){
  unsigned long time_now = millis();
  do 
  {
    while (gps_serial_com_ptr->available())
      gps_client_ptr->encode(gps_serial_com_ptr->read());
  } while (millis() - time_now < ms);
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

void reconnect() {  
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    DEBUG_SERIAL_COM.println("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt_client.connect((const char*)device_id, "mqttclient", "Kf6F2LesMAdmkP2P")) {
      DEBUG_SERIAL_COM.println("connected");
      //mqtt_client.subscribe("$SYS/broker/subscriptions/count");
    } else {
      DEBUG_SERIAL_COM.print("failed, rc=");
      DEBUG_SERIAL_COM.print(mqtt_client.state());
      DEBUG_SERIAL_COM.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void set_device_id(uint8_t *const _device_id, size_t size_device_id){
  unsigned char *token;
  at_engine.execute_at_command("AT+CGSN",100);
  token = strtok((char*)at_engine.get_buffer_content(), "\r\n");
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
