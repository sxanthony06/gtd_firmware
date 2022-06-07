#include <hayesengine.h>
#include <Arduino.h>
#include <HardwareSerial.h>

 HayesEngine::HayesEngine(HardwareSerial& serial_dev, uint16_t buffer_size){
  this->serial_device = &serial_dev;
  this->buffer_size = buffer_size;
  set_buffer_size(buffer_size);
}

 HayesEngine::~HayesEngine(){
    free(this->buffer);
}

 void HayesEngine::execute_at_command(const char *const cmd, uint32_t wait_time)
{	
	memset(this->buffer, 0, (size_t)this->buffer_size);	
	serial_device->println(cmd);
	read_cmd_response(wait_time);
  
	Serial.print("cmd:\t"); Serial.println(cmd);
	Serial.print("buffer content after:"); Serial.println((char*)this->buffer);  
}

// Check if baudrate of GPS/GSM module equals that of the one desired. If not, set it to PREFFERED_BAUDRATE
// Uses AT+IPREX and AT+IPR too as cmd.
uint8_t HayesEngine::establish_module_comms(const uint32_t* const br_arr, uint8_t size_br_arr, uint32_t preferred_baudrate)
{
  uint8_t response_received = 0;
  uint8_t attempts = 3;
  char at_cmd[15] = {0};
  const char* expected_response = "OK";
  
  if(preferred_baudrate > MAX_SERIAL_BAUDRATE)
  	return -1;
  
  for(int i = 0; i < size_br_arr; i++){
  	if((br_arr[i] > MAX_SERIAL_BAUDRATE) || (br_arr[i] < MIN_SERIAL_BAUDRATE))
  		return -1; 
  	serial_device->begin(br_arr[i]);
    	for(int j = 0; j < attempts; j++){
      		execute_at_command("AT", 100);
      		if(strstr((const char*)this->buffer, "OK") != NULL){
			if(br_arr[i] != preferred_baudrate){
				snprintf(at_cmd, sizeof(at_cmd), "AT+IPREX=%li", preferred_baudrate);
				execute_at_command(at_cmd, 100);
				
				snprintf(at_cmd, sizeof(at_cmd), "AT+IPR=%li", preferred_baudrate);
				execute_at_command(at_cmd, 100);
				
				if(strstr((const char*)this->buffer, "OK") != NULL)
					serial_device->begin(preferred_baudrate);								            
			}
			this->yield_duration_in_us = ((float)1/preferred_baudrate)*1000*1000;			
			return 1;
		}     
    	}
  }
  return 0;
}

bool HayesEngine::set_buffer_size(uint16_t size){
    if (size == 0) {
        // Cannot set it back to 0
        return false;
    }
    if (this->buffer_size == 0) {
        this->buffer = (uint8_t*)malloc(size);
    } else {
        uint8_t* newBuffer = (uint8_t*)realloc(this->buffer, size);
        if (newBuffer != NULL) {
            this->buffer = newBuffer;
        } else {
            return false;
        }
    }
    this->buffer_size = size;
    return (this->buffer != NULL);
}

size_t HayesEngine::pipe_raw_input(uint8_t* buf, size_t s, uint32_t wait_time){
	size_t amount_bytes_written = 0;
	
	memset(this->buffer, 0, (size_t)this->buffer_size);
	amount_bytes_written = serial_device->write(buf,s);
	read_cmd_response(wait_time);
	
	return amount_bytes_written;
}
	
size_t HayesEngine::read_cmd_response(uint32_t wait_time){	
	size_t counter = 0;
	uint32_t previous_timestamp = millis();
		
	do{
		while(serial_device->available()){
			buffer[counter++] = serial_device->read();
			delayMicroseconds((this->yield_duration_in_us)*10);
		}
		if(counter > 0)
			break;
	}while((millis() - previous_timestamp) < (uint32_t)wait_time);

	return counter;
}

 const uint8_t *const HayesEngine::get_buffer_content(void){
  return (const uint8_t *const)buffer; 
}