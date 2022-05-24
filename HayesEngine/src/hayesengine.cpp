#include <hayesengine.h>
#include <Arduino.h>

 HayesEngine::HayesEngine(HardwareSerial& serial_dev, uint16_t buffer_size){
  this->serial_device = &serial_dev;
  this->buffer_size = buffer_size;
  set_buffer_size(buffer_size);
}

 HayesEngine::~HayesEngine(){
    free(this->buffer);
}

 uint8_t HayesEngine::execute_at_command(const char *const cmd, uint32_t wait_time)
{		
	memset(this->buffer, 0, (size_t)this->buffer_size);
	uint32_t previous_timestamp = millis();
	uint8_t counter = 0;

	serial_device->println(cmd);
	do{
		while(serial_device->available()){
			buffer[counter++] = serial_device->read();
	}
	}while((millis() - previous_timestamp) < (uint32_t)wait_time);
  
	Serial.print("cmd:\t"); Serial.println(cmd);
	Serial.print("buffer content after:"); Serial.println((char*)this->buffer);  
	return counter;
}

// Check if baudrate of GPS/GSM module equals that of Arduino. If not, set it to PREFFERED_BAUDRATE
// Needs to sent AT+IPREX too as cmd.
 uint8_t HayesEngine::establish_module_comms(const uint32_t* baudrates, uint8_t amount_possible_rates, uint32_t preffered_baudrate)
{
  uint8_t response_received = 0;
  uint8_t attempts = 3;
  char cmd[15] = {0};
  const char* expected_response = "OK";
  
  for(int i = 0; i < amount_possible_rates; i++){
    serial_device->begin(baudrates[i]);
    for(int j = 0; (j < attempts) && response_received !=  1; j++){
      execute_at_command("AT", 50);
      response_received = (String((const char*)this->buffer)).indexOf("OK") != -1 ? 1 : 0;
    }
    if(response_received){
      Serial.print("2. Response received.."); Serial.println(response_received);
      if(baudrates[i] != preffered_baudrate){
        snprintf_P(cmd, sizeof(cmd), PSTR("AT+IPREX=%li"), preffered_baudrate);
        execute_at_command(cmd, 500);

        snprintf_P(cmd, sizeof(cmd), PSTR("AT+IPR=%li"), preffered_baudrate);
        execute_at_command(cmd, 100);
        
        serial_device->begin(preffered_baudrate);            
      }
      break;
    }
  }
  return response_received;
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

size_t HayesEngine::pipe_raw_input(uint8_t* buf, size_t s){
	memset(this->buffer, 0, (size_t)this->buffer_size);
	uint32_t previous_timestamp = millis();
	uint8_t counter = 0;
	
	for(uint16_t i = 0; i < (uint16_t)s; i++){
		serial_device->print(buf[i]);
	}
	do{
		while(serial_device->available()){
			buffer[counter++] = serial_device->read();
	}
	}while((millis() - previous_timestamp) < 100);
  
	Serial.print("raw input:\t"); for(int i = 0; i < (uint16_t)s; i++){ Serial.print(buf[i]); Serial.print(" ");} Serial.println();
}
 const uint8_t *const HayesEngine::get_buffer_content(void){
  return (const uint8_t *const)buffer; 
}