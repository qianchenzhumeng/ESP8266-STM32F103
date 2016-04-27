#include "common.h"

volatile uint32_t millis_number = 0;

void delay (unsigned int timeout)
{
	unsigned long timeIn = millis();
	while(timeIn + timeout > millis());
}

void sysTickInit()
{
	if(SysTick_Config(SystemCoreClock / 1000))	//1ms
	{
		while(1);
	}
}

void SysTick_Handler(void)
{
  millis_number++;
}

uint32_t millis()
{
	return millis_number;
}
