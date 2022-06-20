#pragma once

#include <hayesengine.h>
#include "Client.h"
#include <IPAddress.h>
#include <HardwareSerial.h>

#define ATTEMPTS_TO_FIND_RAW_INPUT_RESPONSE 10
#define ATTEMPTS_TO_FIND_CONNECT_RESPONSE 10

class Sim5320Client : public Client {

private:
	HayesEngine* _at_engine;
    	bool _pin_ready = false;
    	bool _initialized = false;
    	bool _netw_registered = false;

	bool connected_to_gprs();
	bool connect_to_gprs(uint16_t wait_time, uint8_t attempts);
	bool disconnect_from_gprs(uint16_t wait_time, uint8_t attempts);

public:
	Sim5320Client(HayesEngine& engine);
	~Sim5320Client();

	void init(void);
	virtual int connect(IPAddress ip, uint16_t port);
	virtual int connect(const char *host, uint16_t port);
	
	using Print::write;
	virtual size_t write(uint8_t);
	virtual size_t write(const uint8_t *buf, size_t s);

	virtual int available();
	virtual int read();
	virtual int read(uint8_t *buf, size_t size);
	virtual int peek();
	virtual void flush();
	virtual void stop();
	virtual uint8_t connected();
	virtual operator bool();

};