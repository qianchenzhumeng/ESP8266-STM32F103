#ifndef __ESP8266CLIENT__H
#define __ESP8266CLIENT__H
#include "common.h"

bool socketNew(uint8_t* host, uint8_t * port);
bool socketWrite(uint8_t * buf, uint16_t buflen);
bool socketClose(void);
#endif
