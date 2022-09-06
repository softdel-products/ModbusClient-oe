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

#ifndef STACKCONFIG_H_
#define STACKCONFIG_H_

#include "osalLinux.h"
#include "semaphore.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h> // for epoll_create1(), epoll_ctl(), struct epoll_event

#define handle_error_en(en, msg) do { errno = en; perror(msg); } while (0)

// TCP specific macros
#ifdef MODBUS_STACK_TCPIP_ENABLED
	// TCP packet length
	// This is as the modbus standard
	#define TCP_MODBUS_ADU_LENGTH 260

	// This value is used in timeout thread for tracking
	// This value is used to add additional records to timeout tracker list
	#define ADDITIONAL_RECORDS_TIMEOUT_TRACKER 100

	// Multiplier value for request array
	#define REQ_ARRAY_MULTIPLIER 100

	// this value will specify maximum events to be register for epoll thread
	// this is used in epoll receiver thread
	#define MAXEVENTS 100

 	// epoll operation timeout
	// this is used in epoll receiver thread
	#define EPOLL_TIMEOUT 1000

// RTU specific macros
#else
	// RTU packet length
	// This is as the modbus standard
	#define TCP_MODBUS_ADU_LENGTH 256

	// Modbus serial data statndard length
	#define MODBUS_DATA_LENGTH (256)

	// RTU packet header length
	#define PKT_HDR_LEN 5

	// RTU packet exception length value
	#define PKT_EXP_LEN 2

	// RTU packet exception value
	#define EXP_VAL 0x80

	// RTU packet exception code position value
	#define EXP_POS 1
#endif

// common macros used for TCP and RTU

// this value is used to validate maximum length validation for env variables recvd from application
// This is added to fix KW issue
#define MAX_ENV_VAR_LEN 5000

// Enumerated value for modbus type exception
#define MODBUS_EXCEPTION 1

// Enumerated value used for stack errors
#define MODBUS_STACK_ERROR 2

// Response timeout (in milliseconds)value used by timeout thread
// This is used as a default when it is not provided by user in env
#define DEFAULT_RESPONSE_TIMEOUT_MS 80

// Interframe delay (in milliseconds) value used while sending request to end device
// This value is added for every request initiated by stack
// This is used as a default when it is not provided by user in env
#define DEFAULT_INTERFRAME_DELAY_MS 0

// Number of bytes till length parameter in header out of total packet
#define MODBUS_HEADER_LENGTH 6

// maximum devices supported by stack
// This value is used in epoll thread while receiving raw data from socket
#define MAX_DEVICE_PER_SITE 300

// Maximum request array size for request queue
#define MAX_REQUESTS 5000

// Thread priority value for all threads in stack in realtime
#define THREAD_PRIORITY 30

// Thread scheduler value for all threads in stack in realtime
// thread scheduler default value is SCHED_RR (round robin)
#define THREAD_SCHEDULER SCHED_RR

// these values are used in Modbus_Read_File_Record API
#define FILE_RECORD_REFERENCE_TYPE 6

// Validation limits for stack for all the API's according to Modbus Standard
#define MIN_COILS				(1)
#define MAX_COILS				(2000)
#define MIN_INPUT_REGISTER		(1)
#define MAX_INPUT_REGISTER		(125)
#define MIN_MULTI_REGISTER		(1)
#define MAX_MULTI_REGISTER		(123)
#define MIN_MULTI_COIL			(1)
#define MAX_MULTI_COIL			(1968)
#define MIN_HOLDING_REGISTERS	 (1)
#define MAX_HOLDING_REGISTERS 	(125)
#define MIN_INPUT			  	(1)
#define MAX_INPUT				(65535)
#define MIN_MUL_WRITE_REG		(1)
#define MAX_MUL_WRITE_REG		(121)
#define MIN_FILE_BYTE_COUNT		(7)
#define MAX_FILE_BYTE_COUNT		(245)
// Application Address
#define MAX_BITS				(8)
#define VALUE_ZERO				(0x00)
#define MEI_TYPE				(14)


/**
 @struct MbusTXData_t
 @brief
    This structure defines Modbus transmit data
*/
typedef struct
{
	// Holds the data field
	uint8_t m_au8DataFields[ MODBUS_DATA_LENGTH ];
	// Holds the received length of packet
	uint16_t m_u16Length;
}MbusTXData_t;

/**
 @struct MbusRXData_t
 @brief
    This structure defines Modbus receive data
*/
typedef struct
{
	// Holds the data field
	uint8_t m_au8DataFields[ MODBUS_DATA_LENGTH ];
	// Holds the received length of packet
	uint8_t m_u8Length;
	// Pointer to hold the additional data
	void* m_pvAdditionalData;
}MbusRXData_t;

/**
 @enum eTransactionState
 @brief
 	 This structure defines modbus index
*/
typedef enum
{
	REQ_RCVD_FROM_APP,
	REQ_PROCESS_ERROR,
	REQ_SENT_ON_NETWORK,
	RESP_RCVD_FROM_NETWORK,
	RESP_TIMEDOUT,
	RESP_ERROR,
	RESP_SENT_TO_APP,
	IdleState,
	RESERVED,
}eTransactionState;

typedef enum
{
	SOCK_CONNECT_SUCCESS,
	SOCK_CONNECT_FAILED,
	SOCK_CONNECT_INPROGRESS,
	SOCK_CREATE_FAILED,
	SOCK_CREATE_SUCCESS,
	SOCK_SEND_FAILED,
	SOCK_NOT_CONNECTED
}eSockConnect_enum;

// Retry count
#define MAX_RETRY_COUNT 10

typedef struct IP_Connect
{
	int32_t m_retryCount;					// retry count
	struct sockaddr_in m_servAddr;			// socket address
	int32_t m_sockfd;						// socket descriptor
	eSockConnect_enum m_lastConnectStatus;	//Connection status
	bool m_bIsAddedToEPoll;					// Added to Epoll
	int m_iRcvConRef;						// Receive Conference reference
}IP_Connect_t;

/**
*
* @struct  - stMbusPacketVariables_t
*
* DESCRIPTION
* This structure defines the Receive queue along with block
* size, fill & process index. It also defines the mutex
* lock required to synchronize the queue.
*
*/
typedef struct _stMbusPacketVariables
{
	// Holds the received transaction ID
	uint16_t m_u16TransactionID;
	uint16_t m_u16AppTxID;
	// Holds the unit id
	uint8_t  m_u8UnitID;
	_Atomic eTransactionState m_state;
	uint8_t  m_u8ProcessReturn;
#ifdef MODBUS_STACK_TCPIP_ENABLED
	// Holds Ip address of salve/server device
	uint8_t m_u8IpAddr[4];
	uint16_t u16Port;
	struct _stMbusPacketVariables *__next;
	struct _stMbusPacketVariables *__prev;
#else
	// Received destination address
	uint8_t	m_u8ReceivedDestination;
	//Interframe delay
	long m_lInterframeDealy;
	// Response timeout
	long m_lRespTimeout;
#endif
	// Holds the unit id
	uint8_t m_u8FunctionCode;
	// Holds Data to be send to server
	MbusTXData_t m_stMbusTxData;
	// Holds Data received from server
	MbusRXData_t m_stMbusRxData;
	/** Holds Status of command success or failure
	  * after ack received from server or timeout */
	uint8_t m_u8CommandStatus;
	//Holds the start address
	uint16_t  m_u16StartAdd;
	// Holds the Quantity
	uint16_t  m_u16Quantity;
	void *pFunc;
	// Holds the Msg Priority
	long m_lPriority;

	// bool m_bIsAvailable;
	unsigned int m_ulMyId;
	int m_iTimeOutIndex;
	stTimeStamps m_objTimeStamps;
	unsigned char m_u8RawResp[MODBUS_DATA_LENGTH];
}stMbusPacketVariables_t;

struct stReqManager {
	stMbusPacketVariables_t m_objReqArray[MAX_REQUESTS];  // To receive the Request data
	Mutex_H m_mutexReqArray;							  //  mutex request array
};

typedef struct RTUConnectionData
{
	int m_fd;				//	function descriptor
	long m_interframeDelay;	// Interframe delay
}stRTUConnectionData_t;

typedef enum ThreadScheduler
{
	OTHER,
	FIFO,
	RR,
	UNKNOWN
}eThreadScheduler;

struct stResProcessData {
	sem_t m_semaphoreResp;			// response semaphore type
	int32_t m_i32RespMsgQueId;      // response message queue ID
	Thread_H m_threadIdRespToApp;   // Thread id of response to App
};


/**
 *
 * Description
 * Initialize request manager
 *
 * @param none
 * @return int [out] true (if success)
 * 					false (if failure)
 */
bool initReqManager();

/**
 *
 * Description
 * This function sets data structure to process new request.
 *
 * @param tsReqRcvd [in] const struct timespec
 * @return int [out] pointer to emplaced request
 *
 */

stMbusPacketVariables_t* emplaceNewRequest(const struct timespec tsReqRcvd);
/**
 *
 * Description
 * Free the requested node
 *
 * @param a_pobjReq[In] pointer to struct stMbusPacketVariables_t
 * @return int [out] None
 *
 */
void freeReqNode(stMbusPacketVariables_t* a_pobjReq);


/**
 *
 * Description
 * Function to set thread parameters
 *
 * @param none
 * @return none
 *
 */
void set_thread_sched_param();

#ifdef MODBUS_STACK_TCPIP_ENABLED

struct stTimeOutTrackerNode {
	stMbusPacketVariables_t *m_pstStart;  // pointer to modbus start of recieve queue
	stMbusPacketVariables_t *m_pstLast;	  // pointer to modbus last recive queue
	int m_iIsLocked;
};

struct stTimeOutTracker {
	struct stTimeOutTrackerNode *m_pstArray;// pointer to timeout tracker node
	int m_iSize;							// size
	_Atomic int m_iCounter;					// counter
	int32_t m_iTimeoutActionQ;              // Timeout action queue
	sem_t m_semTimeout;						// Timeout
	Thread_H m_threadIdTimeoutTimer;		//Thread timeout timer
	Thread_H m_threadTimeoutAction;			//Thread timeout action
};

/**
 *
 * Description
 * Timeout is implemented in the form of counter.
 * This function returns current counter.
 *
 * @param thread argument - none
 * @return return timer tracker count on success& -1 in case of error
 */
int getTimeoutTrackerCount();

/**
 *
 * Description
 * Remove request from list with locks
 *
 * @param pstMBusRequesPacket [in] pointer to struct of type stMbusPacketVariables_t
 * @return void [out] none
 */
void releaseFromTracker(stMbusPacketVariables_t *pstMBusRequesPacket);

#endif

#ifdef MODBUS_STACK_TCPIP_ENABLED
/**
 *
 * Description
 * Stops listening on a socket with epoll.
 * Resets associated data structures.
 * This function acquires lock for epoll data structure.
 *
 * @param Reference to epoll data structure
 * @return None
 */
void removeEPollRef(int a_iIndex);

/**
 * Description
 * Set socket failure state
 *
 * @param stIPConnect [in] pointer to struct of type IP_Connect_t
 *
 * @return void [out] return status based on condition
 *
 */
int addtoEPollList(IP_Connect_t *a_pstIPConnect);

/**
 @struct IP_address
 @brief
    This structure defines IP address parameters
*/
typedef struct IP_address
{
	union
	{
		struct{
			unsigned char IP_1;
			unsigned char IP_2;
			unsigned char IP_3;
			unsigned char IP_4;
		}s_un_b;
		uint32_t s_addr;
	}s_un;
}IP_address_t;
#endif
/**
 @union uByteOrder
 @brief
    This structure defines byte structure
*/
typedef union uByteOrder
{
	struct
	{
		unsigned char u8ByteOne;
		unsigned char u8ByteTwo;
	}TwoByte;
	uint16_t u16Word;
}uByteOrder_t;

/**
 @union stEndianess
 @brief
    This structure defines endianess structure
*/
typedef union stEndianess
{
	struct
	{
		uint8_t u8FirstByte;
		uint8_t u8SecondByte;
	}stByteOrder;
	uint16_t u16word;
}stEndianess_t;

/**
 @enum eMbusIndex_enum
 @brief
    This structure defines modbus index
*/
typedef enum
{
	MBUS_INDEX_0,
	MBUS_INDEX_1,
	MBUS_INDEX_2,
	MBUS_INDEX_3,
	MBUS_INDEX_4,
	MBUS_INDEX_5,
	MBUS_INDEX_6,
	MBUS_INDEX_7,
	MBUS_INDEX_8,
	MBUS_INDEX_9,
	MBUS_INDEX_10
}eMbusIndex_enum;

/**
 *
 * Description
 * Application callback handler
 *
 * @param pstMBusRequesPacket [in] Request packet
 * @param eMbusStackErr       [in] Stack error codes
 *
 */
void ApplicationCallBackHandler(stMbusPacketVariables_t *pstMBusRequesPacket,
		eStackErrorCode eMbusStackErr);

#ifdef MODBUS_STACK_TCPIP_ENABLED

/**
 * Description
 * Send modbus packet on network
 *
 * @param pstMBusRequesPacket [in] pointer to request packet struct of type stMbusPacketVariables_t
 * @param a_pstIPConnect [in] pointer to socket struct of type IP_Connect_t
 *
 * @return uint8_t [out] respective error codes
 *
 */
uint8_t Modbus_SendPacket(stMbusPacketVariables_t *pstMBusRequesPacket,
		IP_Connect_t *m_pstIPConnect);
#else
/**
 * Description
 * This function sends request to Modbus slave device using RTU communication mode. The function
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
		long a_lRespTimeout);
#endif
#endif // STACKCONFIG_H_
