#include "ESP8266Client.h"
#include "ESP8266WIFI.h"
#include "common.h"

bool socketNew(uint8_t* host, uint8_t * port)
{
	return esp8266TcpConnect(host, port);
}

bool socketWrite(uint8_t* buf, uint16_t buflen)
{
	return esp8266TcpSend(buf, buflen);
}

bool socketClose(void)
{
	bool rc = esp8266TcpClose();
	return rc;
}
