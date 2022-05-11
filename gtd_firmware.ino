#include <SoftwareSerial.h>
#include <AltSoftSerial.h>
#include <TinyGPS.h>
#include <string.h>

#define RX 2
#define TX 3
#define PREFFERED_BAUDRATE 57600 
#define BUFFER_SIZE 75
#define GLOBAL_SS_BUFFERSIZE 64
#define DEFAULT_RESPONSE_WAITTIME 500


static char global_rx_buffer[GLOBAL_SS_BUFFERSIZE];

static void execute_at_command(const char *const cmd, unsigned short wait_time, char* const rx_buffer);
static void establish_module_comms(void);
static void smartdelay(unsigned long duration);
static void wait_for_valid_gps_data(Stream *gps_ss, TinyGPS *gps_dev);
static signed short check_for_expected_response(const char *const expected_response, const char* const rx_buffer);
static signed short check_readiness_transmission(const char* const buffer);


SoftwareSerial sim5320_soft_serial = SoftwareSerial(RX, TX);
AltSoftSerial gps_soft_serial;
TinyGPS gps_dev;


void setup()
{  
  Serial.begin(57600);
  while (!Serial);

  delay(2000);
  
  // Initialize pheripherals
  sim5320_soft_serial.begin(4800);
  gps_soft_serial.begin(9600);
  
  //flush system info given by gsm module at startup out of rx buffer
  if(sim5320_soft_serial.overflow()){ 
    while(sim5320_soft_serial.available())
      sim5320_soft_serial.read();
  }

  establish_module_comms();

  execute_at_command("ATE0", DEFAULT_RESPONSE_WAITTIME, global_rx_buffer);
  
//  execute_at_command("AT+CSQ", DEFAULT_RESPONSE_WAITTIME, PSTR("OK"));
//  execute_at_command("AT+CREG?", DEFAULT_RESPONSE_WAITTIME, PSTR("OK"));
//  execute_at_command("AT+CPSI?", 10, PSTR("OK"));
//  execute_at_command("AT+CREG?", DEFAULT_RESPONSE_WAITTIME, PSTR("OK"));

  execute_at_command("AT+NETCLOSE", DEFAULT_RESPONSE_WAITTIME, global_rx_buffer);
  delay(5000);
  
  //Define socket PDP context. That means setting AP credentials and network layer protocols.
  //execute_at_command("AT+CGSOCKCONT=1,\"IP\",\"premium\"", 500,PSTR("OK"));

  execute_at_command("AT+CSOCKSETPN=1", 100, global_rx_buffer); // Set active PDP context. AP credentials and selected network layer protocol are saved in memory.
  delay(1000);
  execute_at_command("AT+CIPMODE=0", DEFAULT_RESPONSE_WAITTIME, global_rx_buffer);  //Select TCP application mode. See AT command sheet for more info
  delay(2000); 
  execute_at_command("AT+NETOPEN", DEFAULT_RESPONSE_WAITTIME, global_rx_buffer); //Reserve and open socket. 
  delay(5000);
  
  //Establisch TCP connection in multisocked mode
  execute_at_command("AT+CIPOPEN=0,\"TCP\",\"190.112.244.217\",49200", DEFAULT_RESPONSE_WAITTIME, global_rx_buffer);

 wait_for_valid_gps_data(&gps_soft_serial, &gps_dev);
}

void loop()
{
  char message[75] = {0};
  char response[75] = {0};
  char custom_at_cmd[20] = {0};
  long latitude, longitude, current_altitude; 
  unsigned long position_fix_age, time_fix_age, date, current_time, current_course, current_speed;

  gps_dev.get_position(&latitude, &longitude, &position_fix_age);
  gps_dev.get_datetime(&date, &current_time, &time_fix_age);
  current_altitude = gps_dev.altitude();
  current_speed = gps_dev.speed();
  current_course = gps_dev.course();
  
  snprintf(message, sizeof(message), "%li%li%li%lu%lu%lu%lu%lu%lu", latitude, longitude, current_altitude,position_fix_age, time_fix_age, 
  date, current_time, current_course, current_speed);

  snprintf(custom_at_cmd, sizeof(custom_at_cmd), "AT+CIPSEND=0,%i", strlen(message));
  execute_at_command(custom_at_cmd, DEFAULT_RESPONSE_WAITTIME, global_rx_buffer);
  if(check_readiness_transmission(global_rx_buffer)){
    Serial.println("Now sending message!");
    sim5320_soft_serial.print(message);
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

//execute AT command, fill global_rx_buffer with response
static void execute_at_command(const char *const cmd, unsigned short wait_time, char* const rx_buffer)
{
  unsigned long previous_timestamp = millis();
  unsigned short transmission_started = 0;
  unsigned short counter = 0;
    
  sim5320_soft_serial.println(cmd);
  while((millis() - previous_timestamp) < (unsigned long)wait_time){
    if(sim5320_soft_serial.available()){
      rx_buffer[counter++] = (char)sim5320_soft_serial.read();
      delayMicroseconds(18);     
    }
  }

  Serial.print("cmd:\t"); Serial.println(cmd);
  Serial.print("buffer content=\t"); Serial.println(rx_buffer); //For debugging
  if(sim5320_soft_serial.overflow()){
    Serial.println("Overflow in Arduino rx software serial buffer.");  //For debugging. Needs to be replaced with actual handler for production code.
  }
  
  memset(rx_buffer, 0, BUFFER_SIZE);
}

// Check if baudrate of GPS/GSM module equals that of Arduino. If not, set it to PREFFERED_BAUDRATE
// Needs to sent AT+IPREX too as cmd.
static void establish_module_comms(void)
{
  unsigned short response = 0;
  const long baudrates[3] = {4800, 115200, PREFFERED_BAUDRATE};
  char cmd[15] = {0};
  const char* expected_response = "OK";
  
  for(int i = 0; i < sizeof(baudrates)/sizeof(baudrates[0]); i++){
    sim5320_soft_serial.begin(baudrates[i]);
    for(int j = 0; (j < 3) && response !=  1; j++){
      execute_at_command("AT", DEFAULT_RESPONSE_WAITTIME, global_rx_buffer);
      response = check_for_expected_response("OK", global_rx_buffer);
    }
    if(response == 1){
      if(baudrates[i] != PREFFERED_BAUDRATE){
        snprintf_P(cmd, sizeof(cmd), PSTR("AT+IPREX=%li"), PREFFERED_BAUDRATE);
        execute_at_command(cmd, DEFAULT_RESPONSE_WAITTIME, global_rx_buffer);

        snprintf_P(cmd, sizeof(cmd), PSTR("AT+IPR=%li"), PREFFERED_BAUDRATE);
        execute_at_command(cmd, DEFAULT_RESPONSE_WAITTIME, global_rx_buffer);
        
        sim5320_soft_serial.begin(PREFFERED_BAUDRATE);            
      }
      break;
    }
  }
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
