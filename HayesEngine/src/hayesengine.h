#pragma once
#include <HardwareSerial.h>

#define MAX_SERIAL_BAUDRATE 115200
#define MIN_SERIAL_BAUDRATE 4800

class HayesEngine {
    private:
        HardwareSerial* serial_device = NULL;
        uint8_t* buffer;
        uint16_t buffer_size = 0;
        uint8_t yield_duration_in_us = ((float)1/MIN_SERIAL_BAUDRATE)*1000*1000;
    public:
	HayesEngine(HardwareSerial& serial_dev, uint16_t buffer_size);
        ~HayesEngine();
	
        void execute_at_command(const char *const cmd, uint32_t wait_time);
        uint8_t establish_module_comms(const uint32_t* baudrates, uint8_t amount_possible_rates, uint32_t preffered_baudrate);
        bool set_buffer_size(uint16_t size);
        size_t pipe_raw_input(uint8_t* buf, size_t s, uint32_t wait_time);
        size_t read_cmd_response(uint32_t wait_time);
        const uint8_t *const get_buffer_content(void);
        
};