#include <hayesengine.h>
#include <Arduino.h>
#include <HardwareSerial.h>

 HayesEngine::HayesEngine(HardwareSerial& serial_dev, uint16_t len, uint8_t yield_duration){
  this->serial_device = &serial_dev;
  this->buf_size = len;
  this->yield_duration_in_ms = yield_duration;
  set_buf_size(buf_size);
}

 HayesEngine::~HayesEngine(){
    free(this->buf);
}

void HayesEngine::execute_at_command(const char *const cmd){	
	memset(this->buf, 0, (size_t)this->buf_size);
	this->buf_index = 0;
	
	serial_device->println(cmd);
	ingest_serial_dev_initial_response();
		
	//Serial.print("cmd:\t"); Serial.println(cmd);
	//Serial.print("buffer content after cmd execution: "); Serial.println(this->buf);
}

// Check if baudrate of GPS/GSM module equals that of the one desired. If not, set it to PREFFERED_BAUDRATE
// Uses AT+IPREX and AT+IPR too as cmd.
int8_t HayesEngine::establish_module_comms(const uint32_t* const rates_array, size_t rates_array_size, uint32_t pref_rate)
{
  uint8_t attempts = 3;
  char at_cmd[15] = {0};
  const char* expected_response = "OK";
  
  for(int i = 0; i < rates_array_size; i++){
  	if((rates_array[i] > MAX_SERIAL_BAUDRATE) || (rates_array[i] < MIN_SERIAL_BAUDRATE))
  		return -1; 
  	serial_device->begin(rates_array[i]);
    	for(int j = 0; j < attempts; j++){
      		execute_at_command("AT");
      		if(strstr((const char*)this->buf, expected_response) != NULL){
			if(rates_array[i] != pref_rate){
				snprintf(at_cmd, sizeof(at_cmd), "AT+IPREX=%li", pref_rate);
				execute_at_command(at_cmd);
				
				snprintf(at_cmd, sizeof(at_cmd), "AT+IPR=%li", pref_rate);
				execute_at_command(at_cmd);
				
				if(strstr((const char*)this->buf, expected_response) != NULL)
					serial_device->begin(pref_rate);								            
			}
			return 1;
		}     
    	}
  }
  return 0;
}

size_t HayesEngine::pipe_raw_input(const uint8_t* tmp_buf, size_t s){
	return serial_device->write(tmp_buf,s);
}
	
int16_t HayesEngine::ingest_serial_dev_initial_response(){
	uint16_t x = this->buf_index;
	uint32_t previous_timestamp = millis();
		
	do{
		if(this->serial_device->available()){
			if(x == (this->buf_size)-1)
				return -1;
			this->buf[x++] = serial_device->read();
			previous_timestamp = millis();
		}
	}while((millis() - previous_timestamp) < (uint32_t)yield_duration_in_ms);
	this->buf_index = x;
	return this->buf_index;
}

size_t HayesEngine::raw_bytes_sent(uint16_t wait_time, size_t attempts){
    	char *token;
	size_t amount_bytes_written = 0;
	bool response_found = false;
    	
    	
	for(uint8_t i = 0; (i < attempts) && (response_found == false); i++){
		poll_for_async_response(wait_time);
		//Serial.print("buffer content: "); Serial.println((char*)this->buf);
		token = strtok(token, "+CIPSEND:");
		if(token != NULL){
			token = strtok(token, ",");
			token = strtok(NULL, ",");
			token = strtok(NULL, "\r\n");
			if(token != NULL){
				amount_bytes_written = atoi((const char*)token);
				response_found = true;
			}
	
		}
		
	}
	return amount_bytes_written;
}

int16_t HayesEngine::poll_for_async_response(uint16_t wait_time){
	uint16_t x = this->buf_index;	
	uint32_t previous_timestamp = millis();
		
	do{
		if(this->serial_device->available()){
			if(x == (this->buf_size)-1)
				return -1;
			this->buf[x++] = serial_device->read();
		}
	}while((millis() - previous_timestamp) < (uint32_t)wait_time);
	this->buf_index = x;
	return this->buf_index;
}

const char *const HayesEngine::buf_content(void){
	const char *const tmp_buf = this->buf; 	
	return tmp_buf; 
}

bool HayesEngine::set_buf_size(size_t s){
    if (s == 0) {
        return false;
    }
    if (this->buf_size == 0) {
        this->buf = (char*)malloc(s);
    } else {
        char* new_buf = (char*)realloc(this->buf, s);
        if (new_buf != NULL) {
            this->buf = new_buf;
        } else {
            return false;
        }
    }
    this->buf_size = s;
    memset(this->buf, 0, (size_t)this->buf_size);
    
    return (this->buf != NULL);
}