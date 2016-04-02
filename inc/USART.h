#ifndef __USART__H
#define __USART__H

#include "stm32f10x.h"

void USART1_Configuration(void);
void USART2_Configuration(void);
void usartSendArrar(USART_TypeDef *USART, uint8_t *Arrar);
void usartSendData(USART_TypeDef *USART, uint8_t data);

#endif
