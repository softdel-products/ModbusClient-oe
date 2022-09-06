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

#ifndef API_H_
#define API_H_

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

#define MODBUS_STACK_EXPORT __attribute__ ((visibility ("default")))
#define MODBUS_STACK_IMPORT __attribute__ ((visibility ("default")))

typedef unsigned char   	uint8_t;    // 1 byte  0 to 255
typedef signed char     	int8_t;     // 1 byte -127 to 127
typedef unsigned short  	uint16_t;   // 2 bytes 0 to 65535
typedef signed short    	int16_t;    // 2 bytes -32767 to 32767
#ifdef _WIN64
typedef long long          	ilong32_t;  // singed long long declarations
typedef unsigned long long 	ulong32_t;  // unsinged long long declarations
#else
typedef long            	ilong32_t;  // singed long declarations
typedef unsigned long   	ulong32_t;  // unsinged long declarations
#endif

#define MODBUS_DATA_LENGTH (260)  // Modbus maximum data packet length

/*
 ===============================================================================
 Function Definitions
 ===============================================================================
 */

/**
 @enum MODBUS_ERROR_CODE
 @brief
    This enumerator defines MODBUS error codes
*/
typedef enum StackErrorCode
{
	STACK_NO_ERROR, 						 // no error or successful
	STACK_TXNID_OR_UNITID_MISSMATCH,		 // Modbus request transaction id or unit id in response is mismatching
	STACK_ERROR_SOCKET_FAILED,				 // failed to read data from socket connected to Modbus device
	STACK_ERROR_CONNECT_FAILED,				 // socket connection with Modbus device failed
	STACK_ERROR_SEND_FAILED,				 // failed to send request on Modbus via socket
	STACK_ERROR_RECV_FAILED,				 // failed to receive response from Modbus device on socket
	STACK_ERROR_RECV_TIMEOUT,				 // failed to receive response within timeout duration
	STACK_ERROR_MALLOC_FAILED,				 // failed to allocate memory
	STACK_ERROR_QUEUE_SEND,					 // failed to send data in queue
	STACK_ERROR_QUEUE_RECIVE,				 // failed to receive data from queue
	STACK_ERROR_THREAD_CREATE,				 // failed to create thread
	STACK_ERROR_INVALID_INPUT_PARAMETER,	 // invalid input provided
	STACK_ERROR_PACKET_LENGTH_EXCEEDED,		 // packet length exceeded than limit
	STACK_ERROR_SOCKET_LISTEN_FAILED,		 // failed to listen on socket connected to Modbus device
	STACK_ERROR_MAX_REQ_SENT,				 // already sent maximum number of requests
	STACK_ERROR_FAILED_Q_SENT_REQ,			 // Unable to add request for timeout tracking. marking it as filed
	STACK_INIT_FAILED,						 // Stack initialization fail error
	STACK_ERROR_QUEUE_CREATE,				 // Linux queue creation failed error
	STACK_ERROR_MUTEX_CREATE,				 // Linux mutex creation failed error
	STACK_ERROR_STACK_IS_NOT_INITIALIZED,	 // Stack Initialization failed
	STACK_ERROR_STACK_IS_ALREADY_INITIALIZED,//	Stack Already Initialized
	STACK_ERROR_SERIAL_PORT_ERROR,			 //	When serial port not initialized
	STACK_ERROR_SERIAL_PORT_ALREADY_IN_USE,	 //	When port is already used for communication
	STACK_ERROR_PORT_NAME_LENGTH_EXCEEDED,	 //	When port name exceed maximum length
	STACK_ERROR_INVALID_BAUD_RATE,			 //	serial Initialized with Invalid baudrate
	STACK_ERROR_MAX=60						 // max error
}eStackErrorCode;

/**
 @enum MODBUS_FUNC_CODE
 @brief
    This enumerator defines MODBUS function codes
*/
typedef enum {
	MBUS_MIN_FUN_CODE = 0,
	READ_COIL_STATUS,			///01
	READ_INPUT_STATUS,			///02
	READ_HOLDING_REG,			///03
	READ_INPUT_REG,				///04
	WRITE_SINGLE_COIL,			///05
	WRITE_SINGLE_REG,			///06
	WRITE_MULTIPLE_COILS = 15,	///15
	WRITE_MULTIPLE_REG,			///16
	READ_FILE_RECORD = 20,		///20
	WRITE_FILE_RECORD,			///21
	READ_WRITE_MUL_REG = 23,	///23
	READ_DEVICE_IDENTIFICATION = 43,	///43
	MBUS_MAX_FUN_CODE
} eModbusFuncCode_enum;

typedef enum
{
	eNone,
	eOdd,
	eEven
}eParity;

/**
 @struct MbusReadFileRecord
 @brief
    This structure defines request of modbus read file record function code
*/
typedef struct MbusReadFileRecord
{
	uint8_t			m_u8RefType; 	 	     // Reference type (must be specified as 6)
	uint16_t		m_u16FileNum;	 	     //File number
	uint16_t		m_u16RecordNum;		     //m_u16StartingAddr;
	uint16_t		m_u16RecordLength;	     //m_u16NoOfRecords;
	struct MbusReadFileRecord *pstNextNode;  //Pointer to next group in the list
}stMbusReadFileRecord_t;

/**
 @struct SubReq
 @brief
    This structure defines sub request of modbus read file record function code
*/
typedef struct SubReq
{
	uint8_t			m_u8FileRespLen;  //Length of data in this sub-response
	uint8_t			m_u8RefType;	  // Reference type (value is 6)
	uint16_t		*m_pu16RecData;   //Received data
	struct SubReq   *pstNextNode;	  //Pointer to next group in the list
}stRdFileSubReq_t;

/**
 @struct MbusRdFileRecdResp
 @brief
    This structure defines response of modbus read file record function code
*/
typedef struct MbusRdFileRecdResp
{
	uint8_t				m_u8RespDataLen;  //Length of response
	stRdFileSubReq_t	m_stSubReq;		  //Structure containing response groups
}stMbusRdFileRecdResp_t;

/**
 @struct WrFileSubReq
 @brief
    This structure defines request of modbus write file record function code
*/
typedef struct WrFileSubReq
{
	uint8_t			m_u8RefType;        //Reference type (must be specified as 6)
	uint16_t		m_u16FileNum;       //File number
	uint16_t		m_u16RecNum;        // Starting record number within file
	uint16_t		m_u16RecLen;	    //Length of record
	uint16_t		*m_pu16RecData;     //Pointer to data to be written
	struct WrFileSubReq   *pstNextNode; //Pointer to next group in the list
}stWrFileSubReq_t;

/**
 @struct MbusWrFileRecdResp
 @brief
    This structure defines response of modbus write file record function code
*/
typedef struct MbusWrFileRecdResp
{
	uint8_t				m_u8RespDataLen; //Response data length
	stWrFileSubReq_t	m_stSubReq;      //Structure containing response groups
}stMbusWrFileRecdResp_t;

/**
 @struct Exception
 @brief
    This structure defines exception code
*/
typedef struct Exception
{
	uint8_t		m_u8ExcStatus;  //Exception type depends on stack or response from slave
	uint8_t		m_u8ExcCode;    //Actual exception error code
}stException_t;

/**
 @struct SubObjList
 @brief
    This structure defines sub request of read device identification function code
*/
typedef struct SubObjList
{
	unsigned char 	m_u8ObjectID;      // Object ID
	unsigned char 	m_u8ObjectLen;     //Length of Object
	unsigned char	*m_u8ObjectValue;  // Pointer to data value
	struct SubObjList   *pstNextNode;  // pointer to next node in the list
}SubObjList_t;

/**
 @struct RdDevIdReq
 @brief
    This structure defines request of read device identification function code
*/
typedef struct RdDevIdReq
{
#ifdef MODBUS_STACK_TCPIP_ENABLED
	unsigned char	m_u8IpAddr[4];
#else
	unsigned char	m_u8DestAddr;
#endif
	unsigned char 	m_u8UnitId;        // slave ID
	unsigned char 	m_u8MEIType;       // Modbus Encpsulated Interface
	unsigned char 	m_u8RdDevIDCode;   // read device identification code
	unsigned char 	m_u8ObjectID;      // Object ID
	unsigned char 	m_u8FunCode;       // function code
}stRdDevIdReq_t;

/**
 @struct RdDevIdResp
 @brief
    This structure defines response of read device identification function code
*/
typedef struct RdDevIdResp
{
	unsigned char 	m_u8MEIType;          // Modbus Encpsulated Interface
	unsigned char 	m_u8RdDevIDCode; 	  // read device identification code
	unsigned char 	m_u8ConformityLevel;  // Identification conformity level of the device
	unsigned char 	m_u8MoreFollows;      //  More device Id access
	unsigned char 	m_u8NextObjId;        // next object ID only if more follows
	unsigned char 	m_u8NumberofObjects;  // number of identification object
	SubObjList_t 	m_pstSubObjList;      // pointer to next object list
}stRdDevIdResp_t;

typedef struct DevConfig
{
	long 			m_lInterframedelay;   //Interframe delay
	long 			m_lResponseTimeout;   // response timeout
}stDevConfig_t;

typedef struct TimeStamps
{
	struct timespec tsReqRcvd;          // Timestamp for Request recieved
	struct timespec tsReqSent;          // Timestamp for request sent
	struct timespec tsRespRcvd;			// Timestamp for request received
	struct timespec tsRespSent;         // Timestamp for request sent
}stTimeStamps;

// Modbus master stack initialization function
MODBUS_STACK_EXPORT uint8_t AppMbusMaster_StackInit();

// Modbus master stack de-initialization function
MODBUS_STACK_EXPORT void AppMbusMaster_StackDeInit(void);

// Modbus master stack configuration function
MODBUS_STACK_EXPORT uint8_t AppMbusMaster_SetStackConfigParam(stDevConfig_t *pstDevConf);

// Modbus master stack configuration function
MODBUS_STACK_EXPORT stDevConfig_t* AppMbusMaster_GetStackConfigParam();

// Read coil API
MODBUS_STACK_EXPORT uint8_t Modbus_Read_Coils(uint16_t u16StartCoil,
											  uint16_t u16NumOfcoils,
											  uint16_t u16TransacID,
											  uint8_t u8UnitId,
											  long lPriority,
											  int32_t i32Ctx,
											  void* pFunCallBack);

// Read discrete input API
MODBUS_STACK_EXPORT uint8_t Modbus_Read_Discrete_Inputs(uint16_t u16StartDI,
														uint16_t u16NumOfDI,
														uint16_t u16TransacID,
														uint8_t u8UnitId,
														long lPriority,
														int32_t i32Ctx,
														void* pFunCallBack);

// Read holding register API
MODBUS_STACK_EXPORT uint8_t Modbus_Read_Holding_Registers(uint16_t u16StartReg,
														  uint16_t u16NumberOfRegisters,
														  uint16_t u16TransacID,
														  uint8_t u8UnitId,
														  long lPriority,
														  int32_t i32Ctx,
														  void* pFunCallBack);

// Read input register API
MODBUS_STACK_EXPORT uint8_t Modbus_Read_Input_Registers(uint16_t u16StartReg,
													    uint16_t u16NumberOfRegisters,
														uint16_t u16TransacID,
														uint8_t u8UnitId,
														long lPriority,
														int32_t i32Ctx,
														void* pFunCallBack);

// write single coil API
MODBUS_STACK_EXPORT uint8_t Modbus_Write_Single_Coil(uint16_t u16StartCoil,
													 uint16_t u16OutputVal,
													 uint16_t u16TransacID,
													 uint8_t u8UnitId,
													 long lPriority,
													 int32_t i32Ctx,
													 void* pFunCallBack);

// write single register API
MODBUS_STACK_EXPORT uint8_t Modbus_Write_Single_Register(uint16_t u16StartReg,
														 uint16_t u16RegOutputVal,
														 uint16_t u16TransacID,
														 uint8_t u8UnitId,
														 long lPriority,
														 int32_t i32Ctx,
														 void* pFunCallBack);

// write multiple coils API
MODBUS_STACK_EXPORT uint8_t Modbus_Write_Multiple_Coils(uint16_t u16Startcoil,
													   uint16_t u16NumOfCoil,
													   uint16_t u16TransacID,
													   uint8_t  *pu8OutputVal,
													   uint8_t  u8UnitId,
													   long lPriority,
													   int32_t i32Ctx,
													   void*    pFunCallBack);

// write multiple registers API
MODBUS_STACK_EXPORT uint8_t Modbus_Write_Multiple_Register(uint16_t u16StartReg,
									   	   	   	   	   	   uint16_t u16NumOfReg,
														   uint16_t u16TransacID,
														   uint8_t  *pu8OutputVal,
														   uint8_t  u8UnitId,
														   long lPriority,
														   int32_t i32Ctx,
														   void*    pFunCallBack);

// read file record API
MODBUS_STACK_EXPORT uint8_t Modbus_Read_File_Record(uint8_t u8byteCount,
													uint8_t u8FunCode,
													stMbusReadFileRecord_t *pstFileRecord,
													uint16_t u16TransacID,
													uint8_t u8UnitId,
													long lPriority,
													int32_t i32Ctx,
													void* pFunCallBack);

// write file record API
MODBUS_STACK_EXPORT uint8_t Modbus_Write_File_Record(uint8_t u8ReqDataLen,
		 	 	 	 	 	 	 	 	 	 	 	uint8_t u8FunCode,
													stWrFileSubReq_t *pstFileRecord,
													uint16_t u16TransacID,
													uint8_t u8UnitId,
													long lPriority,
													int32_t i32Ctx,
													void* pFunCallBack);

// read write multiple registers API
MODBUS_STACK_EXPORT uint8_t Modbus_Read_Write_Registers(uint16_t u16ReadRegAddress,
									uint8_t u8FunCode,
									uint16_t u16NoOfReadReg,
									uint16_t u16WriteRegAddress,
									uint16_t u16NoOfWriteReg,
									uint16_t u16TransacID,
									uint8_t *pu8OutputVal,
									uint8_t u8UnitId,
									long lPriority,
									int32_t i32Ctx,
									void* pFunCallBack);

// Read device identification
MODBUS_STACK_EXPORT uint8_t Modbus_Read_Device_Identification(uint8_t u8MEIType,
		uint8_t u8FunCode,
		uint8_t u8ReadDevIdCode,
		uint8_t u8ObjectId,
		uint16_t u16TransacID,
		uint8_t u8UnitId,
		long lPriority,
		int32_t i32Ctx,
		void* pFunCallBack);

// struct for Modbus_AppllicationCallbackHandler
typedef struct _stMbusAppCallbackParams
{
	// Holds the received transaction ID
	uint16_t m_u16TransactionID;
    // Holds the unit id
	uint8_t m_u8FunctionCode;
	// Holds Data received from server
	//MbusRXData_t m_stMbusRxData;
	uint8_t m_u8MbusRXDataLength;
#ifdef MODBUS_STACK_TCPIP_ENABLED
	// Holds the unit id
	uint8_t  m_u8UnitID;
	// Holds Ip address of salve/server device
	uint8_t m_u8IpAddr[4];
	uint16_t u16Port;
#else
	// Received destination address
	uint8_t	m_u8ReceivedDestination;
#endif
	uint8_t m_au8MbusRXDataDataFields[ MODBUS_DATA_LENGTH ];

	// Holds the start address
	uint16_t  m_u16StartAdd;
	// Holds the Quantity
	uint16_t  m_u16Quantity;
	// Holds the Msg Priority
	long m_lPriority;
	stTimeStamps m_objTimeStamps;
	// exception if any from Modbus
	uint8_t		m_u8ExceptionExcStatus;
	// exception code
	uint8_t		m_u8ExceptionExcCode;

}stMbusAppCallbackParams_t;
//end of Modbus_ApplicationCallbackHandler struct

typedef struct CtxInfo
{
#ifndef MODBUS_STACK_TCPIP_ENABLED
	uint8_t *m_u8PortName;      // Serial port name
	uint32_t m_u32baudrate;     // baudrate of slave device
	eParity  m_eParity;			// parity bit supported
	long	m_lInterframeDelay;  // Interframe delay
	long	m_lRespTimeout;     // response timeout of the data packet
#else
	uint8_t *pu8SerIpAddr;      // TCPIP- IP Address
	uint16_t u16Port;			// TCPIP - port name
#endif
}stCtxInfo;

#ifndef MODBUS_STACK_TCPIP_ENABLED
// RTU context
MODBUS_STACK_EXPORT eStackErrorCode getRTUCtx(int32_t *rtuCtx, stCtxInfo *pCtxInfo);
#else
// TCP context
MODBUS_STACK_EXPORT eStackErrorCode getTCPCtx(int *tcpCtx, stCtxInfo *pCtxInfo);
#endif

// Remove the context
MODBUS_STACK_EXPORT void removeCtx(int msgQId);

#endif // API_H_
