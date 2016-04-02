#ifndef __COMMON_H
#define __COMMON_H

#include "string.h"
#include "stdint.h"
#include "stm32f10x.h"

typedef enum
{
	FALSE = 0,
	TRUE = 1
}bool;

void delay (unsigned int count);
void sysTickInit(void);
uint32_t millis(void);

#endif
