#include <hayesengine.h>
#include <SoftwareSerial.h>
#include <AltSoftSerial.h>
#include <TinyGPS.h>
#include <string.h>

#define RX 2
#define TX 3
#define BUFFER_SIZE 75
#define GLOBAL_SS_BUFFERSIZE 64
#define DEFAULT_RESPONSE_WAITTIME 100
#define PREFFERED_BAUDRATE 57600

static void smartdelay(unsigned long duration);
static void wait_for_valid_gps_data(Stream *gps_ss, TinyGPS *gps_dev);
static signed short check_for_expected_response(const char *const expected_response, const char* const rx_buffer);
static signed short check_readiness_transmission(const char* const buffer);


SoftwareSerial sim5320_soft_serial = SoftwareSerial(RX, TX);
AltSoftSerial gps_soft_serial;
TinyGPS gps_dev;
HayesEngine<SoftwareSerial> at_engine(sim5320_soft_serial, 64);

void setup()
{ 
  const uint32_t baudrates[3] = {4800, 115200, PREFFERED_BAUDRATE};

  Serial.begin(57600);
  while (!Serial);

  delay(2000);

  // Initialize pheripherals
  sim5320_soft_serial.begin(4800);
  gps_soft_serial.begin(9600);

  //flush system info given by gsm module at startup out of rx buffer
  while (sim5320_soft_serial.available()){
    sim5320_soft_serial.read();
  }
  at_engine.establish_module_comms(baudrates, 3, PREFFERED_BAUDRATE);
  at_engine.execute_at_command("ATE0", 100);
  
//  execute_at_command("AT+CSQ", DEFAULT_RESPONSE_WAITTIME, PSTR("OK"));
//  execute_at_command("AT+CREG?", DEFAULT_RESPONSE_WAITTIME, PSTR("OK"));
//  execute_at_command("AT+CPSI?", 10, PSTR("OK"));
//  execute_at_command("AT+CREG?", DEFAULT_RESPONSE_WAITTIME, PSTR("OK"));

  at_engine.execute_at_command("AT+NETCLOSE", DEFAULT_RESPONSE_WAITTIME);
  delay(5000);
  
  //Define socket PDP context. That means setting AP credentials and network layer protocols.
  //execute_at_command("AT+CGSOCKCONT=1,\"IP\",\"premium\"", 500,PSTR("OK"));

  at_engine.execute_at_command("AT+CSOCKSETPN=1", DEFAULT_RESPONSE_WAITTIME); // Set active PDP context. AP credentials and selected network layer protocol are saved in memory.
  delay(1000);
  at_engine.execute_at_command("AT+CIPMODE=0", DEFAULT_RESPONSE_WAITTIME);  //Select TCP application mode. See AT command sheet for more info
  delay(2000); 
  at_engine.execute_at_command("AT+NETOPEN", DEFAULT_RESPONSE_WAITTIME); //Reserve and open socket. 
  delay(5000);
  
  //Establisch TCP connection in multisocked mode
  at_engine.execute_at_command("AT+CIPOPEN=0,\"TCP\",\"190.112.244.217\",49200", DEFAULT_RESPONSE_WAITTIME);

 Serial.println("Waiting for gps..."); 
 wait_for_valid_gps_data(&gps_soft_serial, &gps_dev);
}

void loop()
{
  char message[75] = {0};
  char response[75] = {0};
  char custom_at_cmd[20] = {0};
  long latitude, longitude, current_altitude; 
  unsigned long position_fix_age, time_fix_age, date, current_time, current_course, current_speed;

  Serial.println("Valida GPS data received..."); 


  gps_dev.get_position(&latitude, &longitude, &position_fix_age);
  gps_dev.get_datetime(&date, &current_time, &time_fix_age);
  current_altitude = gps_dev.altitude();
  current_speed = gps_dev.speed();
  current_course = gps_dev.course();
  
  snprintf(message, sizeof(message), "%li%li%li%lu%lu%lu%lu%lu%lu", latitude, longitude, current_altitude,position_fix_age, time_fix_age, 
  date, current_time, current_course, current_speed);

  snprintf(custom_at_cmd, sizeof(custom_at_cmd), "AT+CIPSEND=0,%i", strlen(message)); 
  at_engine.execute_at_command(custom_at_cmd, DEFAULT_RESPONSE_WAITTIME);

  if(check_readiness_transmission(response)){
    Serial.println("Now sending message!");
    sim5320_soft_serial.print(message);
    memset(response, 0, 75);  
    delay(5000);
    for(unsigned short t=0; sim5320_soft_serial.available(); t++){
      response[t] = sim5320_soft_serial.read();
    }
    Serial.print("Start Rcvd:\t"); Serial.println(response);
    Serial.println("End Rcvd");
  }
  
  if(gps_soft_serial.overflow())
    gps_soft_serial.flushInput();  
  smartdelay(10000, &gps_soft_serial, &gps_dev);
}


static void smartdelay(unsigned long ms, Stream *gps_ss_ptr, TinyGPS *gps_dev_ptr)
{
  unsigned long time_now = millis();
  do 
  {
    while (gps_ss_ptr->available())
      gps_dev_ptr->encode(gps_ss_ptr->read());
  } while (millis() - time_now < ms);
}

static void wait_for_valid_gps_data(Stream *gps_ss_ptr, TinyGPS *gps_dev_ptr)
{
  bool  valid_sentence = false;
  
  do{
    if(gps_ss_ptr->available())
      valid_sentence = gps_dev_ptr->encode(gps_ss_ptr->read());
  }while(!valid_sentence);
}

static signed short check_for_expected_response(const char *const expected_response, const char* const rx_buffer)
{
  signed short ret;
  
  ret = strstr(rx_buffer, "OK") != NULL ? 1 : 0;
}

static signed short check_readiness_transmission(const char* const rx_buffer)
{
  signed short ret;

  if(strstr(rx_buffer, ">") != NULL){
    ret = 1;
  }else {
    ret = 0;
  }
  Serial.print("Readiness is:\t"); Serial.println(ret); Serial.println();
  return ret; 
}
