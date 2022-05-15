#pragma once
#include <SoftwareSerial.h>

template <class T> class HayesEngine {
    private:
        T* _serial_dev = NULL;
        uint8_t * _response_buffer = NULL;
        uint8_t _buffer_size = 0;
    public:
	HayesEngine(T& serial_dev);
	HayesEngine(T& serial_dev, uint8_t *const buffer, uint8_t buf_size);
        ~HayesEngine();

        uint8_t execute_at_command(const char *const cmd, uint8_t wait_time);
        inline uint8_t execute_at_command_part(const char *const cmd, uint8_t wait_time);
        uint8_t establish_module_comms(const uint32_t* baudrates, uint8_t amount_possible_rates, uint32_t preffered_baudrate);
        void setBuffer(uint8_t *const buffer, uint8_t buf_size);
};