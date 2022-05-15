#include <hayesengine.h>
#include <Arduino.h>

template<class T> HayesEngine<T>::HayesEngine(T& serial_dev){
  this->_serial_dev = &serial_dev;
}

template<class T> HayesEngine<T>::HayesEngine(T& serial_dev, uint8_t *const buffer, uint8_t buf_size){
  this->_serial_dev = &serial_dev;
  this->_response_buffer = buffer;
  this->_buffer_size = buf_size;

  memset(_response_buffer, 0, _buffer_size);
}

template<class T> HayesEngine<T>::~HayesEngine(){
  this->_response_buffer = NULL;
  this->_serial_dev = NULL;
}

template<class T> uint8_t HayesEngine<T>::execute_at_command(const char *const cmd, uint8_t wait_time)
{
  memset(this->_response_buffer, 0, this->_buffer_size);
  
  Serial.println("Executing at cmd..");
  uint32_t previous_timestamp = millis();
  uint8_t counter = 0;
    
  _serial_dev->println(cmd);
  while((millis() - previous_timestamp) < (uint32_t)wait_time){
    if(_serial_dev->available()){
      if(counter > (_buffer_size)-1)
        return 0;
      this->_response_buffer[counter++] = _serial_dev->read();
      delayMicroseconds(18);     
    }
  }
  Serial.print("cmd:\t"); Serial.println(cmd);
  Serial.print("buffer content=\t"); Serial.println((char*)this->_response_buffer);  
  execute_at_command_part(cmd, wait_time);
  return counter;
}

template<> inline uint8_t HayesEngine<SoftwareSerial>::execute_at_command_part(const char *const cmd, uint8_t wait_time){
  if(_serial_dev->overflow()){
    Serial.println("Overflow in Arduino rx software serial buffer.");  //For debugging. Needs to be replaced with actual handler for production code
  } 
}

// Check if baudrate of GPS/GSM module equals that of Arduino. If not, set it to PREFFERED_BAUDRATE
// Needs to sent AT+IPREX too as cmd.
template<class T> uint8_t HayesEngine<T>::establish_module_comms(const uint32_t* baudrates, uint8_t amount_possible_rates, uint32_t preffered_baudrate)
{
  uint8_t response_received = 0;
  uint8_t attempts = 3;
  char cmd[15] = {0};
  const char* expected_response = "OK";
  
  for(int i = 0; i < amount_possible_rates; i++){
    _serial_dev->begin(baudrates[i]);
    for(int j = 0; (j < attempts) && response_received !=  1; j++){
      execute_at_command("AT", 50);
      response_received = (String((const char*)this->_response_buffer)).indexOf("OK") != -1 ? 1 : 0;
    }
    if(response_received){
      Serial.print("2. Response received.."); Serial.println(response_received);
      if(baudrates[i] != preffered_baudrate){
        snprintf_P(cmd, sizeof(cmd), PSTR("AT+IPREX=%li"), preffered_baudrate);
        execute_at_command(cmd, 100);

        snprintf_P(cmd, sizeof(cmd), PSTR("AT+IPR=%li"), preffered_baudrate);
        execute_at_command(cmd, 100);
        
        _serial_dev->begin(preffered_baudrate);            
      }
      break;
    }
  }
  return response_received;
}

template<class T> void HayesEngine<T>::setBuffer(uint8_t *const buffer, uint8_t buf_size){
  this->_response_buffer = buffer;
  this->_buffer_size = buf_size;

  memset(_response_buffer, 0, _buffer_size);  
}

template class HayesEngine<SoftwareSerial>;
template class HayesEngine<HardwareSerial>;
 