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
	_at_engine->execute_at_command("AT+CPIN?", 100);
	_pin_ready = strstr((const char*)_at_engine->get_buffer_content(), "READY") != NULL ? true : false;
	
	stop();
	close_net();
	
	_at_engine->execute_at_command("AT+CSQ", 100);		
	//this line is reserved for at cmd to check signal strength. make use of strtol to parse numeric response (CSQ)	
	_at_engine->execute_at_command("AT+CGREG?", 100);

	//_at_engine->execute_at_command("AT+CGDCONT=1,\"IP\",\"premium\",\"0.0.0.0\",0,0",100);

	_at_engine->execute_at_command("AT+CIPRXGET=1", 100);
	_at_engine->execute_at_command("AT+CIPENPSH=0", 100);
	_at_engine->execute_at_command("AT+CIPSENDMODE=0", 100);
	_at_engine->execute_at_command("AT+CIPCCFG=10,0,0,0,1,0,75000", 100);	
	_at_engine->execute_at_command("AT+CIPTIMEOUT=75000,15000,15000", 100);	
	
		
	_at_engine->execute_at_command("AT+CGSOCKCONT=1,\"IP\", \"premium\"", 100);
	_at_engine->execute_at_command("AT+CSOCKSETPN=1", 100);
	
	//this line is reserved for at cmd to check is pdp context is set.	

	_at_engine->execute_at_command("AT+CIPMODE=0", 100);
	_at_engine->execute_at_command("AT+CIPSRIP=0", 100);  
	_at_engine->execute_at_command("AT+CIPHEAD=1", 100);
	
	open_net();
	this->_initialized = true;
}


int Sim5320Client::connect(IPAddress ip, uint16_t port){
    uint8_t raw_ip_string[16] = {0};
    snprintf((char*)raw_ip_string, sizeof(raw_ip_string), "%hu.%hu.%hu.%hu", ip[0], ip[1], ip[2], ip[3]);
    return connect((const char *)raw_ip_string, port);
}

int Sim5320Client::connect(const char* host, uint16_t port){
	uint8_t custom_at_cmd[70] = {0};
	
	if(!is_net_open()){
		if(!open_net())
			return 0;	
	}
	snprintf((char*)custom_at_cmd, sizeof(custom_at_cmd), "AT+CIPOPEN=0,\"TCP\",\"%s\",%hu", host, port);
	_at_engine->execute_at_command((const char*)custom_at_cmd, 5000);
	return connected();
}

size_t Sim5320Client::write(uint8_t b){
  return write(&b, (size_t)1); //porta e mester ta &b i no djis b.
}

size_t Sim5320Client::write(const uint8_t *buf, size_t s){
	char custom_at_cmd[75] = {0};
	uint16_t amount_bytes_written = 0;
	
	//Serial.print("amount of bytes to write: "); Serial.println(s);

	snprintf(custom_at_cmd, sizeof(custom_at_cmd),"AT+CIPSEND=0,%hu", (uint16_t)s);
	_at_engine->execute_at_command((const char*)custom_at_cmd, 100);
	if(strstr((const char*)_at_engine->get_buffer_content(), ">") != NULL){
		amount_bytes_written = _at_engine->pipe_raw_input(buf, s, 2000);
		do{
			if(strstr((const char*)_at_engine->get_buffer_content(), "+CIPSEND:") != NULL)
				break;
			_at_engine->read_cmd_response(1000);
		}while(1);
		Serial.print("amount of bytes written: "); Serial.println(amount_bytes_written);
		return amount_bytes_written;
	}else {
		return 0;
	}
}

int Sim5320Client::available(){
  char* start;
  char* stop;
  uint8_t buf[6];
  
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
	_at_engine->execute_at_command((const char*)custom_at_cmd, 100);
	const char* const response = (const char*)_at_engine->get_buffer_content();			 
	substr = strtok((char*)response,"\r\n");
	substr = strtok((char*)NULL,"\r\n");
	
	// Null must be allowed to be saved in buf since MQTT does make use of 0 or NULL inside its headers e.g CONNACK. See MQTT v. 3.1.1 specification.
	memcpy((uint8_t*)buf, (const char*)substr, amount_chars_available);
	/* start debug code...
	Serial.println("debugging connack");
	for(int i = 0; i < amount_chars_available; i++){
		Serial.print("byte: "); Serial.print(buf[i], HEX); Serial.print(" or "); Serial.print(buf[i], BIN);
	}
	Serial.println();
	// end debug code... */
	return amount_chars_available;
}

int Sim5320Client::peek(){}

void Sim5320Client::flush(){

}

void Sim5320Client::stop(){
	if(connected()){
		while(1){
		  _at_engine->execute_at_command("AT+CIPCLOSE=0", 3000);
		  if(!connected()) break;
		  delay(1000);
		}
	}
/*	if(is_net_open()){
		while(1){
		  _at_engine->execute_at_command("AT+NETCLOSE", 2000);
		  if(!is_net_open()) break;
		  delay(1000);
		}
	}	*/
}

uint8_t Sim5320Client::connected(){
	_at_engine->execute_at_command("AT+CIPOPEN?", 1000);
	return strstr((const char*)_at_engine->get_buffer_content(), "TCP") != NULL ? 1 : 0;

}

uint8_t Sim5320Client::is_net_open(){
  	_at_engine->execute_at_command("AT+NETOPEN?", 100);
	return strstr((const char*)_at_engine->get_buffer_content(), "+NETOPEN: 1,") != NULL;
}

uint8_t Sim5320Client::open_net(){
	if(is_net_open())
		return 1;
	_at_engine->execute_at_command("AT+NETOPEN", 5000);
	return strstr((const char*)_at_engine->get_buffer_content(), "+NETOPEN: 1,") != NULL;

}

uint8_t Sim5320Client::close_net(){
	if(is_net_open()){
		_at_engine->execute_at_command("AT+NETCLOSE", 6000);
		return strstr((const char*)_at_engine->get_buffer_content(), "+NETOPEN: 0,") != NULL;
	}else{
		return 1;
	}
}

Sim5320Client::operator bool(){}