#include "ESP8266WIFI.h"
#include "USART.h"
#include "util/ESP8266_AT.h"
#include "stdio.h"

////////////////////////
// Buffer Definitions //
////////////////////////
#define ESP8266_RX_BUFFER_LEN 128	// Number of bytes in the serial receive buffer
#define TCP_RX_BUFFER_LEN 128
char esp8266RxBuffer[ESP8266_RX_BUFFER_LEN];
char tcpRxBuffer[TCP_RX_BUFFER_LEN];
volatile unsigned int bufferHead; // Holds position of latest byte placed in buffer.
volatile unsigned int tcpBufferHead = 0;
volatile bool buffering = FALSE;

char receiveState = 0x00;

////////////////////
// Initialization //
////////////////////

bool esp8266Begin()
{
	bool test = FALSE;
	test = esp8266Test();
	if(test)
	{
		if (esp8266SetMux(0))
			return TRUE;
		return FALSE;
	}
	return FALSE;
}

///////////////////////
// Basic AT Commands //
///////////////////////

bool esp8266Test()
{
	//esp8266SendCommand(ESP8266_TEST, ESP8266_CMD_EXECUTE, 0);
	esp8266ClearBuffer();
	usartSendArrar(ESP8266_USART, "AT\r\n");
	if(esp8266ReadForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT))
		return TRUE;
	return FALSE;
}

////////////////////
// WiFi Functions //
////////////////////

int16_t esp8266GetMode()
{
	bool rsp = FALSE;
	char* p, mode;
	esp8266SendCommand(ESP8266_WIFI_MODE, ESP8266_CMD_QUERY, 0);
	rsp = esp8266ReadForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
	if (rsp)
	{
		// Then get the number after ':':
		p = strchr(esp8266RxBuffer, ':');
		if (p != NULL)
		{
			mode = *(p+1);
			if ((mode >= '1') && (mode <= '3'))
				return (mode - 48); // Convert ASCII to decimal
		}
		
		return ESP8266_RSP_UNKNOWN;
	}
	
	return rsp;
}

bool esp8266SetMode(esp8266_wifi_mode mode)
{
	char modeChar[2] = {0, 0};
	sprintf(modeChar, "%d", mode);
	esp8266SendCommand(ESP8266_WIFI_MODE, ESP8266_CMD_SETUP, modeChar);
	
	return esp8266ReadForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
}

bool esp8266Connect(const char * ssid, const char * pwd)
{
	// The ESP8266 can be set to one of three modes:
	//  1 - ESP8266_MODE_STA - Station only
	//  2 - ESP8266_MODE_AP - Access point only
	//  3 - ESP8266_MODE_STAAP - Station/AP combo
	bool rc = FALSE;
	if(esp8266SetMode(ESP8266_MODE_STA))
	{
		usartSendArrar(USART2, "Mode set to station\n");
		esp8266ClearBuffer();
		usartSendArrar(ESP8266_USART, "AT");
		usartSendArrar(ESP8266_USART, (uint8_t *)ESP8266_CONNECT_AP);
		usartSendArrar(ESP8266_USART, "=\"");
		usartSendArrar(ESP8266_USART, (uint8_t *)ssid);
		usartSendArrar(ESP8266_USART, "\"");
		if (pwd != NULL)
		{
			usartSendArrar(ESP8266_USART, ",");
			usartSendArrar(ESP8266_USART, "\"");
			usartSendArrar(ESP8266_USART, (uint8_t *)pwd);
			usartSendArrar(ESP8266_USART, "\"");
		}
		usartSendArrar(ESP8266_USART, "\r\n");
		rc = esp8266ReadForResponses(RESPONSE_OK, RESPONSE_FAIL, WIFI_CONNECT_TIMEOUT);
		return rc;
	}
	return FALSE;
}

/////////////////////
// TCP/IP Commands //
/////////////////////

bool esp8266TcpConnect(uint8_t * destination, uint8_t * port)
{
	bool rsp = FALSE;
	esp8266ClearBuffer();
	usartSendArrar(ESP8266_USART, "AT");
	usartSendArrar(ESP8266_USART, (uint8_t *)ESP8266_TCP_CONNECT);
	usartSendArrar(ESP8266_USART, "=");
	usartSendArrar(ESP8266_USART, "\"TCP\",");
	usartSendArrar(ESP8266_USART, "\"");
	usartSendArrar(ESP8266_USART, destination);
	usartSendArrar(ESP8266_USART, "\",");
	usartSendArrar(ESP8266_USART, port);
	usartSendArrar(ESP8266_USART, "\r\n");
	// Example good: CONNECT\r\n\r\nOK\r\n
	// Example bad: DNS Fail\r\n\r\nERROR\r\n
	// Example meh: ALREADY CONNECTED\r\n\r\nERROR\r\n
	rsp = esp8266ReadForResponses(RESPONSE_OK, RESPONSE_ERROR, CLIENT_CONNECT_TIMEOUT);
	
	if(rsp == FALSE)
	{
		// We may see "ERROR", but be "ALREADY CONNECTED".
		// Search for "ALREADY", and return success if we see it.
		rsp = esp8266SearchBuffer("ALREADY");
		if (rsp)
			return TRUE;
		// Otherwise the connection failed. Return the error code:
		return FALSE;
	}
	// Return 1 on successful (new) connection
	return TRUE;
}

bool esp8266TcpSend(uint8_t *buf, uint16_t size)
{
	bool rsp = FALSE;
	uint8_t i = 0;
	uint8_t *p = buf;
	char params[8];
	if (size > 2048)
		return FALSE; //ESP8266_CMD_BAD
	sprintf(params, "%d", size);
	esp8266SendCommand(ESP8266_TCP_SEND, ESP8266_CMD_SETUP, params);
	
	rsp = esp8266ReadForResponses(RESPONSE_OK, RESPONSE_ERROR, COMMAND_RESPONSE_TIMEOUT);
	//if (rsp > 0)
	if(rsp)
	{
		esp8266ClearBuffer();
		//usartSendArrar(ESP8266_USART, buf);
		//usartSendArrar(ESP8266_USART, "\r");
		for(i=0; i<size; i++)
		{
			USART_ClearFlag(ESP8266_USART,USART_FLAG_TC);	//不加这一句第一个字符会丢失
			USART_SendData(ESP8266_USART, *p);
			while(USART_GetFlagStatus(ESP8266_USART, USART_FLAG_TC) == RESET);
			p++;
		}
		rsp = esp8266ReadForResponse("SEND OK", COMMAND_RESPONSE_TIMEOUT);
		if (rsp)
			return TRUE;
	}
	
	return FALSE;
}

bool esp8266TcpClose()
{
	bool rc = FALSE;
	esp8266SendCommand(ESP8266_TCP_CLOSE, ESP8266_CMD_EXECUTE, 0);
	rc = esp8266ReadForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
	return rc;
}

bool esp8266SetMux(uint8_t mux)
{
	bool rc = FALSE;
	char params[2] = {0, 0};
	params[0] = (mux > 0) ? '1' : '0';
	esp8266SendCommand(ESP8266_TCP_MULTIPLE, ESP8266_CMD_SETUP, params);
	rc = esp8266ReadForResponse(RESPONSE_OK, COMMAND_RESPONSE_TIMEOUT);
	return rc;
}

int tcp_getdata(unsigned char* buf, int count)
{
	int i;
	if(count <= TCP_RX_BUFFER_LEN)
	{
		for(i = 0; i < count; i++)
		{
			*(buf + i) = tcpRxBuffer[i];
		}
		for(i = 0; i < TCP_RX_BUFFER_LEN - count; i++)
		{
			tcpRxBuffer[i] = tcpRxBuffer[i+count];
		}
		return count;
	}
	else
	{
		return -1;
	}
}

//////////////////////////////////////////////////
// Private, Low-Level, Ugly, Hardware Functions //
//////////////////////////////////////////////////

void esp8266SendCommand(const char * cmd, esp8266_command_type type, const char * params)
{
	esp8266ClearBuffer();	// Clear the class receive buffer (esp8266RxBuffer)
	usartSendArrar(ESP8266_USART, "AT");
	usartSendArrar(ESP8266_USART, (uint8_t *)cmd);
	if (type == ESP8266_CMD_QUERY)
		usartSendArrar(ESP8266_USART, "?");
	else if (type == ESP8266_CMD_SETUP)
	{
		usartSendArrar(ESP8266_USART, "=");
		usartSendArrar(ESP8266_USART, (uint8_t *)params);
	}
	usartSendArrar(ESP8266_USART, "\r\n");
}

bool esp8266ReadForResponse(const char * rsp, unsigned int timeout)
{
	unsigned long timeIn = millis();	// Timestamp coming into function
	while (timeIn + timeout > millis()) // While we haven't timed out
	{
		if (esp8266RxBufferAvailable()) // If data is available on ESP8266_USART RX
		{
			if (esp8266SearchBuffer(rsp))	// Search the buffer for goodRsp
				return TRUE;
		}
	}
	return FALSE; // Return the timeout error code
}

bool esp8266ReadForResponses(const char * pass, const char * fail, unsigned int timeout)
{
	bool rc = FALSE;
	unsigned long timeIn = millis();	// Timestamp coming into function
	while (timeIn + timeout > millis()) // While we haven't timed out
	{
		if (esp8266RxBufferAvailable()) // If data is available on UART RX
		{
			rc = esp8266SearchBuffer(pass);
			if (rc)	// Search the buffer for goodRsp
				return TRUE;	// Return how number of chars read
			rc = esp8266SearchBuffer(fail);
			if (rc)
				return FALSE;
		}
	}
	return FALSE;
}

//////////////////
// Buffer Stuff //
//////////////////

void esp8266ClearBuffer()
{
	memset(esp8266RxBuffer, '\0', ESP8266_RX_BUFFER_LEN);
	bufferHead = 0;
}

bool esp8266ReadTcpData()
{
	bool rc;
	int len = 0;
	uint8_t i, byteOfLen = 0;
	char *p;
	rc = esp8266ReadForResponse(RESPONSE_RECEIVED, COMMAND_RESPONSE_TIMEOUT);
	if(rc)
	{
		p = strstr((const char *)esp8266RxBuffer, "+IPD,");
		if(p)
		{
			p += 5;
			while(*p != ':')
			{
				if(byteOfLen == 0)
					len += (int)(*p - '0');
				else if(byteOfLen == 1)
					len = (int)(*p - '0') + len * 10;
				else if(byteOfLen == 2)
					len += (int)(*p - '0') + len * 10;
				p++;
				if(byteOfLen >= 3)	//数据接收不完整，跳出循环（接下来的4个字节内没有接收到冒号）。
				{
					byteOfLen = 0;
					return FALSE;
				}
				byteOfLen++;
			}
			p++;
			for(i = 0; i < len; i++)
			{
				tcpRxBuffer[i] = *p;
				p++;
			}
			esp8266ClearBuffer();
			return TRUE;
		}
		else
			return FALSE;
	}
	else
	{
		return FALSE;
	}
}

bool esp8266RxBufferAvailable()
{
	return (bufferHead > 0) ? TRUE:FALSE;
}

bool esp8266SearchBuffer(const char * test)
{
	int i =0;
	int bufferLen = strlen((const char *)esp8266RxBuffer);
	// If our buffer isn't full, just do an strstr
	if (bufferLen < ESP8266_RX_BUFFER_LEN)
	{
		if(strstr((const char *)esp8266RxBuffer, test))
		{
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}
	else
	{	//! TODO
		// If the buffer is full, we need to search from the end of the 
		// buffer back to the beginning.
		int testLen = strlen(test);
		for (i=0; i<ESP8266_RX_BUFFER_LEN; i++)
		{
			
		}
	}
	return FALSE;
}



void USART1_IRQHandler(void)
{
	if(USART_GetITStatus(ESP8266_USART, USART_IT_RXNE) != RESET)
	{
		esp8266RxBuffer[bufferHead++] = USART_ReceiveData(ESP8266_USART);
	}
	if(bufferHead >= ESP8266_RX_BUFFER_LEN)
	{
		bufferHead = 0;
	}
}
