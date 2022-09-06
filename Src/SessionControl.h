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

#ifndef INC_SESSIONCONTROL_H_
#define INC_SESSIONCONTROL_H_
#include "StackConfig.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h> // for epoll_create1(), epoll_ctl(), struct epoll_event

// Function declarations

/**
 *
 * Description
 * Initialize Response list data
 *
 * @param none
 * @return int [out] 0 (if success)
 * 					-1 (if failure)
 */
int initRespStructs();

/**
 *
 * Description
 * De-initialize data structures related to response processing.
 *
 * @param none
 * @return Nothing
 */
void deinitRespStructs();

/**
 *
 * Description
 * Add msg to response queue for TCP and RTU
 *
 * @param a_pstReq [in] pointer to struct of type stMbusPacketVariables_t
 * @return void [out] none
 */
void addToRespQ(stMbusPacketVariables_t *a_pstReq);

/**
 *
 * Description
 * Session control thread function
 *
 * @param threadArg [in] thread argument
 * @return void 	[out] nothing
 */
void* SessionControlThread(void* threadArg);

/**
 * Description
 * Function to decode received modbus data
 *
 * @param ServerReplyBuff [in] Input buffer
 * @param pstMBusRequesPacket [in] Request packet
 * @return uint8_t [out] respective error codes
 *
 */
uint8_t DecodeRxPacket(uint8_t *ServerReplyBuff,
		stMbusPacketVariables_t *pstMBusRequesPacket);

/**
 enum eClientSessionStatus
 @brief
    This enum defines status of client request
*/
typedef enum
{
	CLIENTSESSION_REQ_SENT,
	CLIENTSESSION_RES_REC,
	CLIENTSESSION_REQ_TIMEOUT,
	CLIENTSESSION_CALLBACK,
	CLIENTSESSION_FREE
}eClientSessionStatus;

/**
 @struct LiveSerSessionList
 @brief
    This structure defines list of session
*/
typedef struct LiveSerSessionList
{
	uint8_t m_Index;			// Index of the list
	Thread_H m_ThreadId;		// Thread ID
	int32_t MsgQId;
	eClientSessionStatus m_eCltSesStatus;  // session status
#ifdef MODBUS_STACK_TCPIP_ENABLED
	uint8_t m_u8IpAddr[4];				// IP Address
	uint16_t m_u16Port;					// TCPIP Port
	int32_t m_i32sockfd;				// Socket descriptor
	uint8_t m_u8UnitId;					// Slave ID
	int m_iLastConnectStatus;			// Connection status
	uint8_t m_u8ConnectAttempts;		// Connection attempts
	uint16_t m_u16TxID;					// Transmission ID
#else
	uint8_t m_u8ReceivedDestination;	// Receive destination
	uint8_t m_portName[256];			// Port name
	uint32_t m_baudrate;				//baudrate
	eParity m_parity;					// parity select
	long m_lInterframeDelay;			// Interframe delay
	long m_lrespTimeout;				// response timeout
#endif
	void *m_pNextElm;					// next list element
}stLiveSerSessionList_t;


#ifdef MODBUS_STACK_TCPIP_ENABLED

typedef struct mesg_data
{
	long mesg_type;									//Message type
	unsigned char m_readBuffer[MODBUS_DATA_LENGTH]; //Read buffer
}mesg_data_t;

typedef struct EpollTcpRecv
{
	uint8_t m_Index;							// Index
	Thread_H m_ThreadId;						// Thread ID
	int32_t MsgQId;								// Message ID
	eClientSessionStatus m_eCltSesStatus;		// TCPIP Session status
#ifdef MODBUS_STACK_TCPIP_ENABLED
	uint8_t m_u8IpAddr[4];						// IP Address
	uint16_t m_u16Port;							// TCPIP Port name
#else
	uint8_t m_u8ReceivedDestination;			// Receive destination
#endif
	void *m_pNextElm;							// next list element
}stEpollTcpRecv_t;

typedef struct TcpRecvData
{
	IP_Connect_t *m_pstConRef;				//pointer reference
	int m_len;								//TCPIP data length
	int m_bytesRead;						// bytes read
	int m_bytesToBeRead;					// No.Of bytes to be read
	unsigned char m_readBuffer[MODBUS_DATA_LENGTH];  // read buffer
}stTcpRecvData_t;

/**
 *
 * Description
 * Initializes data structures needed for epoll operation for receiving TCP data
 *
 * @param None
 * @return true/false based on status
 */
bool initEPollData();

/**
 *
 * Description
 * Add request to list
 *
 * @param pstMBusRequesPacket [in] pointer to struct of type stMbusPacketVariables_t
 * @return int 0 for success, -1 for error
 */
int addReqToList(stMbusPacketVariables_t *pstMBusRequesPacket);

/**
 *
 * Description
 * Add response to handle in a queue
 *
 * @param a_u8UnitID [in] uint8_t unit id from modbus header
 * @param a_u16TransactionID  [in] uint16_t transction id from modbus header
 * @return void [out] none
 */
void addToHandleRespQ(stTcpRecvData_t *a_pstReq);


//stMbusPacketVariables_t* searchReqList(uint8_t a_u8UnitID, uint16_t a_u16TransactionID);
/**
 *
 * Description
 * get time stamp in nano-seconds
 *
 * @param - none
 * @return unsigned long [out] time in nano-seconds
 */
unsigned long get_nanos(void);

/**
 * Description
 * Close the socket connection and reset the structure
 *
 * @param stIPConnect [in] pointer to struct of type IP_Connect_t
 *
 * @return void [out] none
 *
 */
void closeConnection(IP_Connect_t *a_pstIPConnect);

/**
 * Description
 * Set socket failure state
 *
 * @param stIPConnect [in] pointer to struct of type IP_Connect_t
 *
 * @return void [out] none
 *
 */
void Mark_Sock_Fail(IP_Connect_t *stIPConnect);

/**
 *
 * Description
 * TCP and callback thread function
 *
 * @param threadArg [in] thread argument
 * @return void pointer
 *
 */
void* ServerSessTcpAndCbThread(void* threadArg);

void deinitTimeoutTrackerArray();

/**
 *
 * Description
 * thread to start polling on sockets o receive data
 *
 * @param - none
 * @return void [out] none
 */
void* EpollRecvThread();

#endif

#endif /* INC_SESSIONCONTROL_H_ */
