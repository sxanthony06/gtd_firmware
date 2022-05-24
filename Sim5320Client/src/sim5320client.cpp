#include <sim5320client.h>
#include <Arduino.h>
#include <string.h>


Sim5320Client::Sim5320Client(HayesEngine& engine, uint16_t timeout){
  this->_at_engine = &engine;
  this->_timeout = timeout;
}

Sim5320Client::~Sim5320Client(){
    _at_engine = NULL;
}

void Sim5320Client::init(void){  	
	_at_engine->execute_at_command("AT+CPIN?", 500);
	_pin_ready = strstr((const char*)_at_engine->get_buffer_content(), "READY") != NULL ? true : false;
	
	if(connected() || net_open())
		stop();
	
	_at_engine->execute_at_command("AT+CGREG?", 100);
	_network_registered = strstr((const char*)_at_engine->get_buffer_content(), "0,1") != NULL ? true : false;
	_at_engine->execute_at_command("AT+CSQ", 100);	
	//this line is reserved for at cmd to check signal strength. make use of strtol to parse numeric response (CSQ)
	_at_engine->execute_at_command("AT+CIPRXGET=1", 100);
	_at_engine->execute_at_command("AT+CIPENPSH=0", 100);
	//_at_engine->execute_at_command("AT+CIPSENDMODE=1", 100);		
	//this line is reserved for at cmd to check is pdp context is set.	
	_at_engine->execute_at_command("AT+CGSOCKCONT=1,\"IP\", \"premium\"", 100);
	_at_engine->execute_at_command("AT+CSOCKSETPN=1", 100);

	_at_engine->execute_at_command("AT+CIPMODE=0", 100);
	_at_engine->execute_at_command("AT+CIPSRIP=0", 100);  
	_at_engine->execute_at_command("AT+CIPHEAD=1", 100);
	_at_engine->execute_at_command("AT+CTCPKA=1,1,5", 100);
	
	
	this->_initialized = true;
}


int Sim5320Client::connect(IPAddress ip, uint16_t port){
    uint8_t raw_ip_string[16] = {0};
    snprintf((char*)raw_ip_string, sizeof(raw_ip_string), "%hu.%hu.%hu.%hu", ip[0], ip[1], ip[2], ip[3]);
    return connect((const char *)raw_ip_string, port);
}

int Sim5320Client::connect(const char* host, uint16_t port){
	uint8_t custom_at_cmd[70] = {0};
	
	_at_engine->execute_at_command("AT+CPSI?", 100);
	  //this line is reserved for at cmd to check operation mode (CPSI)
	_at_engine->execute_at_command("AT+NETOPEN?", 100);
	if(strstr((const char*)_at_engine->get_buffer_content(), "1,") == NULL){
		_at_engine->execute_at_command("AT+NETOPEN", 3000);
	}
	snprintf((char*)custom_at_cmd, sizeof(custom_at_cmd), "AT+CIPOPEN=0,\"TCP\",\"%s\",%hu", host, port);
	_at_engine->execute_at_command((const char*)custom_at_cmd, 4000);
	return connected();
}

size_t Sim5320Client::write(uint8_t b){
  return write(&b, (size_t)1); //porta e mester ta &b i no djis b.
}

size_t Sim5320Client::write(const uint8_t *buf, size_t s){
  char custom_at_cmd[75] = {0};
  char token[15] = {0};
  
  snprintf((char*)token, sizeof(token),"+CIPSTAT: %hu,", (uint16_t)s);
  snprintf((char*)custom_at_cmd, sizeof(custom_at_cmd),"AT+CIPSEND=0,%hu", (uint16_t)s);
  _at_engine->execute_at_command((const char*)custom_at_cmd, 100);
  if(strstr((const char*)_at_engine->get_buffer_content(), ">") != NULL){
    _at_engine->pipe_raw_input(buf, s);
    while(1){
    	_at_engine->execute_at_command("AT+CIPSTAT=0", 100);
    	if(strstr((const char*)_at_engine->get_buffer_content(), token) != NULL)
    		break;
    	delay(200);
    }
    return s;
  }else {
    return 0;
  }
}

int Sim5320Client::available(){
  char* start;
  char* stop;
  uint8_t buf[6];
  
  _at_engine->execute_at_command("AT+CIPSTAT=0", 100);
  _at_engine->execute_at_command("AT+CIPRXGET=4,0", 100);
  const char* const response = (const char*)_at_engine->get_buffer_content();
  start = strrchr(response, ',');
  if(start != NULL){
     stop = strchr(start, 13);
     if(stop != NULL){
          for(int i = 0; i<(size_t)(stop-start-sizeof(uint8_t)); i=i+sizeof(uint8_t)){
              buf[i] = start[i+1];
          }
          return atoi((const char*)buf);
     }     
  }
  return 0;
}

//Attempt to read data. Receive data.  Returns char, or -1 for no data, or 0 if connection closed
int Sim5320Client::read(){
	uint8_t b;
	if(read(&b, 1) > 0) return b;
	return -1;
}

//Attempt to read data. Receive data.  Returns size, or -1 for no data, or 0 if connection closed
int Sim5320Client::read(uint8_t *buf, size_t size){
	char* substr;
	uint8_t start_index;
	uint8_t custom_at_cmd[25] = {0};
	int amount_chars_available = 0;	
	
	amount_chars_available = available();
	if(amount_chars_available > size)
		amount_chars_available = (int)size;
	if(size == 0)
		return 0;
	if(amount_chars_available == 0)
		return -1;
	
	snprintf((char*)custom_at_cmd, sizeof(custom_at_cmd), "AT+CIPRXGET=2,0,%d", (int8_t)amount_chars_available);
	_at_engine->execute_at_command((const char*)custom_at_cmd, 200);
	const char* const response = (const char*)_at_engine->get_buffer_content();			 
	substr = strtok((char*)response,"\r\n");
	substr = strtok((char*)NULL,"\r\n");
	
	if(substr != NULL){
		start_index = substr - (const char* const)response;
		int i;	
		strncpy((char*)buf, (const char*)substr, amount_chars_available);

		return amount_chars_available;
	}
	return 0;
}

int Sim5320Client::peek(){}

void Sim5320Client::flush(){
  _at_engine->execute_at_command("AT+CIPENPSH=1", 1000); 
}

void Sim5320Client::stop(){
  _at_engine->execute_at_command("AT+CIPCLOSE=0", 3000); 
  _at_engine->execute_at_command("AT+NETCLOSE", 5000);  
}

uint8_t Sim5320Client::connected(){
	uint8_t status;
	_at_engine->execute_at_command("AT+CIPOPEN?", 100);
	status = strstr((const char*)_at_engine->get_buffer_content(), "TCP") != NULL ? 1 : 0;
  	return status;
}

uint8_t Sim5320Client::net_open(){
	uint8_t status;
  	_at_engine->execute_at_command("AT+NETOPEN?", 100);
	status = strstr((const char*)_at_engine->get_buffer_content(), "1,") != NULL ? 1 : 0;
	return status;
}

Sim5320Client::operator bool(){}