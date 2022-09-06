/************************************************************************************
// Copyright (c) 2021 SS USA Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM,OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
 ************************************************************************************/

/*
 ===============================================================================
 Includes :
 ===============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <safe_lib.h>
#include <errno.h>
#include <stddef.h>
#include <sys/time.h>
#include <time.h>
#include <sys/select.h>

#include "SessionControl.h"
#include "osalLinux.h"

#ifdef MODBUS_STACK_TCPIP_ENABLED
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <netinet/tcp.h>
#else
#include <termios.h>
#include <signal.h>
#endif


// CRC calculation is for Modbus RTU stack
#ifndef MODBUS_STACK_TCPIP_ENABLED
// high-order byte CRC values table
static const uint8_t crc_high_order_table[] = {
		0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
		0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
		0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
		0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
		0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
		0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
		0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
		0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
		0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
		0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
		0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
		0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
		0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
		0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
		0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
		0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
		0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
		0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
		0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
		0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
		0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
		0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
		0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
		0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
		0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
		0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
};

// low-order byte CRC values table
static const uint8_t crc_lower_order_table[] = {
		0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06,
		0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD,
		0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
		0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A,
		0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4,
		0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
		0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3,
		0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
		0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
		0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29,
		0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED,
		0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
		0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60,
		0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67,
		0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
		0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
		0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E,
		0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
		0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71,
		0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92,
		0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
		0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B,
		0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B,
		0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
		0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42,
		0x43, 0x83, 0x41, 0x81, 0x80, 0x40
};

#endif //#ifndef MODBUS_STACK_TCPIP_ENABLED

// global variable to store stack configurations
extern stDevConfig_t g_stModbusDevConfig;

#ifndef MODBUS_STACK_TCPIP_ENABLED
// To check blocking serial read
int checkforblockingread(int fd, long a_lRespTimeout);

#endif

/*
 ===============================================================================
 Function Declarations
 ===============================================================================
 */

// Function declarations for callback functions from ModbusApp
void (*ModbusMaster_ApplicationCallback)(stMbusAppCallbackParams_t *pstMbusAppCallbackParams,
																	uint16_t u16TransactionID);

// Functions that are used in Modbus TCP communication mode
#ifdef MODBUS_STACK_TCPIP_ENABLED
void (*ReadFileRecord_CallbackFunction)(uint8_t, uint8_t*,uint16_t, uint16_t,uint8_t,
		stException_t *,
		stMbusRdFileRecdResp_t*);

void (*WriteFileRecord_CallbackFunction)(uint8_t, uint8_t*, uint16_t, uint16_t,uint8_t,
		stException_t*,
		stMbusWrFileRecdResp_t*);

void (*ReadDeviceIdentification_CallbackFunction)(uint8_t, uint8_t*, uint16_t, uint16_t,uint8_t,
		stException_t*,
		stRdDevIdResp_t*);
#else
//Functions that are used in Modbus RTU communication mode
void (*ReadFileRecord_CallbackFunction)(uint8_t, uint8_t*, uint16_t,uint8_t,
		stException_t *,
		stMbusRdFileRecdResp_t*);

void (*WriteFileRecord_CallbackFunction)(uint8_t, uint8_t*, uint16_t,uint8_t,
		stException_t*,
		stMbusWrFileRecdResp_t*);

void (*ReadDeviceIdentification_CallbackFunction)(uint8_t, uint8_t*, uint16_t,uint8_t,
		stException_t*,
		stRdDevIdResp_t*);
#endif

#ifndef MODBUS_STACK_TCPIP_ENABLED

/*
 ===============================================================================
 Function Definitions
 ===============================================================================
 */

/**
 * @fn static uint16_t crc16(uint8_t *buffer, uint16_t buffer_length)
 *
 * @brief This function calculates the CRC 16 of Modbus data packet.
 *
 * @param buffer 				[in] uint8_t* pointer to buffer holding modbus data packet
 * @param buffer_length  		[in] uint16_t buffer length of the received packet
 *
 * @return [out] none
 */

static uint16_t crc16(uint8_t *buffer, uint16_t buffer_length)
{
	// MSB
	uint8_t crc_hi = 0xFF;
	// LSB
	uint8_t crc_lo = 0xFF;
	// Index to find out CRC from table
	unsigned int i;

	// Iterate buffer
	while (buffer_length--) {
		// get the CRC
		i = crc_hi ^ *buffer++;
		crc_hi = crc_lo ^ crc_high_order_table[i];
		crc_lo = crc_lower_order_table[i];
	}

	return (crc_hi << 8 | crc_lo);
}  // End of crc16
#endif //#ifndef MODBUS_STACK_TCPIP_ENABLED

/**
 * @fn int sleep_micros(long lMicroseconds)
 *
 * @brief This function lets the current thread to sleep for an interval specified
 * with nanosecond precision. This adjusts the speed to complete previous send request.
 * The function gets the current time, adds the time equal to inter-frame delay and sleeps
 * for that much of time.
 *
 * @param lMilliseconds [in] long time duration in micro-seconds (inter-frame delay of current request)
 *
 * @return uint8_t [out] 0 if thread sleeps for calculated duration in nano-seconds
 * 						 -1 if function fails to calculate or sleep
 *
 */
int sleep_micros(long lMicroseconds)
{
	struct timespec ts;
	// get time
	int rc = clock_gettime(CLOCK_MONOTONIC, &ts);
	if(0 != rc)
	{
		perror("Stack fatal error: Sleep function: clock_gettime failed: ");
		return -1;
	}
	// calculate the next tick
	unsigned long next_tick = (ts.tv_sec * 1000000000L + ts.tv_nsec) + lMicroseconds*1000;
	ts.tv_sec = next_tick / 1000000000L;
	ts.tv_nsec = next_tick % 1000000000L;
	do
	{
		// calculate the nano sleep
		rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
	} while(EINTR == rc);

	// clock_gettime SUCCESS
	if(0 == rc)
	{
		return 0;
	}

	printf("Stack: Error in sleep function: %d\n", rc);
	return -1;
} // End of sleep_micros


/**
 * @fn void ApplicationCallBackHandler(stMbusPacketVariables_t *pstMBusRequesPacket,eStackErrorCode eMbusStackErr)
 *
 * @brief This function get called after a response is received from Modbus device or timeout
 * occurred. It decides which callback function to call from ModbusApp depending on the
 * function code i.e. operation type of the request.
 * This function first checks if there was any exception from Modbus slave device
 * for the request. It fills up this information about exception in the structure which
 * needs to send to ModbusApp callback function. Next, this function stores current
 * time stamp as time to send response to ModbusApp, in the same structure. Later, depending
 * on the function code, it fills up structure parameters and calls ModbusApp callback function.
 *
 * For function codes READ_FILE_RECORD, WRITE_FILE_RECORD and READ_DEVICE_IDENTIFICATION,
 * ApplicationCallBackHandler fills up parameters in the structure and calls ModbusApp
 * functions directly.
 *
 * @param pstMBusRequesPacket [in] pointer to structure containing response of a request
 * 								   with matching transaction ID
 * @param eMbusStackErr       [in] Error code from stack which occurred for this request
 * 								   which needs to be sent back to ModbusApp
 *
 * @return none
 */
void ApplicationCallBackHandler(stMbusPacketVariables_t *pstMBusRequesPacket,eStackErrorCode eMbusStackErr)
{
	stException_t  stException = {0};

	if(NULL == pstMBusRequesPacket)
		return;

	eModbusFuncCode_enum eMbusFunctionCode = ((pstMBusRequesPacket->m_u8FunctionCode) & 0x7F);
	uint8_t u8exception = (8 == ((pstMBusRequesPacket->m_u8FunctionCode) & 0x80)>>4)?1:0;

	//check for exceptions
	if(u8exception)
	{
		stException.m_u8ExcStatus = MODBUS_EXCEPTION;
		stException.m_u8ExcCode = pstMBusRequesPacket->m_stMbusRxData.m_au8DataFields[0];
		pstMBusRequesPacket->m_stMbusRxData.m_au8DataFields[0] = 0;
		pstMBusRequesPacket->m_stMbusRxData.m_u8Length = 0;
		pstMBusRequesPacket->m_u8FunctionCode = (pstMBusRequesPacket->m_u8FunctionCode & (0x7F));
	}
	else if(STACK_NO_ERROR != eMbusStackErr)
	{
		stException.m_u8ExcStatus = MODBUS_STACK_ERROR;
		stException.m_u8ExcCode = eMbusStackErr;
		pstMBusRequesPacket->m_stMbusRxData.m_u8Length = 0;
	}

	// Initialize response posted timestamp
	timespec_get(&(pstMBusRequesPacket->m_objTimeStamps.tsRespSent), TIME_UTC);

	switch(eMbusFunctionCode)
	{
	default:
		break;
	case READ_COIL_STATUS :
	case READ_INPUT_STATUS :
	case READ_HOLDING_REG :
	case READ_INPUT_REG :
	case READ_WRITE_MUL_REG :
	case WRITE_MULTIPLE_COILS :
	case WRITE_SINGLE_COIL :
	case WRITE_SINGLE_REG :
	case WRITE_MULTIPLE_REG :

		ModbusMaster_ApplicationCallback = pstMBusRequesPacket->pFunc;

		//Check for callback function
		if(NULL != ModbusMaster_ApplicationCallback)
		{

			stMbusAppCallbackParams_t stMbusAppCallbackParams;

			stMbusAppCallbackParams.m_lPriority = pstMBusRequesPacket->m_lPriority;

			stMbusAppCallbackParams.m_u16TransactionID = pstMBusRequesPacket->m_u16AppTxID;
#ifdef MODBUS_STACK_TCPIP_ENABLED
			stMbusAppCallbackParams.m_u8UnitID = pstMBusRequesPacket->m_u8UnitID;

			memcpy_s((void*)&stMbusAppCallbackParams.m_u8IpAddr,
					(rsize_t) sizeof(stMbusAppCallbackParams.m_u8IpAddr),
					(void*)&pstMBusRequesPacket->m_stMbusRxData.m_au8DataFields,
					(rsize_t) sizeof(stMbusAppCallbackParams.m_au8MbusRXDataDataFields));

			stMbusAppCallbackParams.u16Port = pstMBusRequesPacket->u16Port;
#else
			stMbusAppCallbackParams.m_u8ReceivedDestination = pstMBusRequesPacket->m_u8ReceivedDestination;
#endif
			stMbusAppCallbackParams.m_u8FunctionCode = pstMBusRequesPacket->m_u8FunctionCode;

			stMbusAppCallbackParams.m_u8ExceptionExcCode = stException.m_u8ExcCode;
			stMbusAppCallbackParams.m_u8ExceptionExcStatus = stException.m_u8ExcStatus;
			stMbusAppCallbackParams.m_u8MbusRXDataLength = pstMBusRequesPacket->m_stMbusRxData.m_u8Length;

			memcpy_s((void*)&stMbusAppCallbackParams.m_au8MbusRXDataDataFields,
					(rsize_t) sizeof(stMbusAppCallbackParams.m_au8MbusRXDataDataFields),
					(void*)&pstMBusRequesPacket->m_stMbusRxData.m_au8DataFields,
					(rsize_t) sizeof(pstMBusRequesPacket->m_stMbusRxData.m_au8DataFields));

			stMbusAppCallbackParams.m_u16StartAdd = pstMBusRequesPacket->m_u16StartAdd;
			stMbusAppCallbackParams.m_u16Quantity = pstMBusRequesPacket->m_u16Quantity;
			stMbusAppCallbackParams.m_objTimeStamps = pstMBusRequesPacket->m_objTimeStamps;
			// send to the master application
			ModbusMaster_ApplicationCallback(&stMbusAppCallbackParams, pstMBusRequesPacket->m_u16TransactionID);
		}
		break;

	case READ_FILE_RECORD :
	{
		stMbusRdFileRecdResp_t	*pstMbusRdFileRecdResp = NULL;
		stRdFileSubReq_t	*pstSubReq = NULL;
		stRdFileSubReq_t	*pstTempSubReq = NULL;

		pstMbusRdFileRecdResp =
				pstMBusRequesPacket->m_stMbusRxData.m_pvAdditionalData;

		ReadFileRecord_CallbackFunction = pstMBusRequesPacket->pFunc;
#ifdef MODBUS_STACK_TCPIP_ENABLED

		if(NULL != ReadFileRecord_CallbackFunction)
			ReadFileRecord_CallbackFunction(pstMBusRequesPacket->m_u8UnitID,
					pstMBusRequesPacket->m_u8IpAddr,
					pstMBusRequesPacket->u16Port,
					pstMBusRequesPacket->m_u16TransactionID,
					pstMBusRequesPacket->m_u8FunctionCode,
					&stException,pstMbusRdFileRecdResp);


#else
		if(NULL != ReadFileRecord_CallbackFunction)
			ReadFileRecord_CallbackFunction(pstMBusRequesPacket->m_u8UnitID,
					&pstMBusRequesPacket->m_u8ReceivedDestination,
					pstMBusRequesPacket->m_u16TransactionID,
					pstMBusRequesPacket->m_u8FunctionCode,
					&stException,pstMbusRdFileRecdResp);
#endif

		if(NULL != pstMbusRdFileRecdResp)
			pstSubReq = pstMbusRdFileRecdResp->m_stSubReq.pstNextNode;

		while(NULL != pstSubReq)
		{
			//deallocate the received data memory
			OSAL_Free(pstSubReq->m_pu16RecData);
			pstTempSubReq = pstSubReq->pstNextNode;
			OSAL_Free(pstSubReq);
			pstSubReq = pstTempSubReq;
		}
		if(NULL != pstMbusRdFileRecdResp)
		{
			OSAL_Free(pstMbusRdFileRecdResp);
			pstMBusRequesPacket->m_stMbusRxData.m_pvAdditionalData = NULL;
		}
	}
	break;
	case WRITE_FILE_RECORD :
	{
		stMbusWrFileRecdResp_t	*pstMbusWrFileRecdResp = NULL;
		stWrFileSubReq_t	*pstSubReq = NULL;
		stWrFileSubReq_t	*pstTempSubReq = NULL;

		pstMbusWrFileRecdResp = pstMBusRequesPacket->m_stMbusRxData.m_pvAdditionalData;

		WriteFileRecord_CallbackFunction = pstMBusRequesPacket->pFunc;

#ifdef MODBUS_STACK_TCPIP_ENABLED
		// callback function to application when write record is received
		if(NULL != WriteFileRecord_CallbackFunction)
			WriteFileRecord_CallbackFunction(pstMBusRequesPacket->m_u8UnitID,
					pstMBusRequesPacket->m_u8IpAddr,
					pstMBusRequesPacket->u16Port,
					pstMBusRequesPacket->m_u16TransactionID,
					pstMBusRequesPacket->m_u8FunctionCode,
					&stException,pstMbusWrFileRecdResp);

#else
		if(NULL != WriteFileRecord_CallbackFunction)
			// callback function to application when write record is received
			WriteFileRecord_CallbackFunction(pstMBusRequesPacket->m_u8UnitID,
					&pstMBusRequesPacket->m_u8ReceivedDestination,
					pstMBusRequesPacket->m_u16TransactionID,
					pstMBusRequesPacket->m_u8FunctionCode,
					&stException,pstMbusWrFileRecdResp);

#endif
		if(NULL != pstMbusWrFileRecdResp)
			pstSubReq = pstMbusWrFileRecdResp->m_stSubReq.pstNextNode;

		while(NULL != pstSubReq)
		{
			//deallocate memory
			OSAL_Free(pstSubReq->m_pu16RecData);
			pstTempSubReq = pstSubReq->pstNextNode;
			OSAL_Free(pstSubReq);
			pstSubReq = pstTempSubReq;
		}
		if(NULL != pstMbusWrFileRecdResp)
		{
			OSAL_Free(pstMbusWrFileRecdResp);
			pstMBusRequesPacket->m_stMbusRxData.m_pvAdditionalData = NULL;
		}
	}
	break;

	case READ_DEVICE_IDENTIFICATION :
	{
		stRdDevIdResp_t	*pstMbusRdDevIdResp = NULL;
		SubObjList_t		*pstObjList = NULL;
		SubObjList_t		*pstTempObjReq = NULL;

		pstMbusRdDevIdResp = pstMBusRequesPacket->m_stMbusRxData.m_pvAdditionalData;

		ReadDeviceIdentification_CallbackFunction = pstMBusRequesPacket->pFunc;

#ifdef MODBUS_STACK_TCPIP_ENABLED
		// callback function to application to read device identification
		if(NULL != ReadDeviceIdentification_CallbackFunction)
			ReadDeviceIdentification_CallbackFunction(pstMBusRequesPacket->m_u8UnitID,
					pstMBusRequesPacket->m_u8IpAddr,
					pstMBusRequesPacket->u16Port,
					pstMBusRequesPacket->m_u16TransactionID,
					pstMBusRequesPacket->m_u8FunctionCode,
					&stException,pstMbusRdDevIdResp);

#else
		// callback function to application to read device identification
		if(NULL != ReadDeviceIdentification_CallbackFunction)
			ReadDeviceIdentification_CallbackFunction(pstMBusRequesPacket->m_u8UnitID,
					&pstMBusRequesPacket->m_u8ReceivedDestination,
					pstMBusRequesPacket->m_u16TransactionID,
					pstMBusRequesPacket->m_u8FunctionCode,
					&stException,pstMbusRdDevIdResp);
#endif

		if(NULL != pstMbusRdDevIdResp)
		{
			pstObjList = pstMbusRdDevIdResp->m_pstSubObjList.pstNextNode;
			if(pstMbusRdDevIdResp->m_pstSubObjList.m_u8ObjectValue != NULL)
			{
				// free allocated memory
				OSAL_Free(pstMbusRdDevIdResp->m_pstSubObjList.m_u8ObjectValue);
				pstMbusRdDevIdResp->m_pstSubObjList.m_u8ObjectValue = NULL;
			}
		}

		while(NULL != pstObjList)
		{
			//free allocated memory
			OSAL_Free(pstObjList->m_u8ObjectValue);
			pstObjList->m_u8ObjectValue = NULL;
			pstTempObjReq = pstObjList->pstNextNode;
			pstObjList->pstNextNode = NULL;
			OSAL_Free(pstObjList);
			pstObjList = pstTempObjReq;
		}
		if(NULL != pstMbusRdDevIdResp)
		{
			OSAL_Free(pstMbusRdDevIdResp);
			pstMBusRequesPacket->m_stMbusRxData.m_pvAdditionalData = NULL;
		}

	}
	break;
	}
}  // End of ApplicationCallBackHandler


/**
 * @fn uint8_t DecodeRxMBusPDU(uint8_t *ServerReplyBuff,
		uint16_t u16BuffInex,
		stMbusPacketVariables_t *pstMBusRequesPacket)
 *
 * @brief This function decodes the data received from a Modbus response from
 * the specified buffer index and fills up the details in a structure
 * of type pstMBusRequesPacket.
 *
 * @param ServerReplyBuff 		[in] uint8_t* Pointer to input buffer received from Modbus response
 * @param u16BuffInex 	  		[in] uint16_t buffer index from where actual response data starts
 * 							  		 and decode
 * @param pstMBusRequesPacket 	[in] stMbusPacketVariables_t* Pointer to Modbus request packet
 * 									 structure to store decoded data
 *
 * @return uint8_t [out] STACK_NO_ERROR in case of successful decoding of the response;
 * 						 STACK_ERROR_MALLOC_FAILED in case of error
 *
 */
uint8_t DecodeRxMBusPDU(uint8_t *ServerReplyBuff,
		uint16_t u16BuffInex,
		stMbusPacketVariables_t *pstMBusRequesPacket)
{
	uint8_t u8ReturnType = STACK_NO_ERROR;
	uint16_t u16TempBuffInex = 0;
	uint8_t u8Count = 0;
	stEndianess_t stEndianess = { 0 };
	eModbusFuncCode_enum eMbusFunctionCode = MBUS_MIN_FUN_CODE;
	uint8_t bIsDone = 0;
	// decode function code
	eMbusFunctionCode = (eModbusFuncCode_enum)pstMBusRequesPacket->m_u8FunctionCode;
	// next object ID only if more follows
	if(8 == ((0x80 & eMbusFunctionCode)>>4))
	{
		pstMBusRequesPacket->m_stMbusRxData.m_au8DataFields[0] = ServerReplyBuff[u16BuffInex++];
		pstMBusRequesPacket->m_stMbusRxData.m_u8Length = 0;
		pstMBusRequesPacket->m_stMbusRxData.m_pvAdditionalData = NULL;
	}
	else
	{
		switch(eMbusFunctionCode)
		{
		default:
			break;
		case READ_COIL_STATUS :
		case READ_INPUT_STATUS :
			pstMBusRequesPacket->m_stMbusRxData.m_u8Length = ServerReplyBuff[u16BuffInex++];
			memcpy_s((pstMBusRequesPacket->m_stMbusRxData.m_au8DataFields),
					sizeof(pstMBusRequesPacket->m_stMbusRxData.m_au8DataFields),
					&(ServerReplyBuff[u16BuffInex]),
					pstMBusRequesPacket->m_stMbusRxData.m_u8Length);
			break;

		case READ_HOLDING_REG :
		case READ_INPUT_REG :
		case READ_WRITE_MUL_REG :
			pstMBusRequesPacket->m_stMbusRxData.m_u8Length = ServerReplyBuff[u16BuffInex++];
			u16TempBuffInex = u16BuffInex;
			while(0 == bIsDone)
			{
				stEndianess.stByteOrder.u8SecondByte = ServerReplyBuff[u16BuffInex++];
				stEndianess.stByteOrder.u8FirstByte = ServerReplyBuff[u16BuffInex++];
				pstMBusRequesPacket->m_stMbusRxData.m_au8DataFields[u8Count++] = stEndianess.stByteOrder.u8FirstByte;
				pstMBusRequesPacket->m_stMbusRxData.m_au8DataFields[u8Count++] = stEndianess.stByteOrder.u8SecondByte;
				if(pstMBusRequesPacket->m_stMbusRxData.m_u8Length <= (u16BuffInex - u16TempBuffInex))
					bIsDone = 1;
			}
			break;

		case WRITE_MULTIPLE_COILS :
		case WRITE_MULTIPLE_REG :
		{
			stEndianess.stByteOrder.u8SecondByte = ServerReplyBuff[u16BuffInex++];
			stEndianess.stByteOrder.u8FirstByte = ServerReplyBuff[u16BuffInex++];
			pstMBusRequesPacket->m_u16StartAdd = stEndianess.u16word;
			stEndianess.stByteOrder.u8SecondByte = ServerReplyBuff[u16BuffInex++];
			stEndianess.stByteOrder.u8FirstByte = ServerReplyBuff[u16BuffInex++];
			pstMBusRequesPacket->m_u16Quantity = stEndianess.u16word;
		}
		break;

		case WRITE_SINGLE_REG :
		case WRITE_SINGLE_COIL :
		{
			pstMBusRequesPacket->m_stMbusRxData.m_u8Length = 2;
			stEndianess.stByteOrder.u8SecondByte = ServerReplyBuff[u16BuffInex++];
			stEndianess.stByteOrder.u8FirstByte = ServerReplyBuff[u16BuffInex++];
			pstMBusRequesPacket->m_u16StartAdd = stEndianess.u16word;
			stEndianess.stByteOrder.u8SecondByte = ServerReplyBuff[u16BuffInex++];
			stEndianess.stByteOrder.u8FirstByte = ServerReplyBuff[u16BuffInex++];
			pstMBusRequesPacket->m_stMbusRxData.m_au8DataFields[u8Count++] = stEndianess.stByteOrder.u8FirstByte;
			pstMBusRequesPacket->m_stMbusRxData.m_au8DataFields[u8Count++] = stEndianess.stByteOrder.u8SecondByte;

		}
		break;

		case READ_FILE_RECORD :
		{
			stMbusRdFileRecdResp_t	*pstMbusRdFileRecdResp = NULL;
			stRdFileSubReq_t		*pstSubReq = NULL;
			uint8_t u16TempBuffInex = 0;
			uint8_t u8Arrayindex2 = 0;
			uint16_t *pu16RecData = NULL;

			// allocate memory to read file record
			pstMbusRdFileRecdResp = OSAL_Malloc(sizeof(stMbusRdFileRecdResp_t));
			if(NULL == pstMbusRdFileRecdResp)
			{
				u8ReturnType = STACK_ERROR_MALLOC_FAILED;
				return u8ReturnType;
			}
			pstMBusRequesPacket->m_stMbusRxData.m_pvAdditionalData = pstMbusRdFileRecdResp;

			pstMbusRdFileRecdResp->m_u8RespDataLen = ServerReplyBuff[u16BuffInex++];

			pstSubReq = &(pstMbusRdFileRecdResp->m_stSubReq);

			pstSubReq->pstNextNode = NULL;

			u16TempBuffInex = u16BuffInex;
			while(1)
			{
				pstSubReq->m_u8FileRespLen = ServerReplyBuff[u16BuffInex++];
				pstSubReq->m_u8RefType = ServerReplyBuff[u16BuffInex++];

				pu16RecData = OSAL_Malloc(pstSubReq->m_u8FileRespLen-1);
				if(NULL == pu16RecData)
				{
					u8ReturnType = STACK_ERROR_MALLOC_FAILED;
					return u8ReturnType;
				}
				pstSubReq->m_pu16RecData = pu16RecData;
				// update allocated memory according to modbus Packet format
				for(u8Arrayindex2 = 0;u8Arrayindex2<((pstSubReq->m_u8FileRespLen-1)/2);u8Arrayindex2++)
				{
					stEndianess.stByteOrder.u8SecondByte = ServerReplyBuff[u16BuffInex++];
					stEndianess.stByteOrder.u8FirstByte = ServerReplyBuff[u16BuffInex++];
					*pu16RecData = stEndianess.u16word;
					pu16RecData++;
				}
				if(pstMbusRdFileRecdResp->m_u8RespDataLen > (u16BuffInex - u16TempBuffInex))
				{
					pstSubReq->pstNextNode = OSAL_Malloc(sizeof(stRdFileSubReq_t));
					if(NULL == pstSubReq->pstNextNode)
					{
						u8ReturnType = STACK_ERROR_MALLOC_FAILED;
						return u8ReturnType;
					}
					pstSubReq = pstSubReq->pstNextNode;
					pstSubReq->pstNextNode = NULL;
				}
				else
				{
					break;
				}
			}
		}
		break;
		case WRITE_FILE_RECORD :
		{
			stMbusWrFileRecdResp_t	*pstMbusWrFileRecdResp = NULL;
			stWrFileSubReq_t		*pstSubReq = NULL;
			uint8_t u16TempBuffInex = 0;
			uint8_t u8Arrayindex = 0;
			uint16_t *pu16RecData = NULL;

			pstMbusWrFileRecdResp = OSAL_Malloc(sizeof(stMbusWrFileRecdResp_t));
			if(NULL == pstMbusWrFileRecdResp )
			{
				u8ReturnType = STACK_ERROR_MALLOC_FAILED;
				return u8ReturnType;
			}
			// Initialize allocated memory with zero
			memset(pstMbusWrFileRecdResp,00,sizeof(stMbusWrFileRecdResp_t));

			pstMBusRequesPacket->m_stMbusRxData.m_pvAdditionalData = pstMbusWrFileRecdResp;

			pstMbusWrFileRecdResp->m_u8RespDataLen = ServerReplyBuff[u16BuffInex++];

			pstSubReq = &(pstMbusWrFileRecdResp->m_stSubReq);

			u16TempBuffInex = u16BuffInex;

			while(1)
			{
				pstSubReq->m_u8RefType = ServerReplyBuff[u16BuffInex++];

				stEndianess.stByteOrder.u8SecondByte = ServerReplyBuff[u16BuffInex++];
				stEndianess.stByteOrder.u8FirstByte = ServerReplyBuff[u16BuffInex++];
				pstSubReq->m_u16FileNum = stEndianess.u16word;

				stEndianess.stByteOrder.u8SecondByte = ServerReplyBuff[u16BuffInex++];
				stEndianess.stByteOrder.u8FirstByte = ServerReplyBuff[u16BuffInex++];
				pstSubReq->m_u16RecNum = stEndianess.u16word;

				stEndianess.stByteOrder.u8SecondByte = ServerReplyBuff[u16BuffInex++];
				stEndianess.stByteOrder.u8FirstByte = ServerReplyBuff[u16BuffInex++];
				pstSubReq->m_u16RecLen = stEndianess.u16word;

				pu16RecData = OSAL_Malloc(pstSubReq->m_u16RecLen*sizeof(uint16_t));
				if(NULL == pu16RecData)
				{
					u8ReturnType = STACK_ERROR_MALLOC_FAILED;
					return u8ReturnType;
				}
				pstSubReq->m_pu16RecData = pu16RecData;
				// update allocated memory according to modbus Packet format
				for(u8Arrayindex = 0;
						u8Arrayindex<(pstSubReq->m_u16RecLen);u8Arrayindex++)
				{
					stEndianess.stByteOrder.u8SecondByte = ServerReplyBuff[u16BuffInex++];
					stEndianess.stByteOrder.u8FirstByte = ServerReplyBuff[u16BuffInex++];
					*pu16RecData = stEndianess.u16word;
					pu16RecData++;
				}

				if(pstMbusWrFileRecdResp->m_u8RespDataLen > (u16BuffInex - u16TempBuffInex))
				{
					pstSubReq->pstNextNode = OSAL_Malloc(sizeof(stWrFileSubReq_t));
					if(NULL == pstSubReq->pstNextNode)
					{
						u8ReturnType = STACK_ERROR_MALLOC_FAILED;
						return u8ReturnType;
					}
					pstSubReq = pstSubReq->pstNextNode;
					memset(pstSubReq,00,sizeof(stWrFileSubReq_t));
					pstSubReq->pstNextNode = NULL;
				}
				else
				{
					break;
				}
			}
		}
		break;

		case READ_DEVICE_IDENTIFICATION :
		{
			stRdDevIdResp_t	*pstMbusRdDevIdResp = NULL;
			SubObjList_t		*pstObjList = NULL;
			uint8_t u8NumObj = 0;
			bool bIsFirstObjflag = true;

			pstMbusRdDevIdResp = OSAL_Malloc(sizeof(stRdDevIdResp_t));
			if(NULL == pstMbusRdDevIdResp)
			{
				u8ReturnType = STACK_ERROR_MALLOC_FAILED;
				return u8ReturnType;
			}
			// Initialize allocated memory with 0
			memset(pstMbusRdDevIdResp,00,sizeof(stRdDevIdResp_t));
			pstMBusRequesPacket->m_stMbusRxData.m_pvAdditionalData = pstMbusRdDevIdResp;

			pstMbusRdDevIdResp->m_u8MEIType = ServerReplyBuff[u16BuffInex++];
			pstMbusRdDevIdResp->m_u8RdDevIDCode = ServerReplyBuff[u16BuffInex++];
			pstMbusRdDevIdResp->m_u8ConformityLevel = ServerReplyBuff[u16BuffInex++];
			pstMbusRdDevIdResp->m_u8MoreFollows = ServerReplyBuff[u16BuffInex++];
			pstMbusRdDevIdResp->m_u8NextObjId = ServerReplyBuff[u16BuffInex++];
			pstMbusRdDevIdResp->m_u8NumberofObjects = ServerReplyBuff[u16BuffInex++];

			u8NumObj = pstMbusRdDevIdResp->m_u8NumberofObjects;
			pstObjList = &(pstMbusRdDevIdResp->m_pstSubObjList);
			u16TempBuffInex = u16BuffInex;
			while(1)
			{
				pstObjList->m_u8ObjectID = ServerReplyBuff[u16BuffInex++];
				pstObjList->m_u8ObjectLen = ServerReplyBuff[u16BuffInex++];
				pstObjList->m_u8ObjectValue = OSAL_Malloc(sizeof(uint8_t) * pstObjList->m_u8ObjectLen);
				if(NULL == pstObjList->m_u8ObjectValue)
				{
					u8ReturnType = STACK_ERROR_MALLOC_FAILED;
					return u8ReturnType;
				}
				memcpy_s(pstObjList->m_u8ObjectValue,
						(sizeof(uint8_t)*pstObjList->m_u8ObjectLen),
						&ServerReplyBuff[u16BuffInex++],
						pstObjList->m_u8ObjectLen);

				if(0 == pstMbusRdDevIdResp->m_u8MoreFollows && bIsFirstObjflag == true)
				{
					u8NumObj = pstMbusRdDevIdResp->m_u8NumberofObjects -
							pstMbusRdDevIdResp->m_u8NextObjId;
					bIsFirstObjflag = false;
				}
				// next object ID only if more follows
				else if(0xFF == pstMbusRdDevIdResp->m_u8MoreFollows && bIsFirstObjflag == true)
				{
					u8NumObj = pstMbusRdDevIdResp->m_u8NumberofObjects;
					bIsFirstObjflag = false;
				}

				if(u8NumObj > 0)
				{
					u8NumObj = u8NumObj - 1;
				}

				if(u8NumObj)
				{
					u16BuffInex = u16BuffInex + (pstObjList->m_u8ObjectLen - 1);
					pstObjList->pstNextNode = OSAL_Malloc(sizeof(SubObjList_t));
					if(NULL == pstObjList->pstNextNode)
					{
						u8ReturnType = STACK_ERROR_MALLOC_FAILED;
						return u8ReturnType;
					}
					pstObjList = pstObjList->pstNextNode;
					pstObjList->pstNextNode = NULL;
				}
				else
				{
					break;
				}
			}
		}
		break;
		}
	}
	return u8ReturnType;
} // End of DecodeRxMBusPDU

/**
 * @fn uint8_t DecodeRxPacket(uint8_t *ServerReplyBuff,stMbusPacketVariables_t *pstMBusRequesPacket)
 *
 * @brief This function decodes the data received from a Modbus response and
 * Fills up the details in a structure of type pstMBusRequesPacket.
 * This function starts decoding the information from 0th index from
 * response buffer. This function also checks if it has received correct response
 * with transaction ID same as that of the request from pstMBusRequesPacket structure.
 *
 * @param ServerReplyBuff 		[in] uint8_t* Pointer to input buffer received from Modbus response
 * @param pstMBusRequesPacket 	[in] stMbusPacketVariables_t* Pointer to Modbus request packet
 * 									 structure to store decoded data
 *
 * @return uint8_t [out] STACK_NO_ERROR in case of successful decoding of the response;
 * 						 STACK_ERROR_MALLOC_FAILED in case of error;
 * 						 STACK_TXNID_OR_UNITID_MISSMATCH in case of transaction/ request
 * 						 ID mismatches with the request ID from pstMBusRequesPacket structure
 *
 *
 */
uint8_t DecodeRxPacket(uint8_t *ServerReplyBuff,stMbusPacketVariables_t *pstMBusRequesPacket)
{
	uint8_t u8ReturnType = STACK_NO_ERROR;
	uint16_t u16BuffInex = 0;

#ifdef MODBUS_STACK_TCPIP_ENABLED
	uByteOrder_t ustByteOrder = {0};

	// Holds the received transaction ID
	uint16_t u16TransactionID = 0;
#endif
	// Holds the unit id
	uint8_t  u8UnitID = 0;
	// Holds the function code
	uint8_t u8FunctionCode = 0;

#ifdef MODBUS_STACK_TCPIP_ENABLED

	// Transaction ID
	ustByteOrder.u16Word = 0;
	ustByteOrder.TwoByte.u8ByteTwo = ServerReplyBuff[u16BuffInex++];
	ustByteOrder.TwoByte.u8ByteOne = ServerReplyBuff[u16BuffInex++];
	u16TransactionID = ustByteOrder.u16Word;

	// Protocol ID
	ustByteOrder.TwoByte.u8ByteTwo = ServerReplyBuff[u16BuffInex++];
	ustByteOrder.TwoByte.u8ByteOne = ServerReplyBuff[u16BuffInex++];

	ustByteOrder.u16Word = 0;
	// Length
	ustByteOrder.TwoByte.u8ByteTwo = ServerReplyBuff[u16BuffInex++];
	ustByteOrder.TwoByte.u8ByteOne = ServerReplyBuff[u16BuffInex++];
#endif

	u8UnitID = ServerReplyBuff[u16BuffInex++];

	u8FunctionCode = ServerReplyBuff[u16BuffInex++];

	// Init resp received timestamp
	//timespec_get(&(pstMBusRequesPacket->m_objTimeStamps.tsRespRcvd), TIME_UTC);

#ifdef MODBUS_STACK_TCPIP_ENABLED
	if((pstMBusRequesPacket->m_u16TransactionID != u16TransactionID) ||
			(pstMBusRequesPacket->m_u8UnitID != u8UnitID) ||
			(pstMBusRequesPacket->m_u8FunctionCode != (u8FunctionCode & 0x7F)))
#else
		if((pstMBusRequesPacket->m_u8ReceivedDestination != u8UnitID)
				||(pstMBusRequesPacket->m_u8FunctionCode != (u8FunctionCode & 0x7F)))
#endif
		{
			u8ReturnType = STACK_TXNID_OR_UNITID_MISSMATCH;
		}
		else
		{
			pstMBusRequesPacket->m_u8FunctionCode = u8FunctionCode;
			u8ReturnType = DecodeRxMBusPDU(ServerReplyBuff,u16BuffInex, pstMBusRequesPacket);
		}
	return u8ReturnType;
}

#ifndef MODBUS_STACK_TCPIP_ENABLED
/**
 * @fn int checkforblockingread(int fd, long a_lRespTimeout)
 *
 * @brief This function adds socket descriptor in select() to check for blocking read calls.
 * The socket is read from Modbus stack configuration.
 *
 *@param fd						[in] file descriptor
 *@param a_lRespTimeout			[in] response timeout used in case of request timeout
 * @return uint8_t [out] 0 in case of function fails and sets ERRNO to error,
 * 						 Non-zero integer which is equal to the number of ready descriptors.
 *
 *
 */
int checkforblockingread(int fd, long a_lRespTimeout)
{
	fd_set rset;
	struct timeval tv;
	//wait upto 1 seconds
	tv.tv_sec = 0;
	tv.tv_usec = a_lRespTimeout;
	//g_stModbusDevConfig.m_lResponseTimeout;
	FD_ZERO(&rset);

	// If fd is not set correctly.
	if(fd < 0)
	{
		return 0;
	}
	FD_SET(fd, &rset);

	return select(fd+1,&rset,NULL,NULL,&tv);
}


/**
 * @fn uint8_t Modbus_SendPacket(stMbusPacketVariables_t *pstMBusRequesPacket,
		stRTUConnectionData_t rtuConnectionData,
		long a_lInterframeDelay,
		long a_lRespTimeout)
 *
 * @brief This function sends request to Modbus slave device using RTU communication mode. The function
 * prepares CRC depending on the request to send and buffer in which to receive response from
 * the Modbus slave device. It then fills up the transaction data, then sleeps for the nano-seconds
 * of frame delay. After sleep, it writes the request on the socket to Modbus slave device. Function
 * then captures the current time as time stamp when request was sent on the Modbus slave device.
 * Then it waits for the select() to notify about the incoming response. Depending on the function code
 * (operation to perform on Modbus slave device), it reads data from the socket descriptor. Once the
 * complete response is read, function gets the current time as time stamp when response is received.
 * After this, it decodes the response received from Modbus slave device and fills up appropriate
 * structures to send the response to ModbusApp.
 *
 * @param pstMBusRequesPacket 	[in] stMbusPacketVariables_t * pointer to structure containing
 * 								   	 request for Modbus slave device
 * @param rtuConnectionData 	[in] stRTUConnectionData_t structure containing the fd and interframe delay
 *
 *@param a_lInterframeDelay		[in] interframe delay apart from standard baudrate
 *@param a_lRespTimeout			[in] response timeout used in case of request timeout
 *
 * @return uint8_t 			[out] STACK_NO_ERROR in case of success;
 * 								  STACK_ERROR_SEND_FAILED if function fails to send the request
 * 								  to Modbus slave device
 * 								  STACK_ERROR_RECV_TIMEOUT if select() fails to notify of incoming
 * 								  response
 *								  STACK_ERROR_RECV_FAILED in case if function fails to read
 *								  data from socket descriptor
 *
 */
uint8_t Modbus_SendPacket(stMbusPacketVariables_t *pstMBusRequesPacket,
		stRTUConnectionData_t rtuConnectionData,
		long a_lInterframeDelay,
		long a_lRespTimeout)
{
	uint8_t u8ReturnType = STACK_NO_ERROR;
	uint8_t recvBuff[TCP_MODBUS_ADU_LENGTH];
	volatile int bytes = 0;
	uint16_t crc;
	uint8_t ServerReplyBuff[TCP_MODBUS_ADU_LENGTH] = {0};
	int totalRead = 0;
	uint16_t numToRead = 0;
	int iBlockingReadResult = 0;

	if(NULL == pstMBusRequesPacket)
	{
		u8ReturnType = STACK_ERROR_SEND_FAILED;
		return u8ReturnType;
	}

	memset(recvBuff, '0',sizeof(recvBuff));
	memcpy_s(recvBuff,sizeof(pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields),
			pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields,
			pstMBusRequesPacket->m_stMbusTxData.m_u16Length);

	{
		crc = crc16(recvBuff,pstMBusRequesPacket->m_stMbusTxData.m_u16Length);

		recvBuff[pstMBusRequesPacket->m_stMbusTxData.m_u16Length++] = (crc & 0xFF00) >> 8;
		recvBuff[pstMBusRequesPacket->m_stMbusTxData.m_u16Length++] = (crc & 0x00FF);

		tcflush(rtuConnectionData.m_fd, TCIOFLUSH);

		// Multiple Slave issue: Adding Frame delay between two packets 
		sleep_micros(a_lInterframeDelay + rtuConnectionData.m_interframeDelay);

		bytes = write(rtuConnectionData.m_fd,recvBuff,(pstMBusRequesPacket->m_stMbusTxData.m_u16Length));
		if(bytes <= 0)
		{
			u8ReturnType = STACK_ERROR_SEND_FAILED;
			pstMBusRequesPacket->m_u8CommandStatus = u8ReturnType;
			return u8ReturnType;
		}

		// Init req sent timestamp
		timespec_get(&(pstMBusRequesPacket->m_objTimeStamps.tsReqSent), TIME_UTC);

		//< Note: Below framing delay is commented intentionally as read function is having blocking read() call in which
		//< select() function is having response waiting time of 1000 mSec as well as it is having blocking read call.
		//< This will not allow to start new message unless mess is received from slave within 1000mSec
		//< Please remove below commented framing delay when nonblocking call is used.
		/*if(baud > 19200)
		{
			//usleep(1750);
			sleep_micros(1750);
		}
		else
		{
			//usleep(3700);
			sleep_micros(3700);
		}*/

		// Frame structure:
		// Sample response - success
		// Slave-ID (1 byte) + Function Code (1 byte) + Length (1 byte) + Data (Length bytes) + CRC (2 bytes)
		// E.g. 0A 03 04 00 00 00 00 40 F3
		// Here, 04 is length
		// Sample response - error
		// Slave-ID (1 byte) + Function Code (1 byte || 0x80) + Exception code (1 byte) + CRC (2 bytes)
		// E.g. 0A 81 02 B0 53
		
		// Steps: 
		// 1. Read 3 bytes
		// 2. Check if 2nd byte has 0x80. If yes, it means it is exception packet. If no, it is data packet
		// 3. For exception packet, read 2 more bytes for CRC
		// 4. For data packet, read data based on length (3rd byte) + 2 bytes for CRC
		
		// 1. Read 3 bytes: slave id (1) + function code (1) + length or exception code (1)
		numToRead = 3;

		bytes = 0;
		// Creating exception flag and setting it to false to check for to handle the msg with exception
		bool expFlag = false;

		int enStep = 1;
		while(numToRead > 0){
			iBlockingReadResult = checkforblockingread(rtuConnectionData.m_fd, a_lRespTimeout);
			if(iBlockingReadResult > 0)
			{
				bytes = read(rtuConnectionData.m_fd, &ServerReplyBuff[totalRead], numToRead);
				totalRead += bytes;
				numToRead -= bytes;

				if(bytes == 0){
					break;
				}
				
				// Before changing the state from "reading header' to "reading data", ensure that header is read completely.
				// This check is important to ensure errors when 3 bytes of header are not read in one go.
				if((2 < totalRead) && (1 == enStep))
				{
					// check if it is exception packet
					if((ServerReplyBuff[EXP_POS] & EXP_VAL) == EXP_VAL)
					{
						// Read CRC byets
						numToRead = 2;
						expFlag = true;
						enStep = 2;
					}
					else
					{
						// This is data packet. Read length which is 3rd byte
						// Set bytes to read + CRC bytes
						numToRead = ServerReplyBuff[2] + 2;
						enStep = 2;
					}
				}
			}
			else
			{
				break;
			}
		}

		// Timestamp for resp received
		timespec_get(&(pstMBusRequesPacket->m_objTimeStamps.tsRespRcvd), TIME_UTC);

		if(totalRead > 0)
		{
			memcpy_s(pstMBusRequesPacket->m_u8RawResp, sizeof(pstMBusRequesPacket->m_u8RawResp),
					ServerReplyBuff, sizeof(ServerReplyBuff));

			pstMBusRequesPacket->m_state = RESP_RCVD_FROM_NETWORK;
			addToRespQ(pstMBusRequesPacket);
			//u8ReturnType = DecodeRxPacket(ServerReplyBuff,pstMBusRequesPacket);
		}
		else // totalRead = 0, means request timed out or recv failed
		{
			// If blocking result = 0, it means timeout has occurred
			if (0 == iBlockingReadResult)
			{
				pstMBusRequesPacket->m_state = RESP_TIMEDOUT;
				u8ReturnType = STACK_ERROR_RECV_TIMEOUT;
			}
			else
			{
				pstMBusRequesPacket->m_state = RESP_ERROR;
				u8ReturnType = STACK_ERROR_RECV_FAILED;
			}
		}
	}

	pstMBusRequesPacket->m_u8CommandStatus = u8ReturnType;
	return u8ReturnType;
}

/**
 * @fn MODBUS_STACK_EXPORT int initSerialPort(stRTUConnectionData_t* pstRTUConnectionData,
										uint8_t *portName,
										uint32_t baudrate,
										eParity  parity)
 *
 * @brief This function initiates a serial port according to baud rate for a Modbus slave
 * communicating using RTU mode.
 *
 * @param pstRTUConnectionData 	[out] stRTUConnectionData_t* Structure containing fd and interfram delay. These values wil
 * 										be created and assigned here based on port name and baudrate.
 * @param portName 				[in] uint8_t serial port number to initialize for specified baud rate
 * @param baudrate 				[in] uint32_t baud rate to compare with the Modbus slave device's baud rate
 * @param parity 				[in] uint8_t parity to set for this Modbus slave device communication
 *
 * @return int 					[out] -1 if function fails to open the specified port or fails to set
 * 										the baud rate for the port or fails to set attributes of the port;
 * 						  				Non-zero file descriptor integer in case if function succeeds to
 * 						  				initiate and set configuration of the port.
 *
 */
MODBUS_STACK_EXPORT int initSerialPort(stRTUConnectionData_t* pstRTUConnectionData,
										uint8_t *portName,
										uint32_t baudrate,
										eParity  parity)
{
	struct termios tios;
	speed_t speed;
	int flags;
	
	/** 
		O_NOCTTY - This flag tells UNIX that this program doesn't want
       to be the "controlling terminal" for that port. if this flag is not set then any input (such as keyboard abort
       signals and so forth) will affect the process
	   
	   NDELAY - Timeouts are ignored in canonical input mode or when the
       NDELAY option is set on the file via open or fcntl

    */

	memset(&tios, 0, sizeof(struct termios));
	flags = O_RDWR | O_NOCTTY | O_NDELAY | O_EXCL;

	pstRTUConnectionData->m_fd = open((const char*)portName, flags);

	if (pstRTUConnectionData->m_fd == -1) {
		return -1;
	}

	
	// set the attributes
	tcgetattr(pstRTUConnectionData->m_fd, &tios);

	// set the interframe delay as per baudrate
	switch (baudrate) {
	case 110:
		speed = B110;
		pstRTUConnectionData->m_interframeDelay = 38500000/baudrate;
		break;
	case 300:
		speed = B300;
		pstRTUConnectionData->m_interframeDelay = 38500000/baudrate;
		break;
	case 600:
		speed = B600;
		pstRTUConnectionData->m_interframeDelay = 38500000/baudrate;
		break;
	case 1200:
		speed = B1200;
		pstRTUConnectionData->m_interframeDelay = 38500000/baudrate;
		break;
	case 2400:
		speed = B2400;
		pstRTUConnectionData->m_interframeDelay = 38500000/baudrate;
		break;
	case 4800:
		speed = B4800;
		pstRTUConnectionData->m_interframeDelay = 38500000/baudrate;
		break;
	case 9600:
		speed = B9600;
		pstRTUConnectionData->m_interframeDelay = 38500000/baudrate;
		break;
	case 19200:
		speed = B19200;
		pstRTUConnectionData->m_interframeDelay = 38500000/baudrate;
		break;
	case 38400:
		speed = B38400;
		pstRTUConnectionData->m_interframeDelay = 1750;
		break;
#ifdef B57600
	case 57600:
		speed = B57600;
		pstRTUConnectionData->m_interframeDelay = 1750;
		break;
#endif
#ifdef B115200
	case 115200:
		speed = B115200;
		pstRTUConnectionData->m_interframeDelay = 1750;
		break;
#endif
#ifdef B230400
	case 230400:
		speed = B230400;
		pstRTUConnectionData->m_interframeDelay = 1750;
		break;
#endif
#ifdef B460800
	case 460800:
		speed = B460800;
		pstRTUConnectionData->m_interframeDelay = 1750;
		break;
#endif
#ifdef B500000
	case 500000:
		speed = B500000;
		pstRTUConnectionData->m_interframeDelay = 1750;
		break;
#endif
#ifdef B576000
	case 576000:
		speed = B576000;
		pstRTUConnectionData->m_interframeDelay = 1750;
		break;
#endif
#ifdef B921600
	case 921600:
		speed = B921600;
		pstRTUConnectionData->m_interframeDelay = 1750;
		break;
#endif
#ifdef B1000000
	case 1000000:
		speed = B1000000;
		pstRTUConnectionData->m_interframeDelay = 1750;
		break;
#endif
#ifdef B1152000
	case 1152000:
		speed = B1152000;
		pstRTUConnectionData->m_interframeDelay = 1750;
		break;
#endif
#ifdef B1500000
	case 1500000:
		speed = B1500000;
		pstRTUConnectionData->m_interframeDelay = 1750;
		break;
#endif
#ifdef B2500000
	case 2500000:
		speed = B2500000;
		pstRTUConnectionData->m_interframeDelay = 1750;
		break;
#endif
#ifdef B3000000
	case 3000000:
		speed = B3000000;
		pstRTUConnectionData->m_interframeDelay = 1750;
		break;
#endif
#ifdef B3500000
	case 3500000:
		speed = B3500000;
		pstRTUConnectionData->m_interframeDelay = 1750;
		break;
#endif
#ifdef B4000000
	case 4000000:
		speed = B4000000;
		pstRTUConnectionData->m_interframeDelay = 1750;
		break;
#endif
	default:
		speed = B9600;
		pstRTUConnectionData->m_interframeDelay = 38500000/9600;
		printf("ERROR Unknown baud rate %d for %s (B9600 used)\n",
				baudrate, portName);
	}

	// Set the baud rate
	if ((cfsetispeed(&tios, speed) < 0) ||
			(cfsetospeed(&tios, speed) < 0)) {
		close(pstRTUConnectionData->m_fd);
		pstRTUConnectionData->m_fd = -1;
		return -1;
	}

	/** C_CFLAG      Control options for RTU communication
        CLOCAL       Local line
        CREAD        Enable read receiver
	    CSIZE        Bit mask for data bits . (used for hardware flow control).
	*/
	tios.c_cflag |= (CREAD | CLOCAL);
	tios.c_cflag &= ~CSIZE;
	tios.c_cflag |= CS8;


	/** PARENB       Flag used to enable parity bit
        PARODD       Flag to use ODD parity */

	// execute this if parity is None
	if (parity == eNone) 
	{
		
		// Disabled both the parities . (i.e. even and odd)
		tios.c_cflag &= ~(PARENB | PARODD); 

		// When parity is none, add a stop bit at the place of parity to make 11 characters i.e. two stop bits
		tios.c_cflag |= CSTOPB; 

	}
	
	// execute this if parity is EVEN
	else if (parity == eEven) 
	{
		tios.c_cflag |= PARENB;

		tios.c_cflag &= ~PARODD;

		tios.c_cflag &= ~CSTOPB;

	}
	// execute this if parity is ODD	
	else 
	{

		tios.c_cflag |= PARENB;
		tios.c_cflag |= PARODD;
		tios.c_cflag &= ~CSTOPB;
	}

	// raw input 
	tios.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	/** C_IFLAG      Available input options

       Constant/FLAG     Description/Use case
       INPCK        Enable parity check in communication
       IGNPAR       Ignore parity errors if any
       PARMRK       Mark parity errors if any
       ISTRIP       Strip parity bits from parity mechanism
       IXON 		enable software flow control (outgoing)
       IXOFF        enable software flow control (incoming)
       IXANY        Allow any character to start flow again in RTU
       IGNBRK       Ignore break condition if occurred
       BRKINT       Send a SIGINT when a break condition is detected in communication
       INLCR        map NL to CR
       IGNCR        ignore CR if required
       ICRNL        map CR into NL
       IUCLC        map the uppercase to lowercase
       IMAXBEL      Echo BEL on input line too long
	 */
	 
	// set the bits as per priority
	if (parity == eNone) 
	{
		tios.c_iflag &= ~INPCK;
	} 
	else 
	{
		tios.c_iflag |= INPCK;
	}

	tios.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

	// disabled the software flow control
	tios.c_iflag &= ~(IXON | IXOFF | IXANY);

	/** C_OFLAG      required Output options
       OPOST        postprocess output (if not set = raw output)
       ONLCR        map NL to CR-NL
       ONCLR ant others needs if OPOST to be enabled
	 */

	tios.c_oflag &=~ OPOST;

	/** C_CC         Control characters for data control 
       VMIN         VMIN specifies the minimum number of characters to read
       VTIME        Time to wait for data (in tenths of seconds)

       VTIME specifies the amount of time to wait for incoming
       characters in tenths of seconds. If VTIME is set to zero (0) (the
       default) then reads will block (wait) indefinitely unless the
       NDELAY option is set on the port with open or fcntl.
	 */
	 
	// will be ignored because we use open with the NDELAY option
	tios.c_cc[VMIN] = 0;
	tios.c_cc[VTIME] = 0;

	if (tcsetattr(pstRTUConnectionData->m_fd, TCSANOW, &tios) < 0) {
		close(pstRTUConnectionData->m_fd);
		pstRTUConnectionData->m_fd = -1;
		return -1;
	}
	return pstRTUConnectionData->m_fd;
}

#endif

// when Modbus stack is using TCP mode to communicate with Modbus slave device
#ifdef MODBUS_STACK_TCPIP_ENABLED
/**
 * @fn void closeConnection(IP_Connect_t *a_pstIPConnect)
 * @brief This function closes the specified socket connection and resets the structure.
 *
 * @param a_pstIPConnect [in] IP_Connect_t* pointer to struct of socket to close the connection and reset
 *
 * @return 				[out] none
 *
 */
void closeConnection(IP_Connect_t *a_pstIPConnect)
{
	if(NULL != a_pstIPConnect)
	{
		// Close socket
		close(a_pstIPConnect->m_sockfd);
		a_pstIPConnect->m_sockfd = 0;
		a_pstIPConnect->m_retryCount = 0;
		a_pstIPConnect->m_lastConnectStatus = SOCK_CONNECT_FAILED;
		a_pstIPConnect->m_bIsAddedToEPoll = false;
		a_pstIPConnect->m_iRcvConRef = -1;
	}
}

/**
 * @fn void Mark_Sock_Fail(IP_Connect_t *a_pstIPConnect)
 *
 * @brief This function sets the specified socket's failure state. The function removes the
 * socket descriptor entry from the epoll file descriptors (which was registered for
 * notification of events on it). The function then closes the socket connection and
 * resets the socket structure.
 *
 * @param a_pstIPConnect [in] IP_Connect_t* pointer to struct of socket to mark as fail
 *
 * @return 				 [out] none
 *
 */
void Mark_Sock_Fail(IP_Connect_t *a_pstIPConnect)
{
	if(NULL != a_pstIPConnect)
	{
		removeEPollRef(a_pstIPConnect->m_iRcvConRef);
		closeConnection(a_pstIPConnect);
	}
}

/**
 * @fn uint8_t Modbus_SendPacket(stMbusPacketVariables_t *pstMBusRequesPacket, IP_Connect_t *a_pstIPConnect)
 *
 * @brief This function sends request to Modbus slave device using TCP communication mode. It then fills
 * up the transaction data. It checks if socket already exists. If socket exists, it sends
 * the request on the socket to Modbus slave device.
 * If the socket does not exist, function creates a socket, tries to connect with the socket and
 * then sends the request on it.
 * Upon successfully sending the request to Modbus slave device, function captures the current
 * time as time stamp when request was sent on the Modbus slave device and adds it to the request structure.
 * Then it adds this request in request manager's list for further watch.
 *
 * @param pstMBusRequesPacket 	[in] stMbusPacketVariables_t * pointer to structure containing
 * 								   	 request for Modbus slave device
 * @param a_pstIPConnect 		[in] IP_Connect_t* pointer to structure of socket descriptor to
 * 									 connect with Modbus slave device on which to send this request
 *
 * @return uint8_t 			[out] 0 in case of success;
 * 								  STACK_ERROR_SEND_FAILED if function fails to send the request
 * 								  to Modbus slave device
 * 								  STACK_ERROR_SOCKET_FAILED if function fails to create a new socket
 * 								  to communicate with the Modbus slave device
 *								  STACK_ERROR_CONNECT_FAILED in case if function fails to connect
 *								  with from socket descriptor
 *								  STACK_ERROR_FAILED_Q_SENT_REQ if function fails to add sent request
 *								  in request manager's list
 *
 */
uint8_t Modbus_SendPacket(stMbusPacketVariables_t *pstMBusRequesPacket, IP_Connect_t *a_pstIPConnect)
{
	//local variables
	int32_t sockfd = 0;
	uint8_t u8ReturnType = STACK_NO_ERROR;
	uint8_t recvBuff[260];
	long arg;
	int res = 0;

	if(NULL == pstMBusRequesPacket || NULL == a_pstIPConnect)
	{
		u8ReturnType = STACK_ERROR_SEND_FAILED;
		return u8ReturnType;
	}

	memset(recvBuff, '0',sizeof(recvBuff));
	memcpy_s(recvBuff,sizeof(pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields),
			pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields,
			pstMBusRequesPacket->m_stMbusTxData.m_u16Length);

	// There are following cases to be handled:
	// 1. Socket is not created. Here socket = 0
	// 2. Socket is created and connection is in progress. Here connectionstatus = EINPROGRESS
	// 3. Connection is established

	do
	{
		// Case 1: Socket is not created
		if(0 == a_pstIPConnect->m_sockfd)
		{
			a_pstIPConnect->m_lastConnectStatus = SOCK_NOT_CONNECTED;
			a_pstIPConnect->m_bIsAddedToEPoll = false;
			a_pstIPConnect->m_retryCount = 0;
			if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			{
				u8ReturnType = STACK_ERROR_SOCKET_FAILED;
				//printf("Socket creation failed !! error ::%d\n", errno);
				break;
			}

			a_pstIPConnect->m_sockfd = sockfd;
			{
				int iEnable = 1;
				if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &iEnable, sizeof(int)) < 0)
				{
					printf("setsockopt(TCP_NODELAY) failed ::%d\n", errno);
				}

				if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &iEnable, sizeof(int)) < 0)
				{
					printf("setsockopt(SO_REUSEADDR) failed ::%d\n", errno);
				}
			}
		}
		sockfd = a_pstIPConnect->m_sockfd;
		if((SOCK_NOT_CONNECTED == a_pstIPConnect->m_lastConnectStatus) &&
				(0 != a_pstIPConnect->m_sockfd))
		{
			a_pstIPConnect->m_retryCount = 0;

			arg = fcntl(sockfd, F_GETFL, NULL);
			arg |= O_NONBLOCK;
			fcntl(sockfd, F_SETFL, arg);

			res = connect(sockfd, (struct sockaddr *)&a_pstIPConnect->m_servAddr, sizeof(a_pstIPConnect->m_servAddr));

			if (res < 0)
			{
				if (errno == EINPROGRESS)
				{
					a_pstIPConnect->m_lastConnectStatus = SOCK_CONNECT_INPROGRESS;
					//printf("\nConnection with Modbus Server is in progress ...\n");
				}
				else
				{
					u8ReturnType = STACK_ERROR_CONNECT_FAILED;
					// closing socket on error.
					Mark_Sock_Fail(a_pstIPConnect);
					//printf("Connection with Modbus slave failed, so closing socket descriptor %d\n", sockfd);
					break;
				}
			}
			else if(res == 0)
			{
				a_pstIPConnect->m_lastConnectStatus = SOCK_CONNECT_SUCCESS;
				//printf("Modbus slave connection established on socket %d\n", sockfd);
				//socket has been created and connected successfully, add it to epoll fd

				int res = send(sockfd, recvBuff, (pstMBusRequesPacket->m_stMbusTxData.m_u16Length), MSG_NOSIGNAL);

				if(res < 0)
				//in order to avoid application stop whenever SIGPIPE gets generated,used send function with MSG_NOSIGNAL argument
				{
					//printf("1. Closing socket %d as error occurred while sending data\n", sockfd);
					u8ReturnType = STACK_ERROR_SEND_FAILED;
					Mark_Sock_Fail(a_pstIPConnect);
					break;
				}
			}
		}
		if((SOCK_CONNECT_INPROGRESS == a_pstIPConnect->m_lastConnectStatus) &&
				(0 != a_pstIPConnect->m_sockfd))
		{

			a_pstIPConnect->m_retryCount++;
			if(a_pstIPConnect->m_retryCount > MAX_RETRY_COUNT)
			{
				//printf("Connect status INPROGRESS. Max retries done %d\n", a_pstIPConnect->m_sockfd);
				Mark_Sock_Fail(a_pstIPConnect);
				//bReturnVal = false;
				u8ReturnType = STACK_ERROR_CONNECT_FAILED;
				break;
			}
			struct timeval tv;
			fd_set myset;
			tv.tv_sec = 0;
			tv.tv_usec = g_stModbusDevConfig.m_lResponseTimeout;
			FD_ZERO(&myset);
			FD_SET(sockfd, &myset);
			int r1 = select(sockfd+1, NULL, &myset, NULL, &tv);
			if (r1 > 0)
			{
				socklen_t lon = sizeof(int);
				int valopt;
				getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon);
				if (valopt)
				{
					//printf("getsockopt failed : %d", valopt);
					u8ReturnType = STACK_ERROR_CONNECT_FAILED;
					Mark_Sock_Fail(a_pstIPConnect);
					break;
				}
				else
				{
					printf("getsockopt passed\n");
				}
			}
			else if(r1 <= 0)
			{
				//printf("select failed : %d", errno);
				u8ReturnType = STACK_ERROR_CONNECT_FAILED;
				Mark_Sock_Fail(a_pstIPConnect);
				break;
			}
		}

		if((false == a_pstIPConnect->m_bIsAddedToEPoll) && (0 != a_pstIPConnect->m_sockfd)
				&& (-1 != a_pstIPConnect->m_sockfd))
		{
			if(0 > addtoEPollList(a_pstIPConnect))
			{
				u8ReturnType = STACK_ERROR_SOCKET_LISTEN_FAILED;
				Mark_Sock_Fail(a_pstIPConnect);
				break;
			}
			printf("Added to listen \n");
			a_pstIPConnect->m_bIsAddedToEPoll = true;
		}

		// forcefully sleep for 50ms to complete previous send request
		// This is to match the speed between master and slave
		//usleep(g_lInterframeDelay);
		sleep_micros(g_stModbusDevConfig.m_lInterframedelay);

		int res = send(sockfd, recvBuff, (pstMBusRequesPacket->m_stMbusTxData.m_u16Length), MSG_NOSIGNAL);

		// in order to avoid application stop whenever SIGPIPE gets generated,used send function with MSG_NOSIGNAL argument
		if(res < 0)
		{
			//printf("Error %d occurred while sending request on %d closing the socket\n", errno, sockfd);
			u8ReturnType = STACK_ERROR_SEND_FAILED;
			Mark_Sock_Fail(a_pstIPConnect);
			break;
		}
		a_pstIPConnect->m_lastConnectStatus = SOCK_CONNECT_SUCCESS;

		// Init req sent timestamp
		timespec_get(&(pstMBusRequesPacket->m_objTimeStamps.tsReqSent), TIME_UTC);
		if(0 != addReqToList(pstMBusRequesPacket))
		{
			printf("Unable to add request for timeout tracking. marking it as failed.\n");
			u8ReturnType = STACK_ERROR_FAILED_Q_SENT_REQ;
			break;
		}

	} while(0);

	pstMBusRequesPacket->m_u8CommandStatus = u8ReturnType;

	return u8ReturnType;
}
#endif
