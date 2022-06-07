#include <TinyGPS++.h>
#include <PubSubClient.h>
#include <IPAddress.h>
#include <sim5320client.h>
#include <hayesengine.h>
#include <string.h>


#define SERIAL_RX_BUFFER_SIZE 128
#define BUFFER_SIZE 75
#define GLOBAL_SS_BUFFERSIZE 64

//#define DUMP_AT_COMMANDS
#define DEBUG_SERIAL_COM  Serial
#define GSM_SERIAL_COM Serial1
#define GPS_SERIAL_COM Serial2

#define PREFFERED_BAUDRATE 57600

#define mqtt_broker_address "190.112.244.217"

static void smartdelay(unsigned long duration);
static void wait_for_valid_gps_data(Stream *gps_ss, TinyGPSPlus *gps_client);
static signed short check_for_expected_response(const char *const expected_response, const char* const rx_buffer);
static signed short check_readiness_transmission(const char* const buffer);

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(GSM_SERIAL_COM, DEBUG_SERIAL_COM);
  HayesEngine at_engine(debugger, 275);
#else
  HayesEngine at_engine(GSM_SERIAL_COM, 275);
#endif
TinyGPSPlus gps_client;
Sim5320Client sim_client(at_engine);
PubSubClient mqtt_client(sim_client);

uint32_t lastReconnectAttempt = 0;

void setup()
{ 
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
}

void loop()
{
  char message[75] = {0};
  char response[75] = {0};
  char custom_at_cmd[20] = {0};
  long lat, lon, current_altitude; 
  unsigned long position_fix_age, time_fix_age, date, current_time, current_course, current_speed;

  if (!mqtt_client.connected()) {
    uint32_t now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      reconnect();
    }
  } else {
    mqtt_client.loop();
    DEBUG_SERIAL_COM.println("Valid GPS data received..."); 
    
    gps_client.get_position(&lat, &lon, &position_fix_age);
    gps_client.get_datetime(&date, &current_time, &time_fix_age);
    current_altitude = gps_client.altitude();
    current_speed = gps_client.speed();
    current_course = gps_client.course();
    
    snprintf(message, sizeof(message), "%li%li%li%lu%lu%lu%lu%lu%lu", lat, lon, current_altitude,position_fix_age, time_fix_age, 
    date, current_time, current_course, current_speed);
    
    DEBUG_SERIAL_COM.print("GPS data: ");DEBUG_SERIAL_COM.println(message); 
    
    smartdelay(5000, &GPS_SERIAL_COM, &gps_client);
  }
}


static void smartdelay(unsigned long ms, Stream *gps_ss_ptr, TinyGPSPlus *gps_client_ptr){
  unsigned long time_now = millis();
  do 
  {
    while (gps_ss_ptr->available())
      gps_client_ptr->encode(gps_ss_ptr->read());
  } while (millis() - time_now < ms);
}

static void wait_for_valid_gps_data(Stream *gps_ss_ptr, TinyGPSPlus *gps_client_ptr){
  bool  valid_sentence = false;
  
  do{
    if(gps_ss_ptr->available())
      valid_sentence = gps_client_ptr->encode(gps_ss_ptr->read());
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
    if (mqtt_client.connect("sxanth23108hvx", "wildcard", "wildcard")) {
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
