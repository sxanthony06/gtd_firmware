#pragma once
#include <HardwareSerial.h>

#define MAX_SERIAL_BAUDRATE 115200
#define MIN_SERIAL_BAUDRATE 4800

class HayesEngine {
    private:
        HardwareSerial* serial_device = NULL;
        char* buf;
        uint16_t buf_size = 0;	
	size_t buf_index = 0;	
        uint8_t yield_duration_in_ms = 0;
        
        size_t raw_bytes_sent(uint16_t wait_time, size_t attempts);
        int16_t ingest_serial_dev_initial_response(void);
        
        
    public:
	HayesEngine(HardwareSerial& serial_dev, uint16_t len, uint8_t yield_duration);
        ~HayesEngine();
	
        void execute_at_command(const char *const cmd);
        int8_t establish_module_comms(const uint32_t* baudrates, size_t amount_possible_rates, uint32_t preffered_baudrate);
        const char *const buf_content(void);
        bool set_buf_size(size_t size);
        
        size_t pipe_raw_input(const uint8_t* tmp_buf, size_t s);
        int16_t poll_for_async_response(uint16_t wait_time);
};