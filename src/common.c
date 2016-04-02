#include "common.h"

volatile uint32_t millis_number = 0;

void delay (unsigned int count)
{
unsigned int index;

	for(index =0;index<count;index++)
	{
		;
	}
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
