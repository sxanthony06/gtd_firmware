#include <sim5320client.h>
#include <Arduino.h>
#include <string.h>


Sim5320Client::Sim5320Client(HayesEngine& engine){
  this->_at_engine = &engine;
}

Sim5320Client::~Sim5320Client(){
    _at_engine = NULL;
}

void Sim5320Client::init(void){  	
	_at_engine->execute_at_command("AT+CPIN?");
	_pin_ready = strstr((const char*)_at_engine->buf_content(), "READY") != NULL ? true : false;
	
	stop();
	disconnect_from_gprs(500,10);
	
	_at_engine->execute_at_command("AT+CSQ");		
	//this line is reserved for at cmd to check signal strength. make use of strtol to parse numeric response (CSQ)	
	_at_engine->execute_at_command("AT+CGREG?");
	_netw_registered = strstr(_at_engine->buf_content(), "+CGREG: 0,1") != NULL ? true : false;
	_at_engine->execute_at_command("AT+CIPRXGET=1");
	_at_engine->execute_at_command("AT+CIPENPSH=0");
	_at_engine->execute_at_command("AT+CIPSENDMODE=0");
	_at_engine->execute_at_command("AT+CIPCCFG=3,0,0,0,1,0,10000");	// max 3 retransmission attempts and minimum timeout 10 seconds
	//_at_engine->execute_at_command("AT+CIPTIMEOUT=75000,15000,15000");	
	
	_at_engine->execute_at_command("AT+CGSOCKCONT=1,\"IP\", \"premium\"");
	_at_engine->execute_at_command("AT+CSOCKSETPN=1");
	
	//this line is reserved for at cmd to check is pdp context is set.	

	_at_engine->execute_at_command("AT+CIPMODE=0");
	_at_engine->execute_at_command("AT+CIPSRIP=0");  
	_at_engine->execute_at_command("AT+CIPHEAD=0");
	
	connect_to_gprs(1000,7);
	this->_initialized = true;
}


int Sim5320Client::connect(IPAddress ip, uint16_t port){
    char raw_ip_string[16] = {0};
    snprintf(raw_ip_string, sizeof(raw_ip_string), "%hu.%hu.%hu.%hu", ip[0], ip[1], ip[2], ip[3]);
    return connect((const char *)raw_ip_string, port);
}

int Sim5320Client::connect(const char* host, uint16_t port){
	char custom_at_cmd[70] = {0};
	
	if(!connected_to_gprs()){
		if(!connect_to_gprs(500,10))
			return 0;	
	}
	snprintf(custom_at_cmd, sizeof(custom_at_cmd), "AT+CIPOPEN=0,\"TCP\",\"%s\",%hu", host, port);
	_at_engine->execute_at_command((const char*)custom_at_cmd);
	for(uint8_t i = 0; i < ATTEMPTS_TO_FIND_CONNECT_RESPONSE; i++){
		_at_engine->poll_for_async_response(500);
		if(strstr((const char*)_at_engine->buf_content(), "+CIPOPEN:") != NULL)						
			break;
	}	
}

size_t Sim5320Client::write(uint8_t b){
  return write(&b, (size_t)1);
}

size_t Sim5320Client::write(const uint8_t *buf, size_t s){
	char custom_at_cmd[75] = {0};
	size_t bytes_sent_gsm_module = 0;

	snprintf(custom_at_cmd, sizeof(custom_at_cmd),"AT+CIPSEND=0,%hu", (uint16_t)s);
	_at_engine->execute_at_command((const char*)custom_at_cmd);
	if(strstr((const char*)_at_engine->buf_content(), ">") != NULL){
		bytes_sent_gsm_module = _at_engine->pipe_raw_input(buf, s);
		for(uint8_t i = 0; i < ATTEMPTS_TO_FIND_RAW_INPUT_RESPONSE; i++){
			_at_engine->poll_for_async_response(500);
			if(strstr((const char*)_at_engine->buf_content(), "+CIPSEND") != NULL)
				break;						
		}
	}
	//Serial.print("amount bytes written: ");Serial.println(bytes_sent_gsm_module);
	return bytes_sent_gsm_module;
}

int Sim5320Client::available(){
  char* start;
  char* stop;
  uint8_t buf[6];
  
  _at_engine->execute_at_command("AT+CIPRXGET=4,0");
  const char* const response = (const char*)_at_engine->buf_content();
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
	_at_engine->execute_at_command((const char*)custom_at_cmd);
	const char* const response = (const char*)_at_engine->buf_content();			 
	substr = strtok((char*)response,"\r\n");
	substr = strtok((char*)NULL,"\r\n");
	
	// Null must be allowed to be saved in buf since MQTT does make use of 0 or NULL inside its headers e.g CONNACK. See MQTT v. 3.1.1 specification.
	memcpy((uint8_t*)buf, (const char*)substr, amount_chars_available);
	return amount_chars_available;
}

int Sim5320Client::peek(){}

void Sim5320Client::flush(){

}

void Sim5320Client::stop(){
	if(connected()){
		while(1){
		  _at_engine->execute_at_command("AT+CIPCLOSE=0");
		  if(!connected()) break;
		  delay(500);
		}
	}
}

uint8_t Sim5320Client::connected(){
	char * token;
	bool connected = false;
	
	_at_engine->execute_at_command("AT+CIPCLOSE?");
	//return strstr((const char*)_at_engine->buf_content(), "TCP") != NULL ? 1 : 0;
	token = strtok((char*)_at_engine->buf_content(), ",");
   	return (atoi(token+(strlen(token)-1)) == 1);
}

bool Sim5320Client::connected_to_gprs(){
  	_at_engine->execute_at_command("AT+NETOPEN?");
  	
	return strstr((const char*)_at_engine->buf_content(), "+NETOPEN: 1,") != NULL;
}

bool Sim5320Client::connect_to_gprs(uint16_t wait_time, uint8_t attempts){
	if(connected_to_gprs())
		return true;
	_at_engine->execute_at_command("AT+NETOPEN");
	for(uint8_t i = 0; i < attempts; i++){
		_at_engine->poll_for_async_response(wait_time);	
		if(connected_to_gprs())
			return true;
		_at_engine->poll_for_async_response(wait_time);	
	}
	return false;

}

bool Sim5320Client::disconnect_from_gprs(uint16_t wait_time, uint8_t attempts){
	if(connected_to_gprs()){
		_at_engine->execute_at_command("AT+NETCLOSE");
		for(uint8_t i = 0; i < attempts; i++){
			_at_engine->poll_for_async_response(wait_time);	
			if(strstr((const char*)_at_engine->buf_content(), "+NETOPEN: 0,") != NULL);
				return true;
		}
		return false;
	}else{
		return true;
	}
}

Sim5320Client::operator bool(){}