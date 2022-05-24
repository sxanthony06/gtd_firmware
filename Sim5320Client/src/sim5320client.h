#pragma once

#include <hayesengine.h>
#include "Client.h"
#include <IPAddress.h>
#include <HardwareSerial.h>

class Sim5320Client : public Client {

private:
	HayesEngine* _at_engine;
	uint16_t _timeout;
    	bool _pin_ready = false;
    	bool _network_registered = false;
    	bool _initialized = false;

public:
	Sim5320Client(HayesEngine& engine, uint16_t timeout);
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
	uint8_t net_open();
	virtual operator bool();

};