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

#include "SessionControl.h"
#include <safe_lib.h>
#include <unistd.h>
#include <time.h>

 /*
  ===============================================================================
  Global variable definitions
  ===============================================================================
  */

//SessionControl thread ID
Thread_H SessionControl_ThreadId = 0;

//Linux message queue ID
extern int32_t i32MsgQueIdSC;

//Flag to notify whether to exit the thread and thus the complete execution or continue
bool g_bThreadExit = false;

// global variable to store stack configurations
stDevConfig_t g_stModbusDevConfig = {0};

// global variable to maintain stack enable status
bool g_bIsStackEnable = false;

//Mutex for session control list
Mutex_H LivSerSesslist_Mutex = NULL;

// a list of sessions having details of a session e.g. socket information,
// session ID, linux msg queue ID etc.
extern stLiveSerSessionList_t *pstSesCtlThdLstHead;

/*
 ===============================================================================
 Function Definitions
 ===============================================================================
 */

/**
 * @fn MODBUS_STACK_EXPORT uint8_t AppMbusMaster_SetStackConfigParam(stDevConfig_t *a_pstDevConf)
 *
 * @brief Exported function to set stack configuration parameter
 * This API has to be called before AppMbusMaster_StackInit() API
 *
 * @param pstDevConf [in] stDevConfig_t* list of parameters to be set as a configuration for stack
 *
 * @return uint8_t [out] STACK_NO_ERROR if function succeeds,
 * 						 STACK_ERROR_STACK_IS_ALREADY_INITIALIZED if stack is already initialized
 */
MODBUS_STACK_EXPORT uint8_t AppMbusMaster_SetStackConfigParam(stDevConfig_t *a_pstDevConf)
{
	// check stack status and return stack error code
	if(g_bIsStackEnable)
	{
		return STACK_ERROR_STACK_IS_ALREADY_INITIALIZED;
	}
	if(a_pstDevConf != NULL)
	{
		// convert it to micro seconds (us)
		if(a_pstDevConf->m_lInterframedelay > 0 &&
				a_pstDevConf->m_lInterframedelay < MAX_ENV_VAR_LEN)
		{
			//1000 is to convert millisecond value to microsecond
			g_stModbusDevConfig.m_lInterframedelay = (a_pstDevConf->m_lInterframedelay) * 1000;
		}
		if(a_pstDevConf->m_lResponseTimeout > 0 &&
				a_pstDevConf->m_lResponseTimeout < MAX_ENV_VAR_LEN)
		{
			//1000 is to convert millisecond value to microsecond
			g_stModbusDevConfig.m_lResponseTimeout = (a_pstDevConf->m_lResponseTimeout) * 1000;
		}
	}
	// set default parameters in micro seconds(us)
	else
	{
		//1000 is to convert millisecond value to microsecond
		g_stModbusDevConfig.m_lResponseTimeout = DEFAULT_RESPONSE_TIMEOUT_MS * 1000;
		g_stModbusDevConfig.m_lInterframedelay = DEFAULT_INTERFRAME_DELAY_MS * 1000;
	}

	printf("\n\nStack config parameters ------------------------\n");
	printf("response timeout is set to :: %ld us\n", g_stModbusDevConfig.m_lResponseTimeout);
	printf("Interframe delay is set to :: %ld us\n\n", g_stModbusDevConfig.m_lInterframedelay);

	return STACK_NO_ERROR;
} // AppMbusMaster_SetStackConfigParam

/**
 * @fn MODBUS_STACK_EXPORT stDevConfig_t* AppMbusMaster_GetStackConfigParam()
 *
 * @brief Exported function to get Modbus master stack configuration parameter
 *
 * @param Nothing
 *
 * @return stDevConfig_t* [out] available stack configurations
 */
MODBUS_STACK_EXPORT stDevConfig_t* AppMbusMaster_GetStackConfigParam()
{
	return &g_stModbusDevConfig;
} // AppMbusMaster_GetStackConfigParam

/**
 * @fn MODBUS_STACK_EXPORT uint8_t AppMbusMaster_StackInit()
 *
 * @brief  Exported function to initiate modbus master stack. If TCP mode is getting used
 * for communication with Modbus device, it initializes TCP request list
 * and a TCP session list. For both TCP and RTU, it then initializes the request manager to
 * keep track of all the incoming requests from ModbusApp and initializes Linux message queue
 * to store all the requests send to the Modbus device. If all of this succeeds,
 * function then starts a session control thread.
 *
 * @param none
 *
 * @return uint8_t [out] STACK_INIT_FAILED or STACK_ERROR_THREAD_CREATE in case of error,
 * 						 STACK_NO_ERROR in case of success
 *
 */
MODBUS_STACK_EXPORT uint8_t AppMbusMaster_StackInit()
{
	//Local variable
	uint8_t eStatus = STACK_NO_ERROR;
	g_bThreadExit = false;

	// if initRespStructs is -1 then STACK_INIT_FAILED (stack error code)
	if(-1 == initRespStructs())
	{
		printf("failed to init initTCPRespStructs\n");
		eStatus = STACK_INIT_FAILED;
	}
	// Mutex for session control list is NULL then STACK_ERROR_MUTEX_CREATE (stack error code)
	LivSerSesslist_Mutex = Osal_Mutex();
	if(NULL == LivSerSesslist_Mutex)
	{
		eStatus = STACK_ERROR_MUTEX_CREATE;
	}
	// Initialize request manager and data structure
	if(!initReqManager())
	{
		eStatus = STACK_INIT_FAILED;
	}

	if(STACK_NO_ERROR == eStatus)
	{
		// set the stack enable status as true
		g_bIsStackEnable = true;
	}
	return eStatus;
} // AppMbusMaster_StackInit

/**
 * @fn MODBUS_STACK_EXPORT void AppMbusMaster_StackDeInit(void)
 *
 * @brief Exported function to de-initiate modbus master stack. This function terminates the
 * sessions from session control list, then terminate the session control thread,
 * clear Linux message queue and request manager; terminate all the mutexes
 *
 * @param none
 *
 * @return void [out] none
 *
 */
MODBUS_STACK_EXPORT void AppMbusMaster_StackDeInit(void)
{
	static bool bDeInitStackFlag = false;
	//making the function non-reentrant
	if(true == bDeInitStackFlag)
		return;
	g_bThreadExit = true;
	
	//validate the session thread ID
	if(SessionControl_ThreadId)
	{
		//Terminate the session thread
		Osal_Thread_Terminate(SessionControl_ThreadId);
	}
	if(i32MsgQueIdSC)
	{
		//For valid message queue, delete the message queue
		OSAL_Delete_Message_Queue(i32MsgQueIdSC);
	}
	
	// Delete session list
	if(0 != Osal_Wait_Mutex(LivSerSesslist_Mutex))
	{
		// fail to lock mutex
		return;
	}
	//Declare pointer to structure to handle thread list
	stLiveSerSessionList_t *pstTempLivSerSesslist = pstSesCtlThdLstHead;

	//Traverse the session list to terminate the valid session present
	while(NULL != pstSesCtlThdLstHead)
	{
		pstTempLivSerSesslist = pstSesCtlThdLstHead;
		pstSesCtlThdLstHead = pstSesCtlThdLstHead->m_pNextElm;
		pstTempLivSerSesslist->m_pNextElm = NULL;
		// terminate thread specification by thread ID
		if(pstTempLivSerSesslist->m_ThreadId)
		{
			//Terminate the session for the valid thread ID
			Osal_Thread_Terminate(pstTempLivSerSesslist->m_ThreadId);
		}
		//Delete message queue by message ID
		if(pstTempLivSerSesslist->MsgQId)
		{
			//Delete the message queue for the valid message ID
			OSAL_Delete_Message_Queue(pstTempLivSerSesslist->MsgQId);
		}
		//De-allocate the memory
		OSAL_Free(pstTempLivSerSesslist);
		pstTempLivSerSesslist = NULL;
	}
	//De-allocate memory allocated for mutex
	Osal_Release_Mutex (LivSerSesslist_Mutex);
	
	if(LivSerSesslist_Mutex)
	{
		//Close the mutex
		Osal_Close_Mutex(LivSerSesslist_Mutex);
	}

	deinitRespStructs();

	//update re-entrancy flag
	bDeInitStackFlag = false;
	g_bThreadExit = false;
} //AppMbusMaster_StackDeInit

/**
 * @fn MODBUS_STACK_EXPORT void removeCtx(int msgQId)
 *
 * @brief This function is used to remove the created context of TCP/RTU communication
 *
 * @param msgQId [in] message queue id which needs to be removed from list
 *
 * @return void [out] none
 *
 *
 */
MODBUS_STACK_EXPORT void removeCtx(int msgQId)
{
	// Delete session list
	if(0 != Osal_Wait_Mutex(LivSerSesslist_Mutex))
	{
		// fail to lock mutex
		return;
	}
	//Declare pointer to structure to handle thread list
	stLiveSerSessionList_t *pstTempLivSerSesslist = pstSesCtlThdLstHead;
	//Traverse the list till end of the list
	while(NULL != pstSesCtlThdLstHead)
	{
		pstTempLivSerSesslist = pstSesCtlThdLstHead;
		pstSesCtlThdLstHead = pstSesCtlThdLstHead->m_pNextElm;
		pstTempLivSerSesslist->m_pNextElm = NULL;
		//check the valid message queue ID
		if(pstTempLivSerSesslist->MsgQId == msgQId)
		{
			//Terminate the thread
			Osal_Thread_Terminate(pstTempLivSerSesslist->m_ThreadId);
			//Delete the message queue ID
			OSAL_Delete_Message_Queue(pstTempLivSerSesslist->MsgQId);
			//Free allocate memory
			OSAL_Free(pstTempLivSerSesslist);
		}
	}
	//release memory allocated for mutex
	Osal_Release_Mutex (LivSerSesslist_Mutex);
} // End of removeCtx

/**
 * @fn  bool ValidateQuantity( uint8_t eFunCode, uint16_t u16Quantity ,	uint8_t u8ByteCount)
 *
 * @brief This function validates quantity; a value contained in the query data field. In case of read
 * operations, it validates the quantity. In case of write operations, it validates
 * the byte count as well.
 *
 * @param eFunCode 		[in] uint8_t received function code (indicates the kind of action to perform)
 * @param u16Quantity 	[in] uint16_t received data to validate for a function code
 * @param u8ByteCount	[in] uint8_t byte count for quantity (For function codes carrying a variable
 * 							 amount of data in the request or response, the data field includes a byte count.)
 *
 * @return bool 		[out] true if the quantity or byte count is not within specified range;
 * 							  false if the quantity or byte count is within specified range.
 *
 */
bool ValidateQuantity( uint8_t eFunCode,
		uint16_t u16Quantity ,
		uint8_t u8ByteCount)
{
	bool bExceptionFound = false;

	switch(eFunCode)
	{
		case READ_COIL_STATUS:
		case READ_INPUT_STATUS:
		{
			// If quantity is not in range then return ILLEGAL_DATA_VAL exception.
			if (( u16Quantity < MIN_COILS)||
				(u16Quantity > MAX_COILS))
			{
				bExceptionFound = true;
			} // Else not required
		}
		break;
		case READ_INPUT_REG:
		{
			// If quantity is not in range then return ILLEGAL_DATA_VAL exception.
			if (( u16Quantity < MIN_INPUT_REGISTER)||
				(u16Quantity > MAX_INPUT_REGISTER))
			{
				bExceptionFound = true;
			} // Else not required
		}
		break;
		case READ_HOLDING_REG:
		{
			// validate the quantity of holding registers.
			if((u16Quantity < MIN_HOLDING_REGISTERS)||
				(u16Quantity >MAX_HOLDING_REGISTERS))
			{
				bExceptionFound = true;
			} //Else not required
		}
		break;
		case WRITE_MULTIPLE_COILS:
		{
			//Decide the byte count depending on no of coils
			uint8_t u8DummyCnt = VALUE_ZERO;
			u8DummyCnt = (u16Quantity / MAX_BITS);
			if(VALUE_ZERO != (u16Quantity % MAX_BITS))
			{
				u8DummyCnt = u8DummyCnt + 1;
			}

			// For Write multiple register,byte count should be 246
			if( (u16Quantity < MIN_MULTI_COIL)||
				(u16Quantity > MAX_MULTI_COIL) ||
				(VALUE_ZERO == u8ByteCount) || (u8DummyCnt != u8ByteCount))    //Byte Count Validation
			{
				bExceptionFound = true;
			} // Else not required
		}
 		break;
		case WRITE_MULTIPLE_REG:
		{
			// If quantity is not in range then return ILLEGAL_DATA_VAL exception.
			// For multiple Register byte count will be 246
			if ((u16Quantity < MIN_MULTI_REGISTER) ||
			     (u16Quantity > MAX_MULTI_REGISTER) ||
				(VALUE_ZERO == u8ByteCount) ||   //Byte Count Validation
				(u8ByteCount != (uint8_t)(u16Quantity<<1))) //Byte = No.Of Points * 2
			{
				bExceptionFound = true;
			} // Else not required
		}
		break;
		case READ_WRITE_MUL_REG:
		{
			// validate the quantity of registers as per spec
			if((u16Quantity < MIN_MUL_WRITE_REG)
					|| (u16Quantity >MAX_MUL_WRITE_REG)
					|| (u8ByteCount != (uint8_t)(u16Quantity<<1)))

			{
				// Invalid send exception
				bExceptionFound = true;
			}
		}
		break;
		case READ_FILE_RECORD:
		{
			// validate the quantity of registers as per spec
			if((u16Quantity < MIN_FILE_BYTE_COUNT)||
							(u16Quantity >MAX_FILE_BYTE_COUNT))

			{
				// Invalid send exception
				bExceptionFound = true;
			}
		}
		break;
		default:
		{
			// if there is no exception return false
			bExceptionFound = false;
		}
		break;
	} // End of switch

	return bExceptionFound;
} // End of ValidateQuantity


/**
 * @fn  uint8_t CreateHeaderForModbusRequest(uint16_t u16StartAddr,
							uint8_t u8UnitId,
							uint16_t u16HeaderLength,
							uint16_t u16TransacID,
							uint8_t u8FunctionCode,
							stMbusPacketVariables_t *pstMBusRequesPacket)
 *
 * @brief This function creates a request header in Modbus header format to send
 * to a Modbus device for specified Modbus operation.
 *
 * @param u16StartAddr 			[in] uint16_t start address of data from where to perform operation
 * @param u8UnitId				[in] uint8_t Modbus slave device ID
 * @param u16HeaderLength		[in] uint16_t Total Modbus request header length
 * @param u16TransacID			[in] uint16_t transaction ID of a request to be sent to a Modbus slave device
 * @param u8FunctionCode		[in] uint8_t Type of operation to be performed on the Modbus slave device
 * @param pstMBusRequesPacket 	[in] pointer to request packet struct of type stMbusPacketVariables_t, containing
 * 									 parameters to fill up in the request
 *
 * @return uint16_t				[out] total length of the request packet
 *
 */
uint8_t CreateHeaderForModbusRequest(uint16_t u16StartAddr,
							uint8_t u8UnitId,
							uint16_t u16HeaderLength,
							uint16_t u16TransacID,
							uint8_t u8FunctionCode,
							stMbusPacketVariables_t *pstMBusRequesPacket)
{
	stEndianess_t stEndianess = { 0 };
	uint16_t u16PacketIndex = 0;

// create header for modbus request
#ifdef MODBUS_STACK_TCPIP_ENABLED

	stEndianess.u16word = u16TransacID;

	// Release Mutex lock.
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;
	pstMBusRequesPacket->m_u16TransactionID = stEndianess.u16word;
#else
	// Transaction ID
	pstMBusRequesPacket->m_u16TransactionID = u16TransacID;
#endif

#ifdef MODBUS_STACK_TCPIP_ENABLED
	// Protocol ID
	stEndianess.u16word = 0;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;

	// Length
	stEndianess.u16word = u16HeaderLength;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;

#else
	// Unit ID
	pstMBusRequesPacket->m_u8ReceivedDestination = u8UnitId;

#endif //MODBUS_STACK_TCPIP_ENABLED

	// Unit Id
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =  u8UnitId;
	pstMBusRequesPacket->m_u8UnitID = u8UnitId;

	// Function Code
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] = u8FunctionCode;
	pstMBusRequesPacket->m_u8FunctionCode = u8FunctionCode;

	// Starting address or offset address
	stEndianess.u16word = u16StartAddr;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;

	return u16PacketIndex;
} // End of CreateHeaderForModbusRequest

/**
 * @fn uint8_t CreateHeaderForDevIdentificationModbusRequest(uint8_t u8UnitId,
							uint16_t u16HeaderLength,
							uint16_t u16TransacID,
							uint8_t u8FunctionCode,
							stMbusPacketVariables_t *pstMBusRequesPacket)
 *
 * @brief This function creates a request header in Modbus request header format for
 * a device identification request for specified Modbus operation.
 *
 * @param u8UnitId 				[in] uint8_t Modbus slave device ID
 * @param u16HeaderLength 		[in] uint16_t Total Modbus request header length
 * @param u16TransacID 			[in] uint16_t transaction ID of a request to be sent to a Modbus slave device
 * @param u8FunctionCode 		[in] uint8_t Type of operation to be performed on the Modbus slave device
 * @param pstMBusRequesPacket 	[in] pointer to request packet struct of type stMbusPacketVariables_t, containing
 * 									 parameters to fill up in the request
 *
 * @return uint8_t				[out] total length of the request packet
 *
 */
uint8_t CreateHeaderForDevIdentificationModbusRequest(uint8_t u8UnitId,
							uint16_t u16HeaderLength,
							uint16_t u16TransacID,
							uint8_t u8FunctionCode,
							stMbusPacketVariables_t *pstMBusRequesPacket)
{

	uint16_t u16PacketIndex = 0;

// Not required for RTU
#ifdef MODBUS_STACK_TCPIP_ENABLED
	stEndianess_t stEndianess = { 0 };

	// Transaction ID
	stEndianess.u16word = u16TransacID;
	// fill the data
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;
	// Transaction ID
	pstMBusRequesPacket->m_u16TransactionID = stEndianess.u16word;
#else
	// Transaction ID
	pstMBusRequesPacket->m_u16TransactionID = u16TransacID;
#endif

#ifdef MODBUS_STACK_TCPIP_ENABLED
	// Protocol ID
	stEndianess.u16word = 0;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;

	//Length
	stEndianess.u16word = u16HeaderLength;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;
#endif  //#ifdef MODBUS_STACK_TCPIP_ENABLED


	//Unit Id
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =  u8UnitId;
	pstMBusRequesPacket->m_u8UnitID = u8UnitId;

	//Function Code
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] = u8FunctionCode;
	pstMBusRequesPacket->m_u8FunctionCode = u8FunctionCode;

	return u16PacketIndex;
} // End of CreateHeaderForDevIdentificationModbusRequest

/**
 * @fn uint8_t InputParameterVerification(uint16_t u16StartCoilOrReg, uint16_t u16NumberOfcoilsOrReg,
		uint8_t u8UnitID, void* pFunCallBack, uint8_t u8FunctionCode, uint8_t u8ByteCount)

 * @brief This function validates input parameters received from ModbusApp for read coil or register request.
 * Checks whether required parameters are available and their values are within specified ranges.
 *
 * @param u16StartCoilOrReg 	[in] uint16_t start address of data from where to perform operation
 * @param u16NumberOfcoilsOrReg [in] uint16_t number of coils or registers to perform operation on
 * @param u8UnitID 				[in] uint8_t Modbus slave device ID
 * @param pFunCallBack 			[in] void* callback function pointer pointing to a ModbusApp function
 * 							 		 which should get executed after success/failure of the operation.
 * @param u8FunctionCode 		[in] uint8_t type of operation to be performed on the Modbus slave device
 * @param u8ByteCount 			[in] uint16_t data length related to no of coils or registers
 *
 * @return uint8_t				[out] STACK_ERROR_INVALID_INPUT_PARAMETER in case of error in parameters
 * 									  received from ModbusApp
 *									  STACK_NO_ERROR in case of all parameters are valid
 *
 */
uint8_t InputParameterVerification(uint16_t u16StartCoilOrReg, uint16_t u16NumberOfcoilsOrReg,
		uint8_t u8UnitID, void* pFunCallBack, uint8_t u8FunctionCode, uint8_t u8ByteCount)
{
	//check for null pointer
	if (NULL == pFunCallBack)
	{
		//printf("STACK_ERROR_INVALID_INPUT_PARAMETER 0");
		return STACK_ERROR_INVALID_INPUT_PARAMETER;
	}

	//Maximum allowed slave 0- 247
	if((0 == u8UnitID) || ((u8UnitID >= 248) && (u8UnitID < 255)))
	{
		//printf("STACK_ERROR_INVALID_INPUT_PARAMETER 1");
		return STACK_ERROR_INVALID_INPUT_PARAMETER;
	}
	// validate quantity
	if(ValidateQuantity(u8FunctionCode,u16NumberOfcoilsOrReg,u8ByteCount))
	{
		//printf("STACK_ERROR_INVALID_INPUT_PARAMETER 2");
		return STACK_ERROR_INVALID_INPUT_PARAMETER;
	}

	return STACK_NO_ERROR;
} // End of InputParameterVerification

/**
 * @fn MODBUS_STACK_EXPORT uint8_t Modbus_Read_Coils(uint16_t u16StartCoil,
											  uint16_t u16NumOfcoils,
											  uint16_t u16TransacID,
											  uint8_t u8UnitId,
											  long lPriority,
											  int32_t i32Ctx,
											  void* pFunCallBack)
 *
 * @brief Exported API to read coil
 * This function gets called from ModbusApp in order to execute read coil operation
 * on a Modbus slave device, which corresponds to Modbus function code 1 (i.e. 0x01).
 *
 * The very first operation this function does is, captures the current time (as a time
 * when this request is received in Modbus stack from ModbusApp). Then the function validates
 * inputs received from ModbusApp. If inputs are valid, it emplaces this request in request manager's list
 * along with the current time stamp captured immediately after entering the function.
 * After that function prepares a request header in the standard format of Modbus request header;
 * then fills up all the details required to create a Modbus request. Later it copies this modbus request
 * in Linux message queue for further processing.
 *
 * @param u16StartCoil 	[in] uint16_t start address in data from where to read coil
 * @param u16NumOfcoils [in] uint16_t number of coils to read from specified Modbus slave device
 * @param u16TransacID 	[in] uint16_t ID of the request sent to the Modbus slave device
 * @param u8UnitId 		[in] uint8_t Modbus slave device ID
 * @param lPriority		[in] long Priority to be assigned to this request. Lower the number, higher the priority
 * 							 and faster the execution.
 * @param i32Ctx 		[in] int32_t TCP/RTU context
 * @param pFunCallBack 	[in] void* callback function pointer pointing to a ModbusApp function
 * 							 which should get executed after success/failure of the operation.
 *
 * @return uint8_t		[out] STACK_ERROR_INVALID_INPUT_PARAMETER in case of error in parameters
 * 									  received from ModbusApp
 * 							  STACK_ERROR_MAX_REQ_SENT in case if Modbus stack has already sent maximum
 * 							  		  number of requests than can be added in the request manager's list
 * 							  STACK_ERROR_PACKET_LENGTH_EXCEEDED if request is longer than standard Modbus request
 * 							  		  length
 * 							  STACK_ERROR_QUEUE_SEND in case if function fails to copy this message in
 * 							  		  Linux message queue
 *							  STACK_NO_ERROR in case of successful execution of the operation
 *
 */
MODBUS_STACK_EXPORT uint8_t Modbus_Read_Coils(uint16_t u16StartCoil,
											  uint16_t u16NumOfcoils,
											  uint16_t u16TransacID,
											  uint8_t u8UnitId,
											  long lPriority,
											  int32_t i32Ctx,
											  void* pFunCallBack)
{
	uint8_t	u8ReturnType = STACK_NO_ERROR;
	uint16_t u16PacketIndex = 0;
	uint8_t u8FunctionCode = READ_COIL_STATUS;
	uint16_t u16HeaderLength = 0;
	stEndianess_t stEndianess = { 0 };
	stMbusPacketVariables_t *pstMBusRequesPacket = NULL;
	Post_Thread_Msg_t stPostThreadMsg = { 0 };
	struct timespec tsReqRcvd = (struct timespec){0};

	// Init req rcvd timestamp
	timespec_get(&tsReqRcvd, TIME_UTC);

	//Input parameter verification for read coils
	u8ReturnType = InputParameterVerification(u16StartCoil, u16NumOfcoils, u8UnitId, pFunCallBack, u8FunctionCode,0);
	if(STACK_NO_ERROR != u8ReturnType)
	{
		return u8ReturnType;
	}
	// create request for read coils initiated from app
	pstMBusRequesPacket = emplaceNewRequest(tsReqRcvd);

	if(NULL == pstMBusRequesPacket)
	{
		return STACK_ERROR_MAX_REQ_SENT;
	}
	// assign transaction ID
	pstMBusRequesPacket->m_u16AppTxID = u16TransacID;
#ifdef MODBUS_STACK_TCPIP_ENABLED
	u16HeaderLength = 6;
#endif
	// create a request header in Modbus header format
	u16PacketIndex = CreateHeaderForModbusRequest(	u16StartCoil,
													u8UnitId,
													u16HeaderLength,
													pstMBusRequesPacket->m_ulMyId,
													u8FunctionCode,
													pstMBusRequesPacket);

	stEndianess.u16word = u16NumOfcoils;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;

	pstMBusRequesPacket->m_stMbusTxData.m_u16Length = (u16PacketIndex);

	pstMBusRequesPacket->pFunc = pFunCallBack;

#ifndef MODBUS_STACK_TCPIP_ENABLED
	pstMBusRequesPacket->m_u8ReceivedDestination = u8UnitId;
#endif

	pstMBusRequesPacket->m_u16StartAdd = u16StartCoil;
	pstMBusRequesPacket->m_u16Quantity = u16NumOfcoils;
	pstMBusRequesPacket->m_lPriority = lPriority;
	stPostThreadMsg.idThread = i32Ctx;
	stPostThreadMsg.lParam = pstMBusRequesPacket;
	stPostThreadMsg.wParam = NULL;
	stPostThreadMsg.MsgType = lPriority;

	// Post the request into message queue
	if(!OSAL_Post_Message(&stPostThreadMsg))
	{
		u8ReturnType = STACK_ERROR_QUEUE_SEND;
		freeReqNode(pstMBusRequesPacket);
	}

	return u8ReturnType;
} // End of Modbus_Read_Coils

/**
 * @fn  MODBUS_STACK_EXPORT uint8_t Modbus_Read_Discrete_Inputs(uint16_t u16StartDI,
														uint16_t u16NumOfDI,
														uint16_t u16TransacID,
														uint8_t u8UnitId,
														long lPriority,
														int32_t i32Ctx,
														void* pFunCallBack)
 *
 * @brief Exported API to read discrete inputs
 * This function gets called from ModbusApp in order to execute read discrete inputs operation
 * on a Modbus slave device, which corresponds to Modbus function code 2 (i.e. 0x02).
 *
 * The very first operation this function does is, captures the current time (as a time
 * when this request is received in Modbus stack from ModbusApp). Then the function validates
 * inputs received from ModbusApp. If inputs are valid, it emplaces this request in request manager's list
 * along with the current time stamp captured immediately after entering the function.
 * After that function prepares a request header in the standard format of Modbus request header;
 * then fills up all the details required to create a Modbus request. Later it copies this modbus request
 * in Linux message queue for further processing.
 *
 * @param u16StartDI 	[in] uint16_t start address in data from where to read discrete inputs
 * @param u16NumOfDI 	[in] uint16_t number of discrete inputs to read from specified Modbus slave device
 * @param u16TransacID 	[in] uint16_t ID of the request sent to the Modbus slave device
 * @param u8UnitId 		[in] uint8_t Modbus slave device ID
 * @param lPriority		[in] long Priority to be assigned to this request. Lower the number, higher the priority
 * 							 and faster the execution.
 * @param i32Ctx 		[in] int32_t TCP/RTU context
 * @param pFunCallBack 	[in] void* callback function pointer pointing to a ModbusApp function
 * 							 which should get executed after success/failure of the operation.
 *
 * @return uint8_t		[out] STACK_ERROR_INVALID_INPUT_PARAMETER in case of error in parameters
 * 									  received from ModbusApp
 * 							  STACK_ERROR_MAX_REQ_SENT in case if Modbus stack has already sent maximum
 * 							  		  number of requests than can be added in the request manager's list
 * 							  STACK_ERROR_PACKET_LENGTH_EXCEEDED if request is longer than standard Modbus request
 * 							  		  length
 * 							  STACK_ERROR_QUEUE_SEND in case if function fails to copy this message in
 * 							  		  Linux message queue
 *							  STACK_NO_ERROR in case of successful execution of the operation
 *
 */
MODBUS_STACK_EXPORT uint8_t Modbus_Read_Discrete_Inputs(uint16_t u16StartDI,
														uint16_t u16NumOfDI,
														uint16_t u16TransacID,
														uint8_t u8UnitId,
														long lPriority,
														int32_t i32Ctx,
														void* pFunCallBack)
{
	uint8_t	u8ReturnType = STACK_NO_ERROR;
	uint16_t u16PacketIndex = 0;
	uint8_t u8FunctionCode = READ_INPUT_STATUS;
	uint16_t u16HeaderLength = 0;
	stEndianess_t stEndianess = { 0 };
	stMbusPacketVariables_t *pstMBusRequesPacket = NULL;
	Post_Thread_Msg_t stPostThreadMsg = { 0 };
	struct timespec tsReqRcvd = (struct timespec){0};

	// Init req rcvd timestamp
	timespec_get(&tsReqRcvd, TIME_UTC);

	// verify the input paramters for descrete inputs
	u8ReturnType = InputParameterVerification(u16StartDI, u16NumOfDI, u8UnitId, pFunCallBack, u8FunctionCode, 0);
	if(STACK_NO_ERROR != u8ReturnType)
	{
		return u8ReturnType;
	}

	pstMBusRequesPacket = emplaceNewRequest(tsReqRcvd);
	if(NULL == pstMBusRequesPacket)
	{
		return STACK_ERROR_MAX_REQ_SENT;
	}
	pstMBusRequesPacket->m_u16AppTxID = u16TransacID;

#ifdef MODBUS_STACK_TCPIP_ENABLED
	u16HeaderLength = 6;
#endif

	u16PacketIndex = CreateHeaderForModbusRequest(	u16StartDI,
													u8UnitId,
													u16HeaderLength,
													pstMBusRequesPacket->m_ulMyId,
													u8FunctionCode,
													pstMBusRequesPacket);

	stEndianess.u16word = u16NumOfDI;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;

	pstMBusRequesPacket->m_stMbusTxData.m_u16Length = (u16PacketIndex);

	pstMBusRequesPacket->pFunc = pFunCallBack;

#ifndef MODBUS_STACK_TCPIP_ENABLED
	pstMBusRequesPacket->m_u8ReceivedDestination = u8UnitId;
#endif

	pstMBusRequesPacket->m_u16StartAdd = u16StartDI;

	pstMBusRequesPacket->m_u16Quantity = u16NumOfDI;

	/*if(u16PacketIndex >= TCP_MODBUS_ADU_LENGTH)
	{
		freeReqNode(pstMBusRequesPacket);
		return STACK_ERROR_PACKET_LENGTH_EXCEEDED;
	}*/

	pstMBusRequesPacket->m_lPriority = lPriority;
	stPostThreadMsg.idThread = i32Ctx;
	stPostThreadMsg.lParam = pstMBusRequesPacket;
	stPostThreadMsg.wParam = NULL;
	stPostThreadMsg.MsgType = lPriority;

	// Post the request into message queue
	if(!OSAL_Post_Message(&stPostThreadMsg))
	{
		u8ReturnType = STACK_ERROR_QUEUE_SEND;
		freeReqNode(pstMBusRequesPacket);
	}

	return u8ReturnType;

} // End of Modbus_Read_Discrete_Inputs

/**
 * @fn MODBUS_STACK_EXPORT uint8_t Modbus_Read_Holding_Registers(uint16_t u16StartReg,
														  uint16_t u16NumberOfRegisters,
														  uint16_t u16TransacID,
														  uint8_t u8UnitId,
														  long lPriority,
														  int32_t i32Ctx,
														  void* pFunCallBack)
 *
 * @brief This function gets called from ModbusApp in order to execute read holding registers operation
 * on a Modbus slave device, which corresponds to Modbus function code 3 (i.e. 0x03).
 *
 * The very first operation this function does is, captures the current time (as a time
 * when this request is received in Modbus stack from ModbusApp). Then the function validates
 * inputs received from ModbusApp. If inputs are valid, it emplaces this request in request manager's list
 * along with the current time stamp captured immediately after entering the function.
 * After that function prepares a request header in the standard format of Modbus request header;
 * then fills up all the details required to create a Modbus request. Later it copies this modbus request
 * in Linux message queue for further processing.
 *
 * @param u16StartReg 			[in] uint16_t start address in data from where to read holding registers
 * @param u16NumberOfRegisters 	[in] uint16_t number of registers to read from specified Modbus slave device
 * @param u16TransacID 			[in] uint16_t ID of the request sent to the Modbus slave device
 * @param u8UnitId 				[in] uint8_t Modbus slave device ID
 * @param lPriority				[in] long Priority to be assigned to this request. Lower the number, higher the priority
 * 							 		 and faster the execution.
 * @param i32Ctx 				[in] int32_t TCP/RTU context
 * @param pFunCallBack 			[in] void* callback function pointer pointing to a ModbusApp function
 * 							 		 which should get executed after success/failure of the operation.
 *
  * @return uint8_t		[out] STACK_ERROR_INVALID_INPUT_PARAMETER in case of error in parameters
 * 									  received from ModbusApp
 * 							  STACK_ERROR_MAX_REQ_SENT in case if Modbus stack has already sent maximum
 * 							  		  number of requests than can be added in the request manager's list
 * 							  STACK_ERROR_PACKET_LENGTH_EXCEEDED if request is longer than standard
 * 							  Modbus request length
 * 							  STACK_ERROR_QUEUE_SEND in case if function fails to copy this message in
 * 							  		  Linux message queue
 *							  STACK_NO_ERROR in case of successful execution of the operation
 *
 */
MODBUS_STACK_EXPORT uint8_t Modbus_Read_Holding_Registers(uint16_t u16StartReg,
														  uint16_t u16NumberOfRegisters,
														  uint16_t u16TransacID,
														  uint8_t u8UnitId,
														  long lPriority,
														  int32_t i32Ctx,
														  void* pFunCallBack)
{
	uint8_t	u8ReturnType = STACK_NO_ERROR;
	uint16_t u16PacketIndex = 0;
	uint8_t u8FunctionCode = READ_HOLDING_REG;
	uint16_t u16HeaderLength = 0;
	stEndianess_t stEndianess = { 0 };
	stMbusPacketVariables_t *pstMBusRequesPacket = NULL;
	Post_Thread_Msg_t stPostThreadMsg = { 0 };
	struct timespec tsReqRcvd = (struct timespec){0};

	// Init req rcvd timestamp
	timespec_get(&tsReqRcvd, TIME_UTC);

	u8ReturnType = InputParameterVerification(u16StartReg, u16NumberOfRegisters, u8UnitId, pFunCallBack, u8FunctionCode,0);
	if(STACK_NO_ERROR != u8ReturnType)
	{
		return u8ReturnType;
	}

	pstMBusRequesPacket = emplaceNewRequest(tsReqRcvd);

	if(NULL == pstMBusRequesPacket)
	{
		return STACK_ERROR_MAX_REQ_SENT;
	}

	pstMBusRequesPacket->m_u16AppTxID = u16TransacID;
#ifdef MODBUS_STACK_TCPIP_ENABLED
	u16HeaderLength = 6;
#endif
	u16PacketIndex = CreateHeaderForModbusRequest(u16StartReg,
													u8UnitId,
													u16HeaderLength,
													pstMBusRequesPacket->m_ulMyId,
													u8FunctionCode,
													pstMBusRequesPacket);

	stEndianess.u16word = u16NumberOfRegisters;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;

	pstMBusRequesPacket->m_stMbusTxData.m_u16Length = (u16PacketIndex);

	pstMBusRequesPacket->pFunc = pFunCallBack;

#ifndef MODBUS_STACK_TCPIP_ENABLED
	pstMBusRequesPacket->m_u8ReceivedDestination = u8UnitId;
#endif
	pstMBusRequesPacket->m_u16StartAdd = u16StartReg;

	pstMBusRequesPacket->m_u16Quantity = u16NumberOfRegisters;

	/*if(u16PacketIndex >= TCP_MODBUS_ADU_LENGTH)
	{
		freeReqNode(pstMBusRequesPacket);
		return STACK_ERROR_PACKET_LENGTH_EXCEEDED;
	}*/

	pstMBusRequesPacket->m_lPriority = lPriority;
	stPostThreadMsg.idThread = i32Ctx;
	stPostThreadMsg.lParam = pstMBusRequesPacket;
	stPostThreadMsg.wParam = NULL;
	stPostThreadMsg.MsgType = lPriority;

	if(!OSAL_Post_Message(&stPostThreadMsg))
	{
		u8ReturnType = STACK_ERROR_QUEUE_SEND;
		freeReqNode(pstMBusRequesPacket);
	}

	return u8ReturnType;
} // End of Modbus_Read_Holding_Registers

/**
 * @fn MODBUS_STACK_EXPORT uint8_t Modbus_Read_Input_Registers(uint16_t u16StartReg,
													    uint16_t u16NumberOfRegisters,
														uint16_t u16TransacID,
														uint8_t u8UnitId,
														long lPriority,
														int32_t i32Ctx,
														void* pFunCallBack)
 *
 * @brief This function gets called from ModbusApp in order to execute read input registers operation
 * on a Modbus slave device, which corresponds to Modbus function code 4 (i.e. 0x04).
 *
 * The very first operation this function does is, captures the current time (as a time
 * when this request is received in Modbus stack from ModbusApp). Then the function validates
 * inputs received from ModbusApp. If inputs are valid, it emplaces this request in request manager's list
 * along with the current time stamp captured immediately after entering the function.
 * After that function prepares a request header in the standard format of Modbus request header;
 * then fills up all the details required to create a Modbus request. Later it copies this modbus request
 * in Linux message queue for further processing.
 *
 * @param u16StartReg 			[in] uint16_t start address in data from where to read input registers
 * @param u16NumberOfRegisters 	[in] uint16_t number of registers to read from specified Modbus slave device
 * @param u16TransacID 			[in] uint16_t ID of the request sent to the Modbus slave device
 * @param u8UnitId 				[in] uint8_t Modbus slave device ID
 * @param lPriority				[in] long Priority to be assigned to this request. Lower the number, higher the priority
 * 							         and faster the execution.
 * @param i32Ctx 				[in] int32_t TCP/RTU context
 * @param pFunCallBack 			[in] void* callback function pointer pointing to a ModbusApp function
 * 							 		 which should get executed after success/failure of the operation.
 *
 * @return uint8_t		[out] STACK_ERROR_INVALID_INPUT_PARAMETER in case of error in parameters
 * 									  received from ModbusApp
 * 							  STACK_ERROR_MAX_REQ_SENT in case if Modbus stack has already sent maximum
 * 							  		  number of requests than can be added in the request manager's list
 * 							  STACK_ERROR_PACKET_LENGTH_EXCEEDED if request is longer than standard
 * 							  Modbus request length
 * 							  STACK_ERROR_QUEUE_SEND in case if function fails to copy this message in
 * 							  		  Linux message queue
 *							  STACK_NO_ERROR in case of successful execution of the operation
 *
 */
MODBUS_STACK_EXPORT uint8_t Modbus_Read_Input_Registers(uint16_t u16StartReg,
													    uint16_t u16NumberOfRegisters,
														uint16_t u16TransacID,
														uint8_t u8UnitId,
														long lPriority,
														int32_t i32Ctx,
														void* pFunCallBack)
{
	uint8_t	u8ReturnType = STACK_NO_ERROR;
	uint16_t u16PacketIndex = 0;
	uint8_t u8FunctionCode = READ_INPUT_REG;
	uint16_t u16HeaderLength = 0;

	stEndianess_t stEndianess = { 0 };
	stMbusPacketVariables_t *pstMBusRequesPacket = NULL;
	Post_Thread_Msg_t stPostThreadMsg = { 0 };
	struct timespec tsReqRcvd = (struct timespec){0};

	// Init req rcvd timestamp
	timespec_get(&tsReqRcvd, TIME_UTC);

	u8ReturnType = InputParameterVerification(u16StartReg, u16NumberOfRegisters, u8UnitId, pFunCallBack, u8FunctionCode, 0);
	if(STACK_NO_ERROR != u8ReturnType)
	{
		return u8ReturnType;
	}
	pstMBusRequesPacket = emplaceNewRequest(tsReqRcvd);

	if(NULL == pstMBusRequesPacket)
	{
		return STACK_ERROR_MAX_REQ_SENT;
	}

	pstMBusRequesPacket->m_u16AppTxID = u16TransacID;

#ifdef MODBUS_STACK_TCPIP_ENABLED
	u16HeaderLength = 6;
#endif

	u16PacketIndex = CreateHeaderForModbusRequest(	u16StartReg,
													u8UnitId,
													u16HeaderLength,
													pstMBusRequesPacket->m_ulMyId,
													u8FunctionCode,
													pstMBusRequesPacket);

	stEndianess.u16word = u16NumberOfRegisters;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;

	pstMBusRequesPacket->m_stMbusTxData.m_u16Length = (u16PacketIndex);

	pstMBusRequesPacket->pFunc = pFunCallBack;


#ifndef MODBUS_STACK_TCPIP_ENABLED
	pstMBusRequesPacket->m_u8ReceivedDestination = u8UnitId;
#endif
	pstMBusRequesPacket->m_u16StartAdd = u16StartReg;

	pstMBusRequesPacket->m_u16Quantity = u16NumberOfRegisters;

	/*if(u16PacketIndex >= TCP_MODBUS_ADU_LENGTH)
	{
		freeReqNode(pstMBusRequesPacket);
		return STACK_ERROR_PACKET_LENGTH_EXCEEDED;
	}*/

	pstMBusRequesPacket->m_lPriority = lPriority;
	stPostThreadMsg.idThread = i32Ctx;
	stPostThreadMsg.lParam = pstMBusRequesPacket;
	stPostThreadMsg.wParam = NULL;
	stPostThreadMsg.MsgType = lPriority;

	// Post the request into message queue
	if(!OSAL_Post_Message(&stPostThreadMsg))
	{
		u8ReturnType = STACK_ERROR_QUEUE_SEND;
		freeReqNode(pstMBusRequesPacket);
	}

	return u8ReturnType;
} // End of Modbus_Read_Input_Registers

/**
 * @fn MODBUS_STACK_EXPORT uint8_t Modbus_Write_Single_Coil(uint16_t u16StartCoil,
													 uint16_t u16OutputVal,
													 uint16_t u16TransacID,
													 uint8_t u8UnitId,
													 long lPriority,
													 int32_t i32Ctx,
													 void* pFunCallBack)
 *
 * @brief This function gets called from ModbusApp in order to execute write single coil operation
 * on a Modbus slave device, which corresponds to Modbus function code 5 (i.e. 0x05).
 *
 * The very first operation this function does is, captures the current time (as a time
 * when this request is received in Modbus stack from ModbusApp). Then the function validates
 * inputs received from ModbusApp. If inputs are valid, it emplaces this request in request manager's list
 * along with the current time stamp captured immediately after entering the function.
 * After that function prepares a request header in the standard format of Modbus request header;
 * then fills up all the details required to create a Modbus request. Later it copies this modbus request
 * in Linux message queue for further processing.
 *
 * @param u16StartCoil 		[in] uint16_t start address in data from where to write coil
 * @param u16OutputVal 		[in] uint16_t value to write in the coil
 * @param u16TransacID 		[in] uint16_t ID of the request sent to the Modbus slave device
 * @param u8UnitId 			[in] uint8_t Modbus slave device ID
 * @param lPriority			[in] long Priority to be assigned to this request. Lower the number, higher the priority
 * 							 	 and faster the execution.
 * @param i32Ctx 			[in] int32_t TCP/RTU context
 * @param pFunCallBack 		[in] void* callback function pointer pointing to a ModbusApp function
 * 							 	 which should get executed after success/failure of the operation.
 *
 * @return uint8_t		[out] STACK_ERROR_INVALID_INPUT_PARAMETER in case of error in parameters
 * 									  received from ModbusApp
 * 							  STACK_ERROR_MAX_REQ_SENT in case if Modbus stack has already sent maximum
 * 							  		  number of requests than can be added in the request manager's list
 * 							  STACK_ERROR_PACKET_LENGTH_EXCEEDED if request is longer than standard
 * 							  Modbus request length
 * 							  STACK_ERROR_QUEUE_SEND in case if function fails to copy this message in
 * 							  		  Linux message queue
 *							  STACK_NO_ERROR in case of successful execution of the operation
 *
 */
MODBUS_STACK_EXPORT uint8_t Modbus_Write_Single_Coil(uint16_t u16StartCoil,
													 uint16_t u16OutputVal,
													 uint16_t u16TransacID,
													 uint8_t u8UnitId,
													 long lPriority,
													 int32_t i32Ctx,
													 void* pFunCallBack)
{
	uint8_t	u8ReturnType = STACK_NO_ERROR;
	uint16_t u16PacketIndex = 0;
	uint8_t u8FunctionCode = WRITE_SINGLE_COIL;
	uint16_t u16HeaderLength = 0;
	stEndianess_t stEndianess = { 0 };
	stMbusPacketVariables_t *pstMBusRequesPacket = NULL;
	Post_Thread_Msg_t stPostThreadMsg = { 0 };
	struct timespec tsReqRcvd = (struct timespec){0};

	// Init req rcvd timestamp
	timespec_get(&tsReqRcvd, TIME_UTC);

	u8ReturnType = InputParameterVerification(u16StartCoil,u16OutputVal, u8UnitId, pFunCallBack, u8FunctionCode,0);
	if(STACK_NO_ERROR != u8ReturnType)
	{
		return u8ReturnType;
	}

	pstMBusRequesPacket = emplaceNewRequest(tsReqRcvd);

	if(NULL == pstMBusRequesPacket)
	{
		return STACK_ERROR_MAX_REQ_SENT;
	}

	pstMBusRequesPacket->m_u16AppTxID = u16TransacID;
#ifdef MODBUS_STACK_TCPIP_ENABLED
	u16HeaderLength = 6;
#endif
	u16PacketIndex = CreateHeaderForModbusRequest(	u16StartCoil,
													u8UnitId,
													u16HeaderLength,
													pstMBusRequesPacket->m_ulMyId,
													u8FunctionCode,
													pstMBusRequesPacket);

	stEndianess.u16word = u16OutputVal;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;

	pstMBusRequesPacket->m_stMbusTxData.m_u16Length = (u16PacketIndex);

	pstMBusRequesPacket->pFunc = pFunCallBack;


#ifndef MODBUS_STACK_TCPIP_ENABLED
	pstMBusRequesPacket->m_u8ReceivedDestination = u8UnitId;
#endif

	/*if(u16PacketIndex >= TCP_MODBUS_ADU_LENGTH)
	{
		freeReqNode(pstMBusRequesPacket);
		return STACK_ERROR_PACKET_LENGTH_EXCEEDED;
	}*/

	pstMBusRequesPacket->m_lPriority = lPriority;
	stPostThreadMsg.idThread = i32Ctx;
	stPostThreadMsg.lParam = pstMBusRequesPacket;
	stPostThreadMsg.wParam = NULL;
	stPostThreadMsg.MsgType = lPriority;

	if(!OSAL_Post_Message(&stPostThreadMsg))
	{
		u8ReturnType = STACK_ERROR_QUEUE_SEND;
		freeReqNode(pstMBusRequesPacket);
	}

	return u8ReturnType;
} // End of Modbus_Write_Single_Coil

/**
 * @fn MODBUS_STACK_EXPORT uint8_t Modbus_Write_Single_Register(uint16_t u16StartReg,
														 uint16_t u16RegOutputVal,
														 uint16_t u16TransacID,
														 uint8_t u8UnitId,
														 long lPriority,
														 int32_t i32Ctx,
														 void* pFunCallBack)
 *
 * @brief This function gets called from ModbusApp in order to execute write a register operation
 * on a Modbus slave device, which corresponds to Modbus function code 6 (i.e. 0x06).
 *
 * The very first operation this function does is, captures the current time (as a time
 * when this request is received in Modbus stack from ModbusApp). Then the function validates
 * inputs received from ModbusApp. If inputs are valid, it emplaces this request in request manager's list
 * along with the current time stamp captured immediately after entering the function.
 * After that function prepares a request header in the standard format of Modbus request header;
 * then fills up all the details required to create a Modbus request. Later it copies this modbus request
 * in Linux message queue for further processing.
 *
 * @param u16StartReg 		[in] uint16_t start address in data from where to write a register
 * @param u16RegOutputVal 	[in] uint16_t value to write for specified register
 * @param u16TransacID 		[in] uint16_t ID of the request sent to the Modbus slave device
 * @param u8UnitId 			[in] uint8_t Modbus slave device ID[in] uint8_t Modbus slave device ID
 * @param lPriority			[in] long Priority to be assigned to this request. Lower the number, higher the priority
 * 							 	 and faster the execution.
 * @param i32Ctx 			[in] int32_t TCP/RTU context
 * @param pFunCallBack 		[in] void* callback function pointer pointing to a ModbusApp function
 * 							 	 which should get executed after success/failure of the operation.
 *
 * @return uint8_t		[out] STACK_ERROR_INVALID_INPUT_PARAMETER in case of error in parameters
 * 									  received from ModbusApp
 * 							  STACK_ERROR_MAX_REQ_SENT in case if Modbus stack has already sent maximum
 * 							  		  number of requests than can be added in the request manager's list
 * 							  STACK_ERROR_PACKET_LENGTH_EXCEEDED if request is longer than standard
 * 							  Modbus request length
 * 							  STACK_ERROR_QUEUE_SEND in case if function fails to copy this message in
 * 							  		  Linux message queue
 *							  STACK_NO_ERROR in case of successful execution of the operation
 *
 */
MODBUS_STACK_EXPORT uint8_t Modbus_Write_Single_Register(uint16_t u16StartReg,
														 uint16_t u16RegOutputVal,
														 uint16_t u16TransacID,
														 uint8_t u8UnitId,
														 long lPriority,
														 int32_t i32Ctx,
														 void* pFunCallBack)
{
	uint8_t	u8ReturnType = STACK_NO_ERROR;
	uint16_t u16PacketIndex = 0;
	uint8_t u8FunctionCode = WRITE_SINGLE_REG;
	uint16_t u16HeaderLength = 0;
	stEndianess_t stEndianess = { 0 };
	stMbusPacketVariables_t *pstMBusRequesPacket = NULL;
	Post_Thread_Msg_t stPostThreadMsg = { 0 };
	struct timespec tsReqRcvd = (struct timespec){0};

	// Init req rcvd timestamp
	timespec_get(&tsReqRcvd, TIME_UTC);

	u8ReturnType = InputParameterVerification(u16StartReg, u16RegOutputVal, u8UnitId, pFunCallBack, u8FunctionCode,0);
	if(STACK_NO_ERROR != u8ReturnType)
	{
		return u8ReturnType;
	}
	pstMBusRequesPacket = emplaceNewRequest(tsReqRcvd);

	if(NULL == pstMBusRequesPacket)
	{
		return STACK_ERROR_MAX_REQ_SENT;
	}

	pstMBusRequesPacket->m_u16AppTxID = u16TransacID;

#ifdef MODBUS_STACK_TCPIP_ENABLED
	u16HeaderLength = 6;
#endif
	u16PacketIndex = CreateHeaderForModbusRequest(	u16StartReg,
													u8UnitId,
													u16HeaderLength,
													pstMBusRequesPacket->m_ulMyId,
													u8FunctionCode,
													pstMBusRequesPacket);

	stEndianess.u16word = u16RegOutputVal;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;

	pstMBusRequesPacket->m_stMbusTxData.m_u16Length = (u16PacketIndex);

	pstMBusRequesPacket->pFunc = pFunCallBack;


#ifndef MODBUS_STACK_TCPIP_ENABLED
	pstMBusRequesPacket->m_u8ReceivedDestination = u8UnitId;
#endif
	/*if(u16PacketIndex >= TCP_MODBUS_ADU_LENGTH)
	{
		freeReqNode(pstMBusRequesPacket);
		return STACK_ERROR_PACKET_LENGTH_EXCEEDED;
	}*/

	pstMBusRequesPacket->m_lPriority = lPriority;
	stPostThreadMsg.idThread = i32Ctx;
	stPostThreadMsg.lParam = pstMBusRequesPacket;
	stPostThreadMsg.wParam = NULL;
	stPostThreadMsg.MsgType = lPriority;

	if(!OSAL_Post_Message(&stPostThreadMsg))
	{
		u8ReturnType = STACK_ERROR_QUEUE_SEND;
		//free(pstMBusRequesPacket);
		freeReqNode(pstMBusRequesPacket);
	}

	return u8ReturnType;
} // End of Modbus_Write_Single_Register

/**
 * @fn MODBUS_STACK_EXPORT uint8_t Modbus_Write_Multiple_Coils(uint16_t u16Startcoil,
									   uint16_t u16NumOfCoil,
									   uint16_t u16TransacID,
									   uint8_t  *pu8OutputVal,
									   uint8_t  u8UnitId,
									   long lPriority,
									   int32_t i32Ctx,
									   void*    pFunCallBack)
 *
 *
 * @brief This function gets called from ModbusApp in order to execute write multiple coils operation
 * on a Modbus slave device, which corresponds to Modbus function code 15 (i.e. 0x0F).
 *
 * The very first operation this function does is, captures the current time (as a time
 * when this request is received in Modbus stack from ModbusApp). Then the function validates
 * inputs received from ModbusApp. If inputs are valid, it emplaces this request in request manager's list
 * along with the current time stamp captured immediately after entering the function.
 * After that function prepares a request header in the standard format of Modbus request header;
 * then fills up all the details required to create a Modbus request. Later it copies this modbus request
 * in Linux message queue for further processing.
 *
 * @param u16Startcoil 		[in] uint16_t start address in data from where to start writing coils
 * @param u16NumOfCoil 		[in] uint16_t number of coils to write
 * @param pu8OutputVal 		[in] uint8_t* value to write for specified coils
 * @param u16TransacID 		[in] uint16_t ID of the request sent to the Modbus slave device
 * @param u8UnitId 			[in] uint8_t Modbus slave device ID[in] uint8_t Modbus slave device ID
 * @param lPriority			[in] long Priority to be assigned to this request. Lower the number, higher the priority
 * 							 	 and faster the execution.
 * @param i32Ctx 			[in] int32_t TCP/RTU context
 * @param pFunCallBack 		[in] void* callback function pointer pointing to a ModbusApp function
 * 							 	 which should get executed after success/failure of the operation.
 *
 * @return uint8_t		[out] STACK_ERROR_INVALID_INPUT_PARAMETER in case of error in parameters
 * 									  received from ModbusApp
 * 							  STACK_ERROR_MAX_REQ_SENT in case if Modbus stack has already sent maximum
 * 							  		  number of requests than can be added in the request manager's list
 * 							  STACK_ERROR_PACKET_LENGTH_EXCEEDED if request is longer than standard
 * 							  Modbus request length
 * 							  STACK_ERROR_QUEUE_SEND in case if function fails to copy this message in
 * 							  		  Linux message queue
 *							  STACK_NO_ERROR in case of successful execution of the operation
 *
 */
MODBUS_STACK_EXPORT uint8_t Modbus_Write_Multiple_Coils(uint16_t u16Startcoil,
									   uint16_t u16NumOfCoil,
									   uint16_t u16TransacID,
									   uint8_t  *pu8OutputVal,
									   uint8_t  u8UnitId,
									   long lPriority,
									   int32_t i32Ctx,
									   void*    pFunCallBack)
{
	uint8_t	u8ReturnType = STACK_NO_ERROR;
	uint16_t u16PacketIndex = 0;
	uint8_t u8FunctionCode = WRITE_MULTIPLE_COILS;
	uint16_t u16HeaderLength = 0;
	uint8_t u8ByteCount = (0 != (u16NumOfCoil%8))?((u16NumOfCoil/8)+1):(u16NumOfCoil/8);

	stEndianess_t stEndianess = { 0 };
	stMbusPacketVariables_t *pstMBusRequesPacket = NULL;
	Post_Thread_Msg_t stPostThreadMsg = { 0 };
	struct timespec tsReqRcvd = (struct timespec){0};

	// Init req rcvd timestamp
	timespec_get(&tsReqRcvd, TIME_UTC);

	u8ReturnType = InputParameterVerification(u16Startcoil, u16NumOfCoil, u8UnitId, pFunCallBack, u8FunctionCode,u8ByteCount);
	if(STACK_NO_ERROR != u8ReturnType)
	{
		return u8ReturnType;
	}
	pstMBusRequesPacket = emplaceNewRequest(tsReqRcvd);

	if(NULL == pstMBusRequesPacket)
	{
		return STACK_ERROR_MAX_REQ_SENT;
	}
	pstMBusRequesPacket->m_u16AppTxID = u16TransacID;
#ifdef MODBUS_STACK_TCPIP_ENABLED
	u16HeaderLength = 7+u8ByteCount;
#endif
	u16PacketIndex = CreateHeaderForModbusRequest(	u16Startcoil,
													u8UnitId,
													u16HeaderLength,
													pstMBusRequesPacket->m_ulMyId,
													u8FunctionCode,
													pstMBusRequesPacket);

	stEndianess.u16word = u16NumOfCoil;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;

	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] = u8ByteCount;
	// Update data fields for multiple coils
	while(u8ByteCount > 0 && NULL != pu8OutputVal)
	{
		if(u16PacketIndex >= TCP_MODBUS_ADU_LENGTH)
		{
			freeReqNode(pstMBusRequesPacket);
			return STACK_ERROR_PACKET_LENGTH_EXCEEDED;
		}
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
				*pu8OutputVal;
		pu8OutputVal++;
		u8ByteCount--;
	}

	pstMBusRequesPacket->m_stMbusTxData.m_u16Length = (u16PacketIndex);

	pstMBusRequesPacket->pFunc = pFunCallBack;


#ifndef MODBUS_STACK_TCPIP_ENABLED
	pstMBusRequesPacket->m_u8ReceivedDestination = u8UnitId;
#endif
	if(u16PacketIndex >= TCP_MODBUS_ADU_LENGTH)
	{
		freeReqNode(pstMBusRequesPacket);
		return STACK_ERROR_PACKET_LENGTH_EXCEEDED;
	}

	pstMBusRequesPacket->m_lPriority = lPriority;
	stPostThreadMsg.idThread = i32Ctx;
	stPostThreadMsg.lParam = pstMBusRequesPacket;
	stPostThreadMsg.wParam = NULL;
	stPostThreadMsg.MsgType = lPriority;

	if(!OSAL_Post_Message(&stPostThreadMsg))
	{
		u8ReturnType = STACK_ERROR_QUEUE_SEND;
		freeReqNode(pstMBusRequesPacket);
	}

	return u8ReturnType;
} // End of Modbus_Write_Multiple_Coils

/**
 * @fn MODBUS_STACK_EXPORT uint8_t Modbus_Write_Multiple_Register(uint16_t u16StartReg,
									   uint16_t u16NumOfReg,
									   uint16_t u16TransacID,
									   uint8_t  *pu8OutputVal,
									   uint8_t  u8UnitId,
									   long lPriority,
									   int32_t i32Ctx,
									   void*    pFunCallBack)
 *
 *
 * @brief This function gets called from ModbusApp in order to execute write multiple registers operation
 * on a Modbus slave device, which corresponds to Modbus function code 16 (i.e. 0x10).
 *
 * The very first operation this function does is, captures the current time (as a time
 * when this request is received in Modbus stack from ModbusApp). Then the function validates
 * inputs received from ModbusApp. If inputs are valid, it emplaces this request in request manager's list
 * along with the current time stamp captured immediately after entering the function.
 * After that function prepares a request header in the standard format of Modbus request header;
 * then fills up all the details required to create a Modbus request. Later it copies this modbus request
 * in Linux message queue for further processing.
 *
 * @param u16StartReg 		[in] uint16_t start address in data from where to start writing multiple registers
 * @param u16NumOfReg 		[in] uint16_t Number of registers to write
 * @param u16TransacID 		[in] uint16_t ID of the request sent to the Modbus slave device
 * @param pu8OutputVal 		[in] uint8_t* value to be write
 * @param u8UnitId 			[in] uint8_t Modbus slave device ID
 * @param lPriority			[in] long Priority to be assigned to this request. Lower the number, higher the priority
 * 							 	 and faster the execution.
 * @param i32Ctx 			[in] int32_t TCP/RTU context
 * @param pFunCallBack 		[in] void* callback function pointer pointing to a ModbusApp function
 * 							 	 which should get executed after success/failure of the operation.
 *
 * @return uint8_t		[out] STACK_ERROR_INVALID_INPUT_PARAMETER in case of error in parameters
 * 									  received from ModbusApp
 * 							  STACK_ERROR_MAX_REQ_SENT in case if Modbus stack has already sent maximum
 * 							  		  number of requests than can be added in the request manager's list
 * 							  STACK_ERROR_PACKET_LENGTH_EXCEEDED if request is longer than standard
 * 							  		  Modbus request length
 * 							  STACK_ERROR_QUEUE_SEND in case if function fails to copy this message in
 * 							  		  Linux message queue
 *							  STACK_NO_ERROR in case of successful execution of the operation
 *
 */
MODBUS_STACK_EXPORT uint8_t Modbus_Write_Multiple_Register(uint16_t u16StartReg,
									   uint16_t u16NumOfReg,
									   uint16_t u16TransacID,
									   uint8_t  *pu8OutputVal,
									   uint8_t  u8UnitId,
									   long lPriority,
									   int32_t i32Ctx,
									   void*    pFunCallBack)
{
	uint8_t	u8ReturnType = STACK_NO_ERROR;
	uint16_t u16PacketIndex = 0;
	uint8_t  u8ByteCount = ((u16NumOfReg * 2) > 246)?246:(u16NumOfReg * 2);
	uint8_t u8FunctionCode = WRITE_MULTIPLE_REG;
	uint16_t u16HeaderLength = 0;
	uint16_t *pu16OutputVal = (uint16_t *)pu8OutputVal;
	stEndianess_t stEndianess = { 0 };
	stMbusPacketVariables_t *pstMBusRequesPacket = NULL;
	Post_Thread_Msg_t stPostThreadMsg = { 0 };
	struct timespec tsReqRcvd = (struct timespec){0};

	// Init req rcvd timestamp
	timespec_get(&tsReqRcvd, TIME_UTC);

	u8ReturnType = InputParameterVerification(u16StartReg, u16NumOfReg, u8UnitId, pFunCallBack, u8FunctionCode,u8ByteCount);
	if(STACK_NO_ERROR != u8ReturnType)
	{
		return u8ReturnType;
	}
	pstMBusRequesPacket = emplaceNewRequest(tsReqRcvd);

	if(NULL == pstMBusRequesPacket)
	{
		return STACK_ERROR_MAX_REQ_SENT;
	}
	pstMBusRequesPacket->m_u16AppTxID = u16TransacID;
#ifdef MODBUS_STACK_TCPIP_ENABLED
	u16HeaderLength = 7+u8ByteCount;
#endif
	u16PacketIndex = CreateHeaderForModbusRequest(	u16StartReg,
													u8UnitId,
													u16HeaderLength,
													pstMBusRequesPacket->m_ulMyId,
													u8FunctionCode,
													pstMBusRequesPacket);

	stEndianess.u16word = u16NumOfReg;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;

#ifndef MODBUS_STACK_TCPIP_ENABLED
	pstMBusRequesPacket->m_u8ReceivedDestination = u8UnitId;
#endif
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] = u8ByteCount;
	// Update data fields for multiple coils
	while(u16NumOfReg > 0 && NULL != pu16OutputVal)
	{
		/*if(u16PacketIndex >= TCP_MODBUS_ADU_LENGTH)
		{
			freeReqNode(pstMBusRequesPacket);
			return STACK_ERROR_PACKET_LENGTH_EXCEEDED;
		}*/
		stEndianess.u16word = *pu16OutputVal;
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
					stEndianess.stByteOrder.u8SecondByte;
		if(u16PacketIndex >= TCP_MODBUS_ADU_LENGTH)
		{
			freeReqNode(pstMBusRequesPacket);
			return STACK_ERROR_PACKET_LENGTH_EXCEEDED;
		}
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
					stEndianess.stByteOrder.u8FirstByte;
		pu16OutputVal++;
		u16NumOfReg--;
	}

	pstMBusRequesPacket->m_stMbusTxData.m_u16Length = (u16PacketIndex);

	pstMBusRequesPacket->pFunc = pFunCallBack;

	if(u16PacketIndex >= TCP_MODBUS_ADU_LENGTH)
	{
		freeReqNode(pstMBusRequesPacket);
		return STACK_ERROR_PACKET_LENGTH_EXCEEDED;
	}

	pstMBusRequesPacket->m_lPriority = lPriority;
	stPostThreadMsg.idThread = i32Ctx;
	stPostThreadMsg.lParam = pstMBusRequesPacket;
	stPostThreadMsg.wParam = NULL;
	stPostThreadMsg.MsgType = lPriority;

	if(!OSAL_Post_Message(&stPostThreadMsg))
	{
		u8ReturnType = STACK_ERROR_QUEUE_SEND;
		freeReqNode(pstMBusRequesPacket);
	}

	return u8ReturnType;
} // End of Modbus_Write_Multiple_Register

/**
 * @fn MODBUS_STACK_EXPORT uint8_t Modbus_Read_File_Record(uint8_t u8byteCount,
													uint8_t u8FunCode,
													stMbusReadFileRecord_t *pstFileRecord,
													uint16_t u16TransacID,
													uint8_t u8UnitId,
													long lPriority,
													int32_t i32Ctx,
													void* pFunCallBack)
 *
 * @brief This function gets called from ModbusApp in order to execute read file record operation
 * on a Modbus slave device, which corresponds to Modbus function code 20 (i.e. 0x14).
 *
 * The very first operation this function does is, captures the current time (as a time
 * when this request is received in Modbus stack from ModbusApp). Then the function validates
 * inputs received from ModbusApp. If inputs are valid, it emplaces this request in request manager's list
 * along with the current time stamp captured immediately after entering the function.
 * After that function prepares a request header in the standard format of Modbus request header;
 * then fills up all the details required to create a Modbus request. Later it copies this modbus request
 * in Linux message queue for further processing.
 *
 * @param u8byteCount 		[in] uint8_t number of bytes to read
 * @param u8FunCode			[in] uint8_t operation to perform on Modbus slave device
 * @param pstFileRecord     [in] pointer to file records which are to be used for reading file records
 * @param u16TransacID 		[in] uint16_t ID of the request sent to the Modbus slave device
 * @param u8UnitId 			[in] uint8_t Modbus slave device ID
 * @param lPriority			[in] long Priority to be assigned to this request. Lower the number,
 * 								 higher the priority and faster the execution.
 * @param i32Ctx 			[in] int32_t TCP/RTU context
 * @param pFunCallBack 		[in] void* callback function pointer pointing to a ModbusApp function
 * 							 	 which should get executed after success/failure of the operation.
 *
 * @return uint8_t		[out] STACK_ERROR_INVALID_INPUT_PARAMETER in case of error in parameters
 * 									  received from ModbusApp
 * 							  STACK_ERROR_MAX_REQ_SENT in case if Modbus stack has already sent maximum
 * 							  		  number of requests than can be added in the request manager's list
 * 							  STACK_ERROR_PACKET_LENGTH_EXCEEDED if request is longer than standard
 * 							  		  Modbus request length
 * 							  STACK_ERROR_QUEUE_SEND in case if function fails to copy this message in
 * 							  		  Linux message queue
 *							  STACK_NO_ERROR in case of successful execution of the operation
 *
 */
MODBUS_STACK_EXPORT uint8_t Modbus_Read_File_Record(uint8_t u8byteCount,
													uint8_t u8FunCode,
													stMbusReadFileRecord_t *pstFileRecord,
													uint16_t u16TransacID,
													uint8_t u8UnitId,
													long lPriority,
													int32_t i32Ctx,
													void* pFunCallBack)
{
	uint8_t	u8ReturnType = STACK_NO_ERROR;
	uint16_t u16PacketIndex = 0;
	//uint8_t u8FunctionCode = u8FunCode;
	stEndianess_t stEndianess = { 0 };
	stMbusPacketVariables_t *pstMBusRequesPacket = NULL;
	Post_Thread_Msg_t stPostThreadMsg = { 0 };
	struct timespec tsReqRcvd = (struct timespec){0};

	// Init req rcvd timestamp
	timespec_get(&tsReqRcvd, TIME_UTC);

	u8ReturnType = InputParameterVerification(0,
			u8byteCount,
			u8UnitId,
			pFunCallBack,
			u8FunCode,
			0);

	if(STACK_NO_ERROR != u8ReturnType)
	{
		return u8ReturnType;
	}

	pstMBusRequesPacket = emplaceNewRequest(tsReqRcvd);

	if(NULL == pstMBusRequesPacket)
	{
		return STACK_ERROR_MAX_REQ_SENT;
	}

	pstMBusRequesPacket->m_u16AppTxID = u16TransacID;

#ifdef MODBUS_STACK_TCPIP_ENABLED
	// Transaction ID
	stEndianess.u16word = pstMBusRequesPacket->m_ulMyId;

	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;
	pstMBusRequesPacket->m_u16TransactionID = stEndianess.u16word;
#else
	pstMBusRequesPacket->m_u16TransactionID = u16TransacID;
#endif

#ifdef MODBUS_STACK_TCPIP_ENABLED
	// Protocol ID
	stEndianess.u16word = 0;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;

	// Length
	stEndianess.u16word = u8byteCount+3;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;
#endif

	// Unit Id
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =  u8UnitId;
	pstMBusRequesPacket->m_u8UnitID = u8UnitId;

	// Function Code
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] = u8FunCode;
	pstMBusRequesPacket->m_u8FunctionCode = u8FunCode;

	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] = u8byteCount;
	//Update data fields for multiple File record
	while(NULL != pstFileRecord)
	{
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++]
					= pstFileRecord->m_u8RefType;
		if(FILE_RECORD_REFERENCE_TYPE != pstFileRecord->m_u8RefType)
		{
			freeReqNode(pstMBusRequesPacket);
			return STACK_ERROR_INVALID_INPUT_PARAMETER;
		}

		stEndianess.u16word = pstFileRecord->m_u16FileNum;
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
				stEndianess.stByteOrder.u8SecondByte;
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
				stEndianess.stByteOrder.u8FirstByte;

		stEndianess.u16word = pstFileRecord->m_u16RecordNum;
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
				stEndianess.stByteOrder.u8SecondByte;
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
				stEndianess.stByteOrder.u8FirstByte;

		stEndianess.u16word = pstFileRecord->m_u16RecordLength;
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
				stEndianess.stByteOrder.u8SecondByte;
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
				stEndianess.stByteOrder.u8FirstByte;

		pstFileRecord = pstFileRecord->pstNextNode;
	}

	pstMBusRequesPacket->m_stMbusTxData.m_u16Length = (u16PacketIndex);

	pstMBusRequesPacket->pFunc = pFunCallBack;

#ifndef MODBUS_STACK_TCPIP_ENABLED
	pstMBusRequesPacket->m_u8ReceivedDestination = u8UnitId;
#endif

	if(u16PacketIndex >= TCP_MODBUS_ADU_LENGTH)
	{
		freeReqNode(pstMBusRequesPacket);
		return STACK_ERROR_PACKET_LENGTH_EXCEEDED;
	}

	pstMBusRequesPacket->m_lPriority = lPriority;
	stPostThreadMsg.idThread = i32Ctx;
	stPostThreadMsg.lParam = pstMBusRequesPacket;
	stPostThreadMsg.wParam = NULL;
	stPostThreadMsg.MsgType = lPriority;

	if(!OSAL_Post_Message(&stPostThreadMsg))
	{
		u8ReturnType = STACK_ERROR_QUEUE_SEND;
		freeReqNode(pstMBusRequesPacket);
	}

	return u8ReturnType;
} // End of Modbus_Read_File_Record

/**
 * @fn MODBUS_STACK_EXPORT uint8_t Modbus_Write_File_Record(uint8_t u8ReqDataLen,
													 uint8_t u8FunCode,
													 stWrFileSubReq_t *pstFileRecord,
													 uint16_t u16TransacID,
													 uint8_t u8UnitId,
													 long lPriority,
													 int32_t i32Ctx,
													 void* pFunCallBack)
 *
 *
 * @brief This function gets called from ModbusApp in order to execute write file record operation
 * on a Modbus slave device, which corresponds to Modbus function code 21 (i.e. 0x15).
 *
 * The very first operation this function does is, captures the current time (as a time
 * when this request is received in Modbus stack from ModbusApp). Then the function validates
 * inputs received from ModbusApp. If inputs are valid, it emplaces this request in request manager's list
 * along with the current time stamp captured immediately after entering the function.
 * After that function prepares a request header in the standard format of Modbus request header;
 * then fills up all the details required to create a Modbus request. Later it copies this modbus request
 * in Linux message queue for further processing.
 *
 * @param u8ReqDataLen 		[in] uint8_t number of bytes to write in a file record
 * @param u8FunCode			[in] uint8_t operation to perform on Modbus slave device
 * @param pstFileRecord     [in] pointer to file records to be written
 * @param u16TransacID 		[in] uint16_t ID of the request sent to the Modbus slave device
 * @param u8UnitId 			[in] uint8_t Modbus slave device ID[in] uint8_t Modbus slave device ID
 * @param lPriority			[in] long Priority to be assigned to this request. Lower the number, higher the priority
 * 							 	 and faster the execution.
 * @param i32Ctx 			[in] int32_t TCP/RTU context
 * @param pFunCallBack 		[in] void* callback function pointer pointing to a ModbusApp function
 * 							 	 which should get executed after success/failure of the operation.
 *
 * @return uint8_t		[out] STACK_ERROR_INVALID_INPUT_PARAMETER in case of error in parameters
 * 									  received from ModbusApp
 * 							  STACK_ERROR_MAX_REQ_SENT in case if Modbus stack has already sent maximum
 * 							  		  number of requests than can be added in the request manager's list
 * 							  STACK_ERROR_PACKET_LENGTH_EXCEEDED if request is longer than standard
 * 							  		  Modbus request length
 * 							  STACK_ERROR_QUEUE_SEND in case if function fails to copy this message in
 * 							  		  Linux message queue
 *							  STACK_NO_ERROR in case of successful execution of the operation
 *
 */
MODBUS_STACK_EXPORT uint8_t Modbus_Write_File_Record(uint8_t u8ReqDataLen,
													 uint8_t u8FunCode,
													 stWrFileSubReq_t *pstFileRecord,
													 uint16_t u16TransacID,
													 uint8_t u8UnitId,
													 long lPriority,
													 int32_t i32Ctx,
													 void* pFunCallBack)
{
	uint8_t	u8ReturnType = STACK_NO_ERROR;
	uint16_t u16PacketIndex = 0;
	uint8_t	u8TempCount = 0;
	stEndianess_t stEndianess = { 0 };
	stMbusPacketVariables_t *pstMBusRequesPacket = NULL;
	Post_Thread_Msg_t stPostThreadMsg = { 0 };
	struct timespec tsReqRcvd = (struct timespec){0};

	// Init req rcvd timestamp
	timespec_get(&tsReqRcvd, TIME_UTC);

	u8ReturnType = InputParameterVerification(0,
			u8ReqDataLen,
			u8UnitId,
			pFunCallBack,
			u8FunCode,
			0);

	if(STACK_NO_ERROR != u8ReturnType)
	{
		return u8ReturnType;
	}

	pstMBusRequesPacket = emplaceNewRequest(tsReqRcvd);

	if(NULL == pstMBusRequesPacket)
	{
		return STACK_ERROR_MAX_REQ_SENT;
	}

	pstMBusRequesPacket->m_u16AppTxID = u16TransacID;

#ifdef MODBUS_STACK_TCPIP_ENABLED
	// Transaction ID
	stEndianess.u16word = pstMBusRequesPacket->m_ulMyId;

	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;
	pstMBusRequesPacket->m_u16TransactionID = stEndianess.u16word;
#else
	// Transaction ID
	pstMBusRequesPacket->m_u16TransactionID = u16TransacID;
#endif

#ifdef MODBUS_STACK_TCPIP_ENABLED
	// Protocol ID
	stEndianess.u16word = 0;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;

	// Length
	stEndianess.u16word = u8ReqDataLen+3;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;
#endif
	// Unit Id
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =  u8UnitId;
	pstMBusRequesPacket->m_u8UnitID = u8UnitId;

	// Function Code
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] = u8FunCode;
	pstMBusRequesPacket->m_u8FunctionCode = u8FunCode;

	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] = u8ReqDataLen;
	// Update data fields for multiple File record
	while(NULL != pstFileRecord)
	{
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++]
					= pstFileRecord->m_u8RefType;
		if(FILE_RECORD_REFERENCE_TYPE != pstFileRecord->m_u8RefType)
		{
			freeReqNode(pstMBusRequesPacket);
			return STACK_ERROR_INVALID_INPUT_PARAMETER;
		}

		stEndianess.u16word = pstFileRecord->m_u16FileNum;
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
				stEndianess.stByteOrder.u8SecondByte;
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
				stEndianess.stByteOrder.u8FirstByte;

		stEndianess.u16word = pstFileRecord->m_u16RecNum;
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
				stEndianess.stByteOrder.u8SecondByte;
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
				stEndianess.stByteOrder.u8FirstByte;

		stEndianess.u16word = pstFileRecord->m_u16RecLen;
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
				stEndianess.stByteOrder.u8SecondByte;
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
				stEndianess.stByteOrder.u8FirstByte;

		while(u8TempCount < (pstFileRecord->m_u16RecLen))
		{
			stEndianess.u16word = pstFileRecord->m_pu16RecData[u8TempCount];
			pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
					stEndianess.stByteOrder.u8SecondByte;
			pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
					stEndianess.stByteOrder.u8FirstByte;
			u8TempCount++;
		}
		u8TempCount = 0;
		pstFileRecord = pstFileRecord->pstNextNode;
	}

	pstMBusRequesPacket->m_stMbusTxData.m_u16Length = (u16PacketIndex);

	pstMBusRequesPacket->pFunc = pFunCallBack;

#ifndef MODBUS_STACK_TCPIP_ENABLED
	pstMBusRequesPacket->m_u8ReceivedDestination = u8UnitId;
#endif

	if(u16PacketIndex >= TCP_MODBUS_ADU_LENGTH)
	{
		freeReqNode(pstMBusRequesPacket);
		return STACK_ERROR_PACKET_LENGTH_EXCEEDED;
	}

	pstMBusRequesPacket->m_lPriority = lPriority;
	stPostThreadMsg.idThread = i32Ctx;
	stPostThreadMsg.lParam = pstMBusRequesPacket;
	stPostThreadMsg.wParam = NULL;
	stPostThreadMsg.MsgType = lPriority;

	if(!OSAL_Post_Message(&stPostThreadMsg))
	{
		u8ReturnType = STACK_ERROR_QUEUE_SEND;
		freeReqNode(pstMBusRequesPacket);
	}

	return u8ReturnType;
} // End of Modbus_Write_File_Record

/**
 * @fn MODBUS_STACK_EXPORT uint8_t Modbus_Read_Write_Registers(uint16_t u16ReadRegAddress,
									uint8_t u8FunCode,
									uint16_t u16NoOfReadReg,
									uint16_t u16WriteRegAddress,
									uint16_t u16NoOfWriteReg,
									uint16_t u16TransacID,
									uint8_t *pu8OutputVal,
									uint8_t u8UnitId,
									long lPriority,
									int32_t i32Ctx,
									void* pFunCallBack)
 *
 *
 * @brief Read-write Multiple Registers API
 * This function gets called from ModbusApp in order to execute read-write multiple registers operation
 * on a Modbus slave device, which corresponds to Modbus function code 23 (i.e. 0x17).
 *
 * The very first operation this function does is, captures the current time (as a time
 * when this request is received in Modbus stack from ModbusApp). Then the function validates
 * inputs received from ModbusApp. If inputs are valid, it emplaces this request in request manager's list
 * along with the current time stamp captured immediately after entering the function.
 * After that function prepares a request header in the standard format of Modbus request header;
 * then fills up all the details required to create a Modbus request. Later it copies this modbus request
 * in Linux message queue for further processing.
 *
 * @param u16ReadRegAddress [in] uint16_t start address in data from where to start read-write multiple registers
 * @param u8FunCode			[in] uint8_t operation to perform on Modbus slave device
 * @param u16NoOfReadReg 	[in] uint16_t number of registers to read-write
 * @param u16WriteRegAddress [in] uint16_t start address in data from where to read-write registers
 * @param u16NoOfWriteReg 	[in] uint16_t number of registers to read-write
 * @param pu8OutputVal 		[in] uint8_t* value to be read-write
 * @param u16TransacID 		[in] uint16_t ID of the request sent to the Modbus slave device
 * @param u8UnitId 			[in] uint8_t Modbus slave device ID[in] uint8_t Modbus slave device ID
 * @param lPriority			[in] long Priority to be assigned to this request. Lower the number, higher the priority
 * 							 	 and faster the execution.
 * @param i32Ctx 			[in] int32_t TCP/RTU context
 * @param pFunCallBack 		[in] void* callback function pointer pointing to a ModbusApp function
 * 							 	 which should get executed after success/failure of the operation.
 *
 * @return uint8_t		[out] STACK_ERROR_INVALID_INPUT_PARAMETER in case of error in parameters
 * 									  received from ModbusApp
 * 							  STACK_ERROR_MAX_REQ_SENT in case if Modbus stack has already sent maximum
 * 							  		  number of requests than can be added in the request manager's list
 * 							  STACK_ERROR_PACKET_LENGTH_EXCEEDED if request is longer than standard
 * 							  		  Modbus request length
 * 							  STACK_ERROR_QUEUE_SEND in case if function fails to copy this message in
 * 							  		  Linux message queue
 *							  STACK_NO_ERROR in case of successful execution of the operation
 *
 */
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
									void* pFunCallBack)
{
	uint8_t	u8ReturnType = STACK_NO_ERROR;
	uint16_t u16PacketIndex = 0;
	uint8_t u8WriteByteCount = ((u16NoOfWriteReg * 2) > 244)?244:(u16NoOfWriteReg * 2);
	uint16_t *pu16OutputVal = (uint16_t *)pu8OutputVal;
	stEndianess_t stEndianess = { 0 };
	uint16_t u16HeaderLength = 0;
	stMbusPacketVariables_t *pstMBusRequesPacket = NULL;
	Post_Thread_Msg_t stPostThreadMsg = { 0 };
	struct timespec tsReqRcvd = (struct timespec){0};

	// Init req rcvd timestamp
	timespec_get(&tsReqRcvd, TIME_UTC);

	u8ReturnType = InputParameterVerification(u16ReadRegAddress,
			u16NoOfReadReg,
			u8UnitId,
			pFunCallBack,
			READ_HOLDING_REG,
			0);
		if(STACK_NO_ERROR != u8ReturnType)
	{
			return u8ReturnType;
	}
	pstMBusRequesPacket = emplaceNewRequest(tsReqRcvd);

	if(NULL == pstMBusRequesPacket)
	{
		return STACK_ERROR_MAX_REQ_SENT;
	}

	pstMBusRequesPacket->m_u16AppTxID = u16TransacID;

#ifdef MODBUS_STACK_TCPIP_ENABLED
	u16HeaderLength = 11+u8WriteByteCount;
#endif
		u16PacketIndex = CreateHeaderForModbusRequest(u16ReadRegAddress,
														u8UnitId,
														u16HeaderLength,
														pstMBusRequesPacket->m_ulMyId,
														u8FunCode,
														pstMBusRequesPacket);

	stEndianess.u16word = u16NoOfReadReg;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;

	// writing
	u8ReturnType = InputParameterVerification(u16WriteRegAddress,
			u16NoOfWriteReg,
			u8UnitId,
			pFunCallBack,
			READ_WRITE_MUL_REG,
			u8WriteByteCount);

	if(STACK_NO_ERROR != u8ReturnType)
	{
		freeReqNode(pstMBusRequesPacket);
		return u8ReturnType;
	}

	// write starting address
	stEndianess.u16word = u16WriteRegAddress;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;

	// Quantity to write
	stEndianess.u16word = u16NoOfWriteReg;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8SecondByte;
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
			stEndianess.stByteOrder.u8FirstByte;

	// calculate byte count for writing
	u8WriteByteCount = u16NoOfWriteReg * MBUS_INDEX_2;

	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] = u8WriteByteCount;
	//Update data fields for multiple registers
	while(u16NoOfWriteReg > 0 && NULL != pu8OutputVal)
	{
		/*if(u16PacketIndex >= TCP_MODBUS_ADU_LENGTH)
		{
			freeReqNode(pstMBusRequesPacket);
			return STACK_ERROR_PACKET_LENGTH_EXCEEDED;
		}*/
		stEndianess.u16word = *pu16OutputVal;
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
					stEndianess.stByteOrder.u8SecondByte;
		if(u16PacketIndex >= TCP_MODBUS_ADU_LENGTH)
		{
			freeReqNode(pstMBusRequesPacket);
			return STACK_ERROR_PACKET_LENGTH_EXCEEDED;
		}
		pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] =
					stEndianess.stByteOrder.u8FirstByte;
		pu16OutputVal++;
		u16NoOfWriteReg--;
	}

	pstMBusRequesPacket->m_stMbusTxData.m_u16Length = (u16PacketIndex);

	pstMBusRequesPacket->pFunc = pFunCallBack;

	if(NULL == pu8OutputVal && u8WriteByteCount > 0)
		u8ReturnType = STACK_ERROR_INVALID_INPUT_PARAMETER;

#ifndef MODBUS_STACK_TCPIP_ENABLED
	pstMBusRequesPacket->m_u8ReceivedDestination = u8UnitId;
#endif

	if(u16PacketIndex >= TCP_MODBUS_ADU_LENGTH)
	{
		freeReqNode(pstMBusRequesPacket);
		return STACK_ERROR_PACKET_LENGTH_EXCEEDED;
	}

	pstMBusRequesPacket->m_lPriority = lPriority;
	stPostThreadMsg.idThread = i32Ctx;
	stPostThreadMsg.lParam = pstMBusRequesPacket;
	stPostThreadMsg.wParam = NULL;
	stPostThreadMsg.MsgType = lPriority;

	if(!OSAL_Post_Message(&stPostThreadMsg))
	{
		u8ReturnType = STACK_ERROR_QUEUE_SEND;
		freeReqNode(pstMBusRequesPacket);
	}

	return u8ReturnType;

} // End of Modbus_Read_Write_Registers

/**
 * @fn MODBUS_STACK_EXPORT uint8_t Modbus_Read_Device_Identification(uint8_t u8MEIType,
		uint8_t u8FunCode,
		uint8_t u8ReadDevIdCode,
		uint8_t u8ObjectId,
		uint16_t u16TransacID,
		uint8_t u8UnitId,
		long lPriority,
		int32_t i32Ctx,
		void* pFunCallBack)
 *
 * @brief  Read Device Identification API.
 * This function gets called from ModbusApp in order to execute read device identification operation
 * on a Modbus slave device, which corresponds to Modbus function code 43 (i.e. 0x2B).
 *
 * The very first operation this function does is, captures the current time (as a time
 * when this request is received in Modbus stack from ModbusApp). Then the function validates
 * inputs received from ModbusApp. If inputs are valid, it emplaces this request in request manager's list
 * along with the current time stamp captured immediately after entering the function.
 * After that function prepares a request header in the standard format of Modbus request header;
 * then fills up all the details required to create a Modbus request. Later it copies this modbus request
 * in Linux message queue for further processing.
 *
 * @param u8MEIType 		[in] uint8_t MEI type (MODBUS encapsulated interface) type
 * @param u8FunCode			[in] uint8_t operation to perform on Modbus slave device
 * @param u8ReadDevIdCode 	[in] uint8_t read device id code
 * @param u8ObjectId 		[in] uint8_t object id
 * @param u16TransacID 		[in] uint16_t ID of the request sent to the Modbus slave device
 * @param u8UnitId 			[in] uint8_t Modbus slave device ID
 * @param lPriority			[in] long Priority to be assigned to this request. Lower the number,
 * 								 higher the priority and faster the execution.
 * @param i32Ctx 			[in] int32_t TCP/RTU context
 * @param pFunCallBack 		[in] void* callback function pointer pointing to a ModbusApp function
 * 							 	 which should get executed after success/failure of the operation.
 * @return uint8_t		[out] STACK_ERROR_INVALID_INPUT_PARAMETER in case of error in parameters
 * 									  received from ModbusApp
 * 							  STACK_ERROR_MAX_REQ_SENT in case if Modbus stack has already sent maximum
 * 							  		  number of requests than can be added in the request manager's list
 * 							  STACK_ERROR_PACKET_LENGTH_EXCEEDED if request is longer than standard
 * 							  		  Modbus request length
 * 							  STACK_ERROR_QUEUE_SEND in case if function fails to copy this message in
 * 							  		  Linux message queue
 *							  STACK_NO_ERROR in case of successful execution of the operation
 *
 */
MODBUS_STACK_EXPORT uint8_t Modbus_Read_Device_Identification(uint8_t u8MEIType,
		uint8_t u8FunCode,
		uint8_t u8ReadDevIdCode,
		uint8_t u8ObjectId,
		uint16_t u16TransacID,
		uint8_t u8UnitId,
		long lPriority,
		int32_t i32Ctx,
		void* pFunCallBack)
{
	uint8_t	u8ReturnType = STACK_NO_ERROR;
	uint16_t u16PacketIndex = 0;
	stMbusPacketVariables_t *pstMBusRequesPacket = NULL;
	Post_Thread_Msg_t stPostThreadMsg = { 0 };
	struct timespec tsReqRcvd = (struct timespec){0};
	uint16_t u16HeaderLength = 0;

	// Init req rcvd timestamp
	timespec_get(&tsReqRcvd, TIME_UTC);

	if(NULL == pFunCallBack)
	{
		return STACK_ERROR_INVALID_INPUT_PARAMETER;
	}

	// Maximum allowed slave 1- 247 or 255
	if((0 == u8UnitId) || ((u8UnitId >= 248) && (u8UnitId < 255)))
	// Maximum allowed slave 1- 247
	// if(u8UnitId > MAX_ALLOWED_SLAVES )
	{
		return STACK_ERROR_INVALID_INPUT_PARAMETER;
	}

	// Validate MEI type
	if(u8MEIType != MEI_TYPE)
	{
		return STACK_ERROR_INVALID_INPUT_PARAMETER;
	}

	// Validate read device id code type
	if((u8ReadDevIdCode == 1) && (u8ReadDevIdCode == 2) && (u8ReadDevIdCode == 3)
			&& (u8ReadDevIdCode == 4))
	{
		return STACK_ERROR_INVALID_INPUT_PARAMETER;
	}

	pstMBusRequesPacket = emplaceNewRequest(tsReqRcvd);

	if(NULL == pstMBusRequesPacket)
	{
		return STACK_ERROR_MAX_REQ_SENT;
	}

	pstMBusRequesPacket->m_u16AppTxID = u16TransacID;

#ifdef MODBUS_STACK_TCPIP_ENABLED
	// Here 5 = len of trans ID + fun code + MEI + devId + ObjeId
	u16HeaderLength = 5;
#endif
	u16PacketIndex = CreateHeaderForDevIdentificationModbusRequest(u8UnitId,
														u16HeaderLength,
														pstMBusRequesPacket->m_ulMyId,
														u8FunCode,
														pstMBusRequesPacket);

	// Fill MEI Type
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] = u8MEIType;

	// Fill Read Device ID  Code
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] = u8ReadDevIdCode;

	// Fill Object ID
	pstMBusRequesPacket->m_stMbusTxData.m_au8DataFields[u16PacketIndex++] = u8ObjectId;

	pstMBusRequesPacket->m_stMbusTxData.m_u16Length = (u16PacketIndex);

	pstMBusRequesPacket->pFunc = pFunCallBack;

#ifndef MODBUS_STACK_TCPIP_ENABLED
	pstMBusRequesPacket->m_u8ReceivedDestination = u8UnitId;
#endif

	/*if(u16PacketIndex >= TCP_MODBUS_ADU_LENGTH)
	{
		freeReqNode(pstMBusRequesPacket);
		return STACK_ERROR_PACKET_LENGTH_EXCEEDED;
	}*/

	pstMBusRequesPacket->m_lPriority = lPriority;
	stPostThreadMsg.idThread = i32Ctx;
	stPostThreadMsg.lParam = pstMBusRequesPacket;
	stPostThreadMsg.wParam = NULL;
	stPostThreadMsg.MsgType = lPriority;

	if(!OSAL_Post_Message(&stPostThreadMsg))
	{
		u8ReturnType = STACK_ERROR_QUEUE_SEND;
		freeReqNode(pstMBusRequesPacket);
	}

	return u8ReturnType;
} // End of Modbus_Read_Device_Identification

#ifndef MODBUS_STACK_TCPIP_ENABLED
/**
 * @fn bool validateBaudRate(uint32_t nBaudRate)
 *
 * @brief validateBaudRate function
 * This function validate the baud rate used for RTU communication
 *
 * @param nBaudRate			[in] uint32_t Baud rate used for RTU communication
 * @return bool				[out] true if success, false otherwise
 *
 */
bool validateBaudRate(uint32_t nBaudRate)
{
	bool isValidBaudRate = false;

	uint32_t arrBaudRate[] = {110,300,600,1200,2400,4800,9600,19200,38400,57600,115200,230400,460800,500000,576000,921600,1000000,1152000,1500000,2500000,3000000,3500000,4000000};
	int len = sizeof(arrBaudRate)/sizeof(arrBaudRate[0]);

	for(int index=0; index < len; ++index)
	{
		if(nBaudRate == arrBaudRate[index])
		{
			isValidBaudRate = true;
			break;
		}
	}

	return isValidBaudRate;
} // End of validateBaudRate
#endif

/**
 * @fn MODBUS_STACK_EXPORT eStackErrorCode getCtx(int32_t *pCtx, stCtxInfo *pCtxInfo)
 *
 * @brief This function gets called from ModbusApp to get the Context for TCP/RTU Communication
 *
 * @param pCtxInfo			[in] Combined structure for port, baudrate, parity, interframe delay
 * 							response timeout
 * @param pCtx 			    [out] int32_t* Context for TCP or RTu for communication
 * @return eStackErrorCode	[out] MODBUS_STACK_EXPORT in case of error in parameters
 * 									  received from ModbusApp
 *
 */
MODBUS_STACK_EXPORT eStackErrorCode getCtx(int32_t *pCtx, stCtxInfo *pCtxInfo)
{
	eStackErrorCode retError = STACK_NO_ERROR;
	thread_Create_t stThreadParam = { 0 };
	Thread_H threadId;
	stLiveSerSessionList_t *pstLivSerSesslist = NULL;
	uint8_t u8NewDevEntryFalg = 0;

	if(NULL == pCtx || NULL == pCtxInfo)
	{
		return STACK_ERROR_INVALID_INPUT_PARAMETER;
	}

#ifndef MODBUS_STACK_TCPIP_ENABLED
	int nPortNameLen = strnlen_s((const char*)pCtxInfo->m_u8PortName, MODBUS_DATA_LENGTH);
	if(nPortNameLen >= MODBUS_DATA_LENGTH)
	{
		return STACK_ERROR_PORT_NAME_LENGTH_EXCEEDED;
	}

	if(!validateBaudRate(pCtxInfo->m_u32baudrate))
	{
		return STACK_ERROR_INVALID_BAUD_RATE;
	}
#endif

	// Assign default value
	*pCtx = -1;
	
	do
	{
		// Loop through the list to check if context is already obtained for given parameters
		pstLivSerSesslist = pstSesCtlThdLstHead;
		while(NULL != pstLivSerSesslist)
		{
			if(NULL != pstLivSerSesslist)
			{
#ifdef MODBUS_STACK_TCPIP_ENABLED
				if(pCtxInfo->pu8SerIpAddr[0] == pstLivSerSesslist->m_u8IpAddr[0] &&
						pCtxInfo->pu8SerIpAddr[1] == pstLivSerSesslist->m_u8IpAddr[1] &&
						pCtxInfo->pu8SerIpAddr[2] == pstLivSerSesslist->m_u8IpAddr[2] &&
						pCtxInfo->pu8SerIpAddr[3] == pstLivSerSesslist->m_u8IpAddr[3] &&
						pCtxInfo->u16Port == pstLivSerSesslist->m_u16Port)
				{
					*pCtx = pstLivSerSesslist->MsgQId;
					retError = STACK_NO_ERROR;
					break;
				}
#else
				// Check if port name is same
				if(strncmp((char*)pCtxInfo->m_u8PortName, (char*)pstLivSerSesslist->m_portName, sizeof(pstLivSerSesslist->m_portName)) == 0)
				{
					// Check if other parameters are same
					if((pCtxInfo->m_u32baudrate == pstLivSerSesslist->m_baudrate &&
						pCtxInfo->m_eParity == pstLivSerSesslist->m_parity))
					{
						// Context is already available for given configuration
						*pCtx = pstLivSerSesslist->MsgQId;
						retError = STACK_NO_ERROR;
						break;
					}
					else
					{
						// Port name is same. But other configuration is wrong
						retError = STACK_ERROR_SERIAL_PORT_ALREADY_IN_USE;
						break;
					}
				}
#endif
				else
				{
					pstLivSerSesslist = pstLivSerSesslist->m_pNextElm;
				}
			}
		}
		
		// This configuration is found for the first time.
		// To be processed for new context
		if(NULL == pstLivSerSesslist)
		{
			pstLivSerSesslist = OSAL_Malloc(sizeof(stLiveSerSessionList_t));
			if(NULL == pstLivSerSesslist)
			{
				retError = STACK_ERROR_MALLOC_FAILED;
			}
			else
			{
				pstLivSerSesslist->m_pNextElm = pstSesCtlThdLstHead;
				pstSesCtlThdLstHead = pstLivSerSesslist;
				u8NewDevEntryFalg = 1;
			}
		}
		
		if(u8NewDevEntryFalg)
		{
#ifdef MODBUS_STACK_TCPIP_ENABLED
			pstLivSerSesslist->m_u8IpAddr[0] = pCtxInfo->pu8SerIpAddr[0];
			pstLivSerSesslist->m_u8IpAddr[1] = pCtxInfo->pu8SerIpAddr[1];
			pstLivSerSesslist->m_u8IpAddr[2] = pCtxInfo->pu8SerIpAddr[2];
			pstLivSerSesslist->m_u8IpAddr[3] = pCtxInfo->pu8SerIpAddr[3];
			pstLivSerSesslist->m_u16Port = pCtxInfo->u16Port;
#else
			strncpy_s((char*)pstLivSerSesslist->m_portName, sizeof(pstLivSerSesslist->m_portName),
					(const char*)pCtxInfo->m_u8PortName, nPortNameLen);
			pstLivSerSesslist->m_baudrate = pCtxInfo->m_u32baudrate;
			pstLivSerSesslist->m_parity = pCtxInfo->m_eParity;
			pstLivSerSesslist->m_lInterframeDelay = (pCtxInfo->m_lInterframeDelay) * 1000; // convert to usec
			pstLivSerSesslist->m_lrespTimeout = (pCtxInfo->m_lRespTimeout) * 1000; // convert to usec
#endif
			pstLivSerSesslist->MsgQId = OSAL_Init_Message_Queue();	// generating message Queue id
			if(-1 == pstLivSerSesslist->MsgQId)
			{
				retError = STACK_ERROR_QUEUE_CREATE;
			}
			else
			{
				stThreadParam.dwStackSize = 0;
#ifdef MODBUS_STACK_TCPIP_ENABLED
				stThreadParam.lpStartAddress = ServerSessTcpAndCbThread;
#else
				stThreadParam.lpStartAddress = SessionControlThread;
#endif
				stThreadParam.lpParameter = (void*)pstLivSerSesslist;
				stThreadParam.lpThreadId = &SessionControl_ThreadId;

				threadId = Osal_Thread_Create(&stThreadParam);
				if(0 == threadId)
				{
					retError = STACK_ERROR_THREAD_CREATE;
					OSAL_Delete_Message_Queue(pstLivSerSesslist->MsgQId);
				}
				else
				{
					retError = STACK_NO_ERROR;
					*pCtx = pstLivSerSesslist->MsgQId;
				}
			}
		}
	} while(0);
	
	// Check if any error occurred
	if(retError != STACK_NO_ERROR)
	{
		// Check if memory to be released
		if((NULL != pstLivSerSesslist) && u8NewDevEntryFalg)
		{
			// Appropraitely set the head of list
			if(pstSesCtlThdLstHead == pstLivSerSesslist)
			{
				pstSesCtlThdLstHead = pstSesCtlThdLstHead->m_pNextElm;
			}
			// Free the node
			OSAL_Free(pstLivSerSesslist);
			pstLivSerSesslist = NULL;
		}
	}

	return retError;
}

#ifdef MODBUS_STACK_TCPIP_ENABLED

/**
 * @fn MODBUS_STACK_EXPORT eStackErrorCode getTCPCtx(int *tcpCtx, stCtxInfo *pCtxInfo)
 *
 * @brief This function gets called from ModbusApp to get the TCP Context for TCP Communication
 *
 * @param pCtxInfo 			[in] uint8_t* Ip address for TCP communication and port number
 * @param tcpCtx 			[out] int* TCP Context based on ip-address and port which will be used for communication
 * @return eStackErrorCode	[out] MODBUS_STACK_EXPORT in case of error in parameters
 * 									  received from ModbusApp
 *
 */
MODBUS_STACK_EXPORT eStackErrorCode getTCPCtx(int *tcpCtx, stCtxInfo *pCtxInfo)
{
	return getCtx(tcpCtx, pCtxInfo);
} // End of getTCPCtx
#else

/**
 * @fn MODBUS_STACK_EXPORT eStackErrorCode getRTUCtx(int32_t *rtuCtx, stCtxInfo *pCtxInfo)
 *
 * @brief This function gets called from ModbusApp to get the RTU Context for RTU Communication
 *
 * @param pCtxInfo			[in] Combined structure for port, baudrate, parity, interframe delay
 * 							response timeout
 * @param rtuCtx 			[out] int32_t* RTU Context based on portname, baudrate, parity which will be used for communication
 * @return eStackErrorCode	[out] MODBUS_STACK_EXPORT in case of error in parameters
 * 									  received from ModbusApp
 *
 */
MODBUS_STACK_EXPORT eStackErrorCode getRTUCtx(int32_t *rtuCtx, stCtxInfo *pCtxInfo)
{
	return getCtx(rtuCtx, pCtxInfo);
} // End of getRTUCtx
#endif
