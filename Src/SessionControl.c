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
#include <safe_lib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdatomic.h>
#include <sys/epoll.h> // for epoll_create1(), epoll_ctl(), struct epoll_event
#include <time.h>
#include "SessionControl.h"

 /*
  ===============================================================================
  Global variable definitions
  ===============================================================================
  */

//global variable to store data of request received from node
struct stReqManager g_objReqManager;

// global variable to store stack configurations
extern stDevConfig_t g_stModbusDevConfig;

//Linux message queue ID
int32_t i32MsgQueIdSC = 0;

//flag to check whether to exit the current thread or continue
extern bool g_bThreadExit;

//pointer to session list
stLiveSerSessionList_t *pstSesCtlThdLstHead = NULL;

//structure to store response for processsing
struct stResProcessData g_stRespProcess;

/*
 ===============================================================================
 Function Declarations
 ===============================================================================
 */

//if Modbus stack communicates with Modbus slave device using TCP mode
#ifdef MODBUS_STACK_TCPIP_ENABLED

//array of structures containing socket descriptors the have been registered with epoll
//and data to be read from
stTcpRecvData_t m_clientAccepted[MAX_DEVICE_PER_SITE] = {{0}};

//initialization of epoll descriptors
int m_epollFd = 0;

//pointer to structure holding events for registered socket descriptors
struct epoll_event *m_events = NULL;

//handle of thread receiving data from socket descriptors continuously using epoll mechanism
Thread_H EpollRecv_ThreadId = 0;

//handle of mutex to synchronize epoll data
Mutex_H EPollMutex;

//structure to tract timeout requests
struct stTimeOutTracker g_oTimeOutTracker = {0};
#endif

//structure to hold data for message queue
struct stIntDataForQ
{
	//message type, must be > 0
	long m_lType;
	//message ID
	unsigned short m_iID;
};

/*
 ===============================================================================
 Function Definitions
 ===============================================================================
 */

/**
 * @fn long getAvailableReqNode(void)
 *
 * @brief This function looks for an available node in request array.
 * It reserves the available node for using this node.
 *
 * @param none
 * @return long [out] non-zero node index in long format;
 * 					   -1 in case of error
 *
 */
long getAvailableReqNode(void)
{
	// default value for last index
	static long lastIndex = -1;
	//default value
	long index = -1;

	// addressed review comment
	if(0 != Osal_Wait_Mutex(g_objReqManager.m_mutexReqArray))
	{
		// fail to lock mutex
		return -1;
	}
	//look for the valid node received in request manager array until index reaches maximum value in array
	for (long iLoop = 0; iLoop < MAX_REQUESTS; iLoop++)
	{
		//Increment the last index until max request hold by the request manager array
		++lastIndex;
		if(MAX_REQUESTS == lastIndex)
		{
			lastIndex = 0;
		}
		eTransactionState expected = IdleState;
		//create a pointer assign to the data of last index of array
		stMbusPacketVariables_t* ptr = &g_objReqManager.m_objReqArray[lastIndex];
		if(true ==
			atomic_compare_exchange_strong(&ptr->m_state, &expected, RESERVED))
		{
			//If the expected state matches with the state of data in array then return the index
			index = lastIndex;
			break;
		}
	}
	Osal_Release_Mutex(g_objReqManager.m_mutexReqArray);
	return index;
} //End of getAvailableReqNode

/**
 * @fn void resetReqNode(stMbusPacketVariables_t* a_pObjReqNode)
 *
 * @brief This function resets the request node to default values.
 *
 * @param a_pObjReqNode [in] stMbusPacketVariables_t* pointer to structure of node to reset
 *
 * @return none
 *
 */
void resetReqNode(stMbusPacketVariables_t* a_pObjReqNode)
{
	if(NULL != a_pObjReqNode)
	{
#ifdef MODBUS_STACK_TCPIP_ENABLED
		a_pObjReqNode->__next = NULL;
		a_pObjReqNode->__prev = NULL;
#endif
		// Initialize timestamps to 0
		a_pObjReqNode->m_objTimeStamps.tsReqRcvd = (struct timespec){0};
		a_pObjReqNode->m_objTimeStamps.tsReqSent = (struct timespec){0};
		a_pObjReqNode->m_objTimeStamps.tsRespRcvd = (struct timespec){0};
		a_pObjReqNode->m_objTimeStamps.tsRespSent = (struct timespec){0};
		a_pObjReqNode->m_iTimeOutIndex = -1;

		// Initialize state to idle state
		a_pObjReqNode->m_state = IdleState;
	}
	else
	{
		printf("Null pointer to reset\n");
	}
} //End of resetReqNode

/**
 * @fn bool initReqManager(void)
 *
 * @brief This function initiates the request manager and data structures within it. The request
 * manager keeps track of requests send to Modbus slave and responses received for those requests.
 *
 * @param none
 *
 * @return bool [out] true if function succeeds in initializing the request manager;
 * 					   false if function fails to initialize the request manager;
 *
 */
bool initReqManager(void)
{
	//Initialize count to 0
	unsigned int iCount = 0;

	// loop until all nodes initialize to default values
	for (; iCount < MAX_REQUESTS; iCount++)
	{
		g_objReqManager.m_objReqArray[iCount].m_ulMyId = iCount;
		// reset request node
		resetReqNode(&(g_objReqManager.m_objReqArray[iCount]));
	}
	// create mutex
	g_objReqManager.m_mutexReqArray = Osal_Mutex();
	if(NULL == g_objReqManager.m_mutexReqArray )
	{
		return false;
	}
	return true;
} // End of initReqManager

/**
 * @fn stMbusPacketVariables_t* emplaceNewRequest(const struct timespec tsReqRcvd)
 *
 * @brief This function sets data structure to process new request. It gets the available node
 * from the request manager's list and updates new request at the empty node.
 *
 * @param tsReqRcvd [in] const struct timespec time-stamp when request was received
 * @return [out] stMbusPacketVariables_t* pointer to emplaced request
 * 				 NULL in case of error
 *
 */
stMbusPacketVariables_t* emplaceNewRequest(const struct timespec tsReqRcvd)
{
	stMbusPacketVariables_t* ptr = NULL;
	// Get index of available request node in request array
	long iCount = getAvailableReqNode();
	if ((iCount >= 0) && (iCount < MAX_REQUESTS))
	{
		eTransactionState expected = RESERVED;
		ptr = &g_objReqManager.m_objReqArray[iCount];
		if(true ==
				atomic_compare_exchange_strong(&ptr->m_state, &expected, REQ_RCVD_FROM_APP))
		{
#ifdef MODBUS_STACK_TCPIP_ENABLED
			ptr->__next = NULL;
			ptr->__prev = NULL;
#endif
			ptr->m_ulMyId = iCount;

			// copy the req recvd timestamp
			memcpy_s(&(ptr->m_objTimeStamps.tsReqRcvd), sizeof(struct timespec),
					&tsReqRcvd, sizeof(struct timespec));

			// Init other timestamps to 0
			ptr->m_objTimeStamps.tsReqSent = (struct timespec){0};
			ptr->m_objTimeStamps.tsRespRcvd = (struct timespec){0};
			ptr->m_objTimeStamps.tsRespSent = (struct timespec){0};

			ptr->m_iTimeOutIndex = -1;
		}
	}
	return ptr;
} // End of emplaceNewRequest

/**
 * @fn void freeReqNode(stMbusPacketVariables_t* a_pobjReq)
 *
 * @brief This function frees a request node and resets it for use by next requests. This function
 * also releases the request from tracking for timeout.
 *
 * @param a_pobjReq [in] stMbusPacketVariables_t* request node to mark as free
 *
 * @return none
 *
 */
void freeReqNode(stMbusPacketVariables_t* a_pobjReq)
{
	// 1. remove the node from timeout tracker in case of TCP
	// 2. reset the request node elements
	// 3. mark the index as available
#ifdef MODBUS_STACK_TCPIP_ENABLED
	releaseFromTracker(a_pobjReq);
#endif
	// reset the structure
	resetReqNode(a_pobjReq);
}  //End of freeReqNode

#ifdef MODBUS_STACK_TCPIP_ENABLED
/**
 * @fn int getClientIdFromList(int socketID)
 *
 * @brief This function gets client socket id from already established client's list
 *
 * @param socketID [in] int socket id to retrieve from the list
 *
 * @return int [out] client index from list if client socket id already exists in the list;
 * 					 -1 if client socket id is not in the list
 *
 */
int getClientIdFromList(int socketID)
{
	int socketIndex = -1;

	for(int i = 0; i < MAX_DEVICE_PER_SITE; i++)
	{
		if(NULL != m_clientAccepted[i].m_pstConRef)
		{
			if(m_clientAccepted[i].m_pstConRef->m_sockfd == socketID)
			{
				return i;
			}
		}
	}

	return socketIndex;
} // resetEPollClientDataStruct

/**
 * @fn void resetEPollClientDataStruct(stTcpRecvData_t* clientAccepted)
 *
 * @brief This function resets client structure after data is read from the socket.
 *
 * @param clientAccepted [in] stTcpRecvData_t* pointer that holds socket's data received information
 *
 * @return [out] none
 */
void resetEPollClientDataStruct(stTcpRecvData_t* clientAccepted)
{
	if(NULL != clientAccepted)
	{
	clientAccepted->m_bytesRead = 0;
	clientAccepted->m_bytesToBeRead = 0;
	clientAccepted->m_len = 0;
	memset(&clientAccepted->m_readBuffer, 0,
			sizeof(clientAccepted->m_readBuffer));
	}
} // End of resetEPollClientDataStruct

/**
 * @fn bool initEPollData()
 *
 * @brief This function initializes data structures needed for epoll operation for receiving TCP data,
 * then creates epoll descriptor and starts a thread to receive data from registered socket
 * descriptors using epoll mechanism.
 *
 * @param None
 *
 * @return bool [out] true if function succeeds;
 * 					  false if function fails or in case of errors
 */
bool initEPollData()
{
	//reset client structure after data is read from the socket
	for(int i = 0; i < MAX_DEVICE_PER_SITE; i++)
	{
		resetEPollClientDataStruct(&m_clientAccepted[i]);
	}

	//init epoll
	m_epollFd = epoll_create1(0);
	if (m_epollFd == -1) {
		perror("Failed to create epoll file descriptor :: \n");
		return false;
	}

	thread_Create_t stEpollRecvThreadParam = { 0 };
	stEpollRecvThreadParam.dwStackSize = 0;
	stEpollRecvThreadParam.lpStartAddress = EpollRecvThread;
	stEpollRecvThreadParam.lpThreadId = &EpollRecv_ThreadId;

	EpollRecv_ThreadId = Osal_Thread_Create(&stEpollRecvThreadParam);
	if(-1 == EpollRecv_ThreadId)
	{
		return false;
	}
	EPollMutex = Osal_Mutex();
	if(NULL == EPollMutex)
	{
		return false;
	}

	return true;
} // End of initEPollData

/**
 * @fn void deinitEPollData(void)
 *
 * @brief This function de-initializes data structures related to epoll operation for receiving TCP data.
 * 			It resets all the socket descriptors registered for epoll.
 *
 * @param None
 *
 * @return None
 */
void deinitEPollData(void)
{
	Osal_Thread_Terminate(EpollRecv_ThreadId);
	//reset client structure after data is read from the socket
	for(int i = 0; i < MAX_DEVICE_PER_SITE; i++)
	{
		resetEPollClientDataStruct(&m_clientAccepted[i]);
		m_clientAccepted[i].m_pstConRef = NULL;
	}

	Osal_Close_Mutex(EPollMutex);
} // End of deinitEPollData

/**
 * @fn void removeEPollRefNoLock(int a_iIndex)
 *
 * @brief This function removes a socket descriptor registered with with epoll and
 * resets associated data structures.
 * The calling function should have the lock for epoll data structure.
 *
 * @param a_iIndex [in] int reference to epoll data structure
 *
 * @return None
 */
void removeEPollRefNoLock(int a_iIndex)
{
	if((a_iIndex >= 0) && (a_iIndex < MAX_DEVICE_PER_SITE))
	{
		// Obtain lock
		if(NULL != m_clientAccepted[a_iIndex].m_pstConRef)
		{
			// remove from epoll
			struct epoll_event m_event;
			m_event.events = EPOLLIN;
			m_event.data.fd = m_clientAccepted[a_iIndex].m_pstConRef->m_sockfd;
			if (epoll_ctl(m_epollFd, EPOLL_CTL_DEL, m_clientAccepted[a_iIndex].m_pstConRef->m_sockfd, &m_event))
			{
				perror("Failed to delete file descriptor from epoll:");
			}
			m_clientAccepted[a_iIndex].m_pstConRef = NULL;
		}
		resetEPollClientDataStruct(&m_clientAccepted[a_iIndex]);
		m_clientAccepted[a_iIndex].m_pstConRef = NULL;
	}
} // End of removeEPollRefNoLock

/**
 * @fn void removeEPollRef(int a_iIndex)
 *
 * @brief This function removes a socket descriptor from epoll descriptors and
 * resets associated data structures.
 * This function acquires lock for epoll data structure.
 *
 * @param a_iIndex [in] int Reference to epoll data structure
 *
 * @return None
 */
void removeEPollRef(int a_iIndex)
{
	if((a_iIndex >= 0) && (a_iIndex < MAX_DEVICE_PER_SITE))
	{
		//Osal_Wait_Mutex(EPollMutex);
		// addressed review comment
		if(0 != Osal_Wait_Mutex(EPollMutex))
		{
			// fail to lock mutex
			return;
		}
		removeEPollRefNoLock(a_iIndex);
		// addressed review comment
		if(0 != Osal_Release_Mutex (EPollMutex))
		{
			// fail to unlock mutex
			return;
		}
		//printf("Removing ref %d\n", a_iIndex);
	}
} // End of removeEPollRef

/**
 * @fn int addtoEPollList(IP_Connect_t *a_pstIPConnect)
 *
 * @brief This function sets the socket's failure state. It checks if the referenced socket ID and
 * socket ID from the function argument are same or not. If they are different, it removes the
 * socket descriptor from the epoll and resets the structure for next use. If socket IDs are same
 * and getting used for different operation, close the connection. If there is no socket ID with
 * the reference given, reset the socket descriptor.
 *
 * @param stIPConnect [in] IP_Connect_t* pointer to struct of type IP_Connect_t
 *
 * @return [out] none
 *
 */
int addtoEPollList(IP_Connect_t *a_pstIPConnect)
{
	if(NULL != a_pstIPConnect)
	{
		int ret = -1;
		// Osal_Wait_Mutex(EPollMutex);
		// addressed review comment
		if(0 != Osal_Wait_Mutex(EPollMutex))
		{
			// fail to lock mutex
			return ret;
		}
		int index = -1;
		for(int i = 0; i < MAX_DEVICE_PER_SITE; i++)
		{
			// Check if this connection ref is already in list
			if(m_clientAccepted[i].m_pstConRef == a_pstIPConnect)
			{
				if(m_clientAccepted[i].m_pstConRef->m_sockfd == a_pstIPConnect->m_sockfd)
				{
					// Ref and socket are same. No action
				}
				else
				{
					// Sockets are not same. Remove earlier connection
					removeEPollRefNoLock(i);
				}
				index = i;
				break;
			}
			else if(m_clientAccepted[i].m_pstConRef == NULL)
			{
				// This is to find first empty node in array
				if(-1 == index)
				{
					index = i;
				}
			}
			else
			{
				// Node is not NULL
				if(m_clientAccepted[i].m_pstConRef->m_sockfd == a_pstIPConnect->m_sockfd)
				{
					// Sockets are same but connection references are different.
					// This should ideally not occur.
					printf("Error: Socket for EPOLL-ADD is already used for some other connection");
					removeEPollRefNoLock(i);
					closeConnection(m_clientAccepted[i].m_pstConRef);
					ret = -2;
					break;
				}
			}
		}

		// This fd is not yet added to list
		if (-1 != index)
		{
			resetEPollClientDataStruct(&m_clientAccepted[index]);
			m_clientAccepted[index].m_pstConRef = NULL;
			// add to epoll events
			struct epoll_event m_event;
			m_event.events = EPOLLIN;
			m_event.data.fd = a_pstIPConnect->m_sockfd;
			ret = 0;
			if (epoll_ctl(m_epollFd, EPOLL_CTL_ADD, a_pstIPConnect->m_sockfd, &m_event))
			{
				if(EEXIST != errno)
				{
					perror("Failed to add file descriptor to epoll:");
					ret = -1;
				}
			}
			if(ret == 0)
			{
				// Set 2 way-references
				m_clientAccepted[index].m_pstConRef = a_pstIPConnect;
				a_pstIPConnect->m_iRcvConRef = index;
				ret = index;
				//printf("adding ref: %d", ret);
			}
		}
		if(0 != Osal_Release_Mutex (EPollMutex))
		{
			// fail to unlock mutex
			return -1;
		}
		return ret;
	}
	return -1;
} // End of addtoEPollList

/**
 *
 * @fn struct timespec getEpochTime()
 *
 * @brief This function gets the current epoch time
 *
 * @param none
 *
 * @return struct timespec [out] epoch time struct
 */
struct timespec getEpochTime()
{

	struct timespec ts;
    timespec_get(&ts, TIME_UTC);

    return ts;
} // End of getEpochTime

/**
 *
 * @fn void* EpollRecvThread()
 *
 * @brief This function is a thread routine to start polling on sockets registered with epoll to receive data.
 * This function sets the thread priority as per the configuration. It then allocates the number of polling events
 * the can occur and thus initializes epoll descriptors event structure.
 * Function then waits for events to occur on registered socket descriptors. If any socket descriptor notifies
 * of incoming response, epoll notifies it to the function. Function then iterates through all the socket descriptors
 * and reads the response data. In order to read the data, the function first reads only 6 bytes from the response
 * and parses the length of the response. Then calculates the number of bytes to be read from that socket descriptor.
 * It keeps on reading till a complete response is not received or if it has read 0 bytes, it goes to read from the
 * next socket descriptor.
 * Once the complete response is received, function adds it to the request manager's queue to process further
 * and resets the socket descriptor to be used for next request.
 *
 * @param  none
 * @return [out] none
 *
 */
void* EpollRecvThread(void)
{
	int event_count = 0;
	size_t bytes_read = 0;
	size_t bytes_to_read = 0;

	// set thread priority
	set_thread_sched_param();

	// allocate no of polling events
	m_events = (struct epoll_event*) calloc(MAXEVENTS, sizeof(struct epoll_event));

	if(NULL == m_events)
	{
		return NULL;
	}

	while (true != g_bThreadExit)
	{
		event_count = 0;

		event_count = epoll_wait(m_epollFd, m_events, MAXEVENTS, EPOLL_TIMEOUT);

		// addressed review comment
		if(0 != Osal_Wait_Mutex(EPollMutex))
		{
			// fail to lock mutex
			continue;
		}

		for (int i = 0; i < event_count; i++)
		{
			int clientID = getClientIdFromList(m_events[i].data.fd);

			if (clientID == -1)
			{
				continue;
			}
			if(NULL == m_clientAccepted[clientID].m_pstConRef)
			{
				continue;
			}

			//if there are still bytes remaining to be read for this socket, adjust read size accordingly
			if (m_clientAccepted[clientID].m_bytesToBeRead > 0)
			{
				bytes_to_read = m_clientAccepted[clientID].m_bytesToBeRead;
			}
			else
			{
				bytes_to_read = MODBUS_HEADER_LENGTH; //read only header till length
			}

			//receive data from socket
			bytes_read = recv(m_clientAccepted[clientID].m_pstConRef->m_sockfd,
					m_clientAccepted[clientID].m_readBuffer+m_clientAccepted[clientID].m_bytesRead, bytes_to_read, 0);


			if (bytes_read == 0)
			{
				// No data received, check for new socket
				continue;
			}

			//parse length if fd has 0 bytes read, otherwise rest packets are with actual data and not header
			if (m_clientAccepted[clientID].m_bytesRead == 0)
			{
				m_clientAccepted[clientID].m_len = (m_clientAccepted[clientID].m_readBuffer[4] << 8
						| m_clientAccepted[clientID].m_readBuffer[5]);
			}

			if (bytes_to_read == bytes_read)
			{
				m_clientAccepted[clientID].m_bytesRead += bytes_read;

				m_clientAccepted[clientID].m_bytesToBeRead =
						(m_clientAccepted[clientID].m_len + MODBUS_HEADER_LENGTH)
								- m_clientAccepted[clientID].m_bytesRead;
			}

			//add one more recv to read response completely
			if (m_clientAccepted[clientID].m_bytesToBeRead > 0)
			{
				while (m_clientAccepted[clientID].m_bytesToBeRead != 0)
					{
					bytes_to_read = m_clientAccepted[clientID].m_bytesToBeRead;

					//receive data from socket
					bytes_read = recv(m_clientAccepted[clientID].m_pstConRef->m_sockfd,
								m_clientAccepted[clientID].m_readBuffer+m_clientAccepted[clientID].m_bytesRead,
								bytes_to_read, 0);

					if(bytes_read < 0)
					{
						//recv failed with error
						perror("Recv() failed : ");
						//Close the connection and mark socket invalid
						removeEPollRefNoLock(clientID);
						closeConnection(m_clientAccepted[clientID].m_pstConRef);

						break;
					}
					else
					{
						if (bytes_read == 0)
						{
							// there is no data read. Let's check new thing
							break;
						}

						m_clientAccepted[clientID].m_bytesRead += bytes_read;

						m_clientAccepted[clientID].m_bytesToBeRead =
								(m_clientAccepted[clientID].m_len + MODBUS_HEADER_LENGTH)
										- m_clientAccepted[clientID].m_bytesRead;
					}
				}

				//should have read response completely
				if ((m_clientAccepted[clientID].m_len + MODBUS_HEADER_LENGTH)
						== m_clientAccepted[clientID].m_bytesRead)
				{
					addToHandleRespQ(&m_clientAccepted[clientID]);

					//clear socket struct for next iteration
					resetEPollClientDataStruct(&m_clientAccepted[clientID]);

				}
				else
				{
					//this case should never happen ideally
					//continue to read for the same socket & epolling
					continue;
				}

			}
		} //for loop for sockets ends

		if(0 != Osal_Release_Mutex (EPollMutex))
		{
			// fail to unlock mutex
			continue;
		}
	} //while ends

	//close client socket or push all the client sockets in a vector and close all
	close(m_epollFd);

	if(m_events != NULL)
		free(m_events);

	return NULL;
} // End of EpollRecvThread

#else

/**
 *
 *@fn void* SessionControlThread(void* threadArg)
 *
 * @brief This function is a thread routine to send Modbus RTU messages to Modbus slave device
 * which are available in the Linux message queue. Thread continuously checks for a new message in
 * Linux message queue; once the message is available it sends the request to the Modbus slave device.
 * The Modbus_SendPacket function receives the response from Modbus slave device, after this session
 * control thread function invokes the ModbusApp callback function. After invoking ModbusApp callback
 * function, this function frees up with request node from request manager's list.
 *
 * @param threadArg [in] void* thread argument
 *
 * @return [out] none
 *
 */
void* SessionControlThread(void* threadArg)
{
	int32_t i32MsgQueIdSC = 0;
	Linux_Msg_t stScMsgQue = { 0 };
	stMbusPacketVariables_t *pstMBusReqPact = NULL;
	stRTUConnectionData_t stRTUConnectionData = {};
	uint8_t u8ReturnType = 0;
	stLiveSerSessionList_t pstLivSerSesslist;

	pstLivSerSesslist = *((stLiveSerSessionList_t *)threadArg);
	i32MsgQueIdSC = pstLivSerSesslist.MsgQId;

	stRTUConnectionData.m_fd = -1;
	while(false == g_bThreadExit)
	{
		memset(&stScMsgQue,00,sizeof(stScMsgQue));
		if(OSAL_Get_Message(&stScMsgQue, i32MsgQueIdSC))
		{
			pstMBusReqPact = stScMsgQue.lParam;
			// Check if connection is established
			if(stRTUConnectionData.m_fd == -1)
			{
				int ret = initSerialPort(&stRTUConnectionData,
								pstLivSerSesslist.m_portName,
								pstLivSerSesslist.m_baudrate,
								pstLivSerSesslist.m_parity);
				if(-1 == ret)
				{
					stRTUConnectionData.m_fd = -1;
					pstMBusReqPact->m_u8ProcessReturn = STACK_ERROR_SERIAL_PORT_ERROR;
					printf("Failed to initialize serial port for RTU. File descriptor is set to :: %d\n",stRTUConnectionData.m_fd);
					addToRespQ(pstMBusReqPact);
					continue;
				}
			}
			u8ReturnType = Modbus_SendPacket(pstMBusReqPact, stRTUConnectionData,
					pstLivSerSesslist.m_lInterframeDelay, pstLivSerSesslist.m_lrespTimeout);

			pstMBusReqPact->m_u8ProcessReturn = u8ReturnType;
			if(STACK_NO_ERROR == u8ReturnType)
			{
				//pstMBusReqPact->m_state = RESP_RCVD_FROM_NETWORK;
			}
			else
			{
				addToRespQ(pstMBusReqPact);
			}
		}
		fflush(stdin);

		// check for thread exit
		if(g_bThreadExit)
		{
			break;
		}
	}
	return NULL;
}  // End of SessionControlThread
#endif


// when Modbus communication mode is TCP
#ifdef MODBUS_STACK_TCPIP_ENABLED
/**
 *
 *@fn unsigned long get_nanos(void)
 *
 * @brief This function gets current time stamp in nano-seconds.
 *
 * @param none
 *
 * @return [out] unsigned long time in nano-seconds
 */
unsigned long get_nanos(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (unsigned long)ts.tv_sec * 1000000000L + ts.tv_nsec;
} // End of get_nanos

/**
 *
 * @fn void releaseFromTrackerNode(stMbusPacketVariables_t *a_pstNodeToRemove,
	struct stTimeOutTrackerNode *a_pstTracker)
 *
 * @brief This function removes request from timeout tracker node.
 * Assumed is calling function has acquired lock.
 *
 * @param pstMBusRequesPacket [in] stMbusPacketVariables_t* pointer to structure holding request
 * 								   sent on Modbus slave device
 *
 * @param a_pstTracker [in] stTimeOutTrackerNode* reference to timeout tracker list from which
 * 							node needs to be removed
 *
 * @return [out] none
 */
void releaseFromTrackerNode(stMbusPacketVariables_t *a_pstNodeToRemove,
		struct stTimeOutTrackerNode *a_pstTracker)
{
	if((NULL == a_pstNodeToRemove) || (NULL == a_pstTracker))
	{
		return;
	}

	{
		stMbusPacketVariables_t *pstPrev = a_pstNodeToRemove->__prev;
		stMbusPacketVariables_t *pstNext = a_pstNodeToRemove->__next;

		if(NULL != pstPrev)
		{
			pstPrev->__next = pstNext;
		}
		if(NULL != pstNext)
		{
			pstNext->__prev = pstPrev;
		}
		if(a_pstNodeToRemove == a_pstTracker->m_pstStart)
		{
			a_pstTracker->m_pstStart = pstNext;
		}
		if(a_pstNodeToRemove == a_pstTracker->m_pstLast)
		{
			a_pstTracker->m_pstLast = pstPrev;
		}

		a_pstNodeToRemove->__prev = NULL;
		a_pstNodeToRemove->__next = NULL;
	}
} // End of releaseFromTrackerNode


/**
 *
 * @fn void releaseFromTracker(stMbusPacketVariables_t *pstMBusRequesPacket)
 *
 * @brief This function removes the request from timeout tracker.
 * Identifies tracker node from the tracker list and then removes the node from that tracker
 *
 * @param pstMBusRequesPacket [in] stMbusPacketVariables_t* pointer to structure holding information
 * 									about the request
 *
 * @return void [out] none
 */
void releaseFromTracker(stMbusPacketVariables_t *pstMBusRequesPacket)
{
	if(NULL == pstMBusRequesPacket)
	{
		return;
	}
	// Timeout index is out of bounds
	if((pstMBusRequesPacket->m_iTimeOutIndex < 0) ||
			(pstMBusRequesPacket->m_iTimeOutIndex > g_oTimeOutTracker.m_iSize))
	{
		return;
	}
	// tract timeout requests
	struct stTimeOutTrackerNode *pstTemp = &g_oTimeOutTracker.m_pstArray[pstMBusRequesPacket->m_iTimeOutIndex];
	if(NULL == pstTemp)
	{
		printf("Error: Null timeout tracker node\n");
		return;
	}

	// Obtain the lock
	int expected = 0;
	do
	{
		expected = 0;
	} while(!atomic_compare_exchange_weak(&(pstTemp->m_iIsLocked), &expected, 1));

	// Lock is obtained

	releaseFromTrackerNode(pstMBusRequesPacket, pstTemp);
	pstMBusRequesPacket->m_iTimeOutIndex = -1;
	// Done. Release the lock
	pstTemp->m_iIsLocked = 0;

} // End of releaseFromTracker

#endif

/**
 *
 * @fn void addToRespQ(stMbusPacketVariables_t *a_pstReq)
 *
 * @brief This function adds Modbus request (sent to Modbus slave device) in
 * response queue
 *
 * @param a_pstReq [in] stMbusPacketVariables_t* pointer to structure holding information
 * 						about request to add in response queue
 *
 * @return [out] none
 */
void addToRespQ(stMbusPacketVariables_t *a_pstReq)
{
	if(NULL != a_pstReq)
	{
		Post_Thread_Msg_t stPostThreadMsg = { 0 };
		// Add to queue
		stPostThreadMsg.idThread = g_stRespProcess.m_i32RespMsgQueId;
		stPostThreadMsg.wParam = NULL;
		stPostThreadMsg.lParam = a_pstReq;
		stPostThreadMsg.MsgType = a_pstReq->m_lPriority;

		if(!OSAL_Post_Message(&stPostThreadMsg))
		{
			//OSAL_Free(a_pstReq);
			freeReqNode(a_pstReq);
		}
		else
		{
			// Signal response timeout thread
			sem_post(&g_stRespProcess.m_semaphoreResp);
		}
	}
} // End of addToRespQ

/**
 *
 * @fn void* postResponseToApp(void* threadArg)
 *
 * @brief This function is thread routine which posts response to application.
 * It listens on a Linux message queue to receive response of requests to post to ModbusApp.
 * Once the response is sent to ModbusApp, function frees the response node so that other
 * requests can reuse it. The thread keeps working till the flag is not set to terminate
 * the thread.
 *
 * @param threadArg [in] void* pointer
 *
 * @return [out] none
 */
void* postResponseToApp(void* threadArg)
{
	Linux_Msg_t stScMsgQue = { 0 };
	stMbusPacketVariables_t *pstMBusRequesPacket = NULL;
	int32_t i32RetVal = 0;

	// set thread priority
	set_thread_sched_param();

	while(false == g_bThreadExit)
	{
		if((sem_wait(&g_stRespProcess.m_semaphoreResp)) == -1 && errno == EINTR)
		{
			// Continue if interrupted by handler
			continue;
		}
		memset(&stScMsgQue,00,sizeof(stScMsgQue));
		i32RetVal = 0;
		i32RetVal = OSAL_Get_NonBlocking_Message(&stScMsgQue, g_stRespProcess.m_i32RespMsgQueId);
		if(i32RetVal > 0)
		{
			pstMBusRequesPacket = stScMsgQue.lParam;
			if(NULL != pstMBusRequesPacket)
			{
				if(pstMBusRequesPacket->m_state == RESP_RCVD_FROM_NETWORK)
				{
					pstMBusRequesPacket->m_u8ProcessReturn = DecodeRxPacket(pstMBusRequesPacket->m_u8RawResp, pstMBusRequesPacket);
				}
				ApplicationCallBackHandler(pstMBusRequesPacket, pstMBusRequesPacket->m_u8ProcessReturn);
				freeReqNode(pstMBusRequesPacket);
			}
		}
		// check for thread exit
		if(g_bThreadExit)
		{
			break;
		}
	}

	return NULL;
} // End of postResponseToApp

#ifdef MODBUS_STACK_TCPIP_ENABLED
/**
 *
 * @fn int getTimeoutTrackerCount()
 *
 * @brief This function returns current timeout counter.
 * Timeout is implemented in the form of counter.
 *
 * @param none
 *
 * @return [out] int the timeout counter
 */
int getTimeoutTrackerCount()
{
	return g_oTimeOutTracker.m_iCounter;
} // End of getTimeoutTrackerCount

/**
 *
 * @fn void* timeoutTimerThread(void* threadArg)
 *
 * @brief This function is a thread routine which implements a timer to measure timeout for
 * initiated requests. If the timeout occurs for a request, it notifies the processing
 * thread about the same.
 *
 * @param [in] void* thread argument
 *
 * @return none
 */
void* timeoutTimerThread(void* threadArg)
{
	// set thread priority
	set_thread_sched_param();
	g_oTimeOutTracker.m_iCounter = 0;

	struct timespec ts;
	int rc = clock_getres(CLOCK_MONOTONIC, &ts);
	if(0 != rc)
	{
		perror("Unable to get clock resolution: ");
	}
	else
	{
		printf("Monotonic Clock resolution: %10ld.%09ld\n", (long) ts.tv_sec, ts.tv_nsec);
	}

	rc = clock_gettime(CLOCK_MONOTONIC, &ts);
	if(0 != rc)
	{
		perror("Stack fatal error: Response timeout: clock_gettime failed: ");
		return NULL;
	}
	while(false == g_bThreadExit)
	{
		unsigned long next_tick = (ts.tv_sec * 1000000000L + ts.tv_nsec) + 1*1000*1000;
		ts.tv_sec = next_tick / 1000000000L;
		ts.tv_nsec = next_tick % 1000000000L;
		do
		{
			rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL);
		} while(EINTR == rc);

		if(0 == rc)
		{
			g_oTimeOutTracker.m_iCounter = (g_oTimeOutTracker.m_iCounter + 1) % g_oTimeOutTracker.m_iSize;

			struct stIntDataForQ objSt;
			objSt.m_iID = g_oTimeOutTracker.m_iCounter;
			objSt.m_lType = 1;

			if(-1 != msgsnd(g_oTimeOutTracker.m_iTimeoutActionQ, &objSt, sizeof(objSt) - sizeof(long), 0))
			{
				sem_post(&g_oTimeOutTracker.m_semTimeout);
			}
			else
			{
				perror("Unable to post timeout timer action:");
			}
		}
		else
		{
			printf("Stack: Error in Response timeout function: %d\n", rc);
		}
	}

	return NULL;
} // End of timeoutTimerThread

/**
 *
 * @fn void* timeoutActionThread(void* threadArg)
 *
 * @brief The function is a thread routine which identifies timed out requests and
 * initiates a response accordingly.
 *
 * @param [in] void* thread argument
 *
 * @return none
 */
void* timeoutActionThread(void* threadArg)
{
	printf("in timeoutActionThread thread\n");

	// set thread priority
	set_thread_sched_param();

	const int iArrayToTimeoutDiff = g_oTimeOutTracker.m_iSize - g_stModbusDevConfig.m_lResponseTimeout/1000;
	const size_t MsgSize = sizeof(struct stIntDataForQ) - sizeof(long);

	while(false == g_bThreadExit)
	{
		if((sem_wait(&g_oTimeOutTracker.m_semTimeout)) == -1 && errno == EINTR)
		{
			// Continue if interrupted by handler
			continue;
		}
		struct stIntDataForQ objSt = {0};
		int i32RetVal = msgrcv(g_oTimeOutTracker.m_iTimeoutActionQ, &objSt, MsgSize, 0, MSG_NOERROR | IPC_NOWAIT);
		if(i32RetVal <= 0)
		{
			//i32RetVal = -1;
			perror("Unable to get data from timeout q");
			continue;
		}

		// get list corresponding to the index
		// This is the current index
		// Find timed out index using following formula
		// timedOutIndex = [current index + (timeout-array-size - timeout)] % timeout-array-size
		int iTimedOutIndex = (objSt.m_iID + iArrayToTimeoutDiff) % g_oTimeOutTracker.m_iSize;

		// identify index into q
		if(iTimedOutIndex > g_oTimeOutTracker.m_iSize)
		{
			printf("Error: Timed out Index is greater than array size\n");
			continue;
		}
		int expected = 0;
		struct stTimeOutTrackerNode *pstTemp = &g_oTimeOutTracker.m_pstArray[iTimedOutIndex];
		if(NULL == pstTemp)
		{
			printf("Error: Null timeout tracker node\n");
			continue;
		}

		// Obtain the lock
		do
		{
			expected = 0;
		} while(!atomic_compare_exchange_weak(&(pstTemp->m_iIsLocked), &expected, 1));

		// Lock is obtained
		stMbusPacketVariables_t *pstNextNode = pstTemp->m_pstStart;
		while(NULL != pstNextNode)
		{
			stMbusPacketVariables_t *pstCur = pstNextNode;
			pstNextNode = pstNextNode->__next;
			eTransactionState expected = REQ_SENT_ON_NETWORK;
			// Change the state of request node
			if(true ==
					atomic_compare_exchange_strong(&pstCur->m_state, &expected, RESP_TIMEDOUT))
			{
				pstCur->m_u8ProcessReturn = STACK_ERROR_RECV_TIMEOUT;
				pstCur->m_stMbusRxData.m_u8Length = 0;
				// Init resp received timestamp
				timespec_get(&(pstCur->m_objTimeStamps.tsRespRcvd), TIME_UTC);
				addToRespQ(pstCur);
			}
		}
		// Done. Release the lock
		pstTemp->m_iIsLocked = 0;
	}
	return NULL;
} // End of timeoutActionThread

/**
 *
 * @fn int initTimeoutTrackerArray(void)
 *
 * @brief This function initialize timeout tracker data structure and threads.
 *
 * @param none
 *
 * @return [out] int 0 if function succeeds in the initialization of timeout tracker;
 * 					-1 if function fails
 */
int initTimeoutTrackerArray(void)
{
	g_oTimeOutTracker.m_iTimeoutActionQ = OSAL_Init_Message_Queue();
	if(-1 == g_oTimeOutTracker.m_iTimeoutActionQ)
	{
		return -1;
	}

	if(-1 == sem_init(&g_oTimeOutTracker.m_semTimeout, 0, 0 /* Initial value of zero*/))
	{
		perror("Timeout tracker: semaphore creation error: ");
		return -1;
	}

	// determine size of timeout tracker array
	g_oTimeOutTracker.m_iSize = g_stModbusDevConfig.m_lResponseTimeout/1000 + ADDITIONAL_RECORDS_TIMEOUT_TRACKER;
	if(g_oTimeOutTracker.m_iSize % REQ_ARRAY_MULTIPLIER)
	{
		g_oTimeOutTracker.m_iSize =
				g_oTimeOutTracker.m_iSize + (REQ_ARRAY_MULTIPLIER - g_oTimeOutTracker.m_iSize % REQ_ARRAY_MULTIPLIER);
	}
	printf("Timeout tracker array size: %d\n", g_oTimeOutTracker.m_iSize);

	if(g_oTimeOutTracker.m_iSize > 0)
	{
		// allocate memory
		g_oTimeOutTracker.m_pstArray =
			(struct stTimeOutTrackerNode*)calloc(g_oTimeOutTracker.m_iSize, sizeof(struct stTimeOutTrackerNode));
	}

	if(NULL == g_oTimeOutTracker.m_pstArray)
	{
		printf("Unable to allocate memory for timeout tracker of size: %d\n", g_oTimeOutTracker.m_iSize);
		return -1;
	}

	// Resets the timeout tracker lists
	int iCount = 0;
	for(; iCount < g_oTimeOutTracker.m_iSize; ++iCount)
	{
		g_oTimeOutTracker.m_pstArray[iCount].m_pstStart = NULL;
		g_oTimeOutTracker.m_pstArray[iCount].m_pstLast = NULL;
		g_oTimeOutTracker.m_pstArray[iCount].m_iIsLocked = 0;
	}

	// Initiate timeout timer thread. This thread measures timeout tracker counter
	{
		thread_Create_t stThreadParam = { 0 };
		stThreadParam.dwStackSize = 0;
		stThreadParam.lpStartAddress = timeoutTimerThread;
		stThreadParam.lpParameter = NULL;
		stThreadParam.lpThreadId = &g_oTimeOutTracker.m_threadIdTimeoutTimer;

		g_oTimeOutTracker.m_threadIdTimeoutTimer = Osal_Thread_Create(&stThreadParam);

		if(-1 == g_oTimeOutTracker.m_threadIdTimeoutTimer)
		{
			return -1;
		}
	}

	// Initiate timeout action thread
	{
		thread_Create_t stThreadParam = { 0 };
		stThreadParam.dwStackSize = 0;
		stThreadParam.lpStartAddress = timeoutActionThread;
		stThreadParam.lpParameter = NULL;
		stThreadParam.lpThreadId = &g_oTimeOutTracker.m_threadTimeoutAction;

		// create thread
		g_oTimeOutTracker.m_threadTimeoutAction = Osal_Thread_Create(&stThreadParam);

		if(-1 == g_oTimeOutTracker.m_threadTimeoutAction)
		{
			return -1;
		}
	}
	printf("Timeout tracker is configured\n");
	return 0;
} // End of initTimeoutTrackerArray
#endif
/**
 *
 * @fn int initRespStructs(void)
 *
 * @brief This function initializes the request list data and data structures
 * needed for epoll mechanism.
 *
 * @param none
 *
 * @return [out] int  0 if function succeeds in initialization;
 * 					  -1 if function fails to initialize
 */
int initRespStructs(void)
{
	g_stRespProcess.m_i32RespMsgQueId = OSAL_Init_Message_Queue();

	if(-1 == g_stRespProcess.m_i32RespMsgQueId)
	{
		 return -1;
	}
	// Initialize semaphore for response process
	if(-1 == sem_init(&g_stRespProcess.m_semaphoreResp, 0, 0 /* Initial value of zero*/))
	{
	   //printf("initTCPRespStructs::Could not create unnamed semaphore\n");
	   return -1;
	}
#ifdef MODBUS_STACK_TCPIP_ENABLED
	if(0 > initTimeoutTrackerArray())
	{
		printf("Timeout tracker array init failed\n");
		return -1;
	}
#endif

	// Initiate response thread
	{
		thread_Create_t stThreadParam1 = { 0 };
		stThreadParam1.dwStackSize = 0;
		stThreadParam1.lpStartAddress = postResponseToApp;
		stThreadParam1.lpParameter = &g_stRespProcess.m_i32RespMsgQueId;
		stThreadParam1.lpThreadId = &g_stRespProcess.m_threadIdRespToApp;

		// osal create thread
		g_stRespProcess.m_threadIdRespToApp = Osal_Thread_Create(&stThreadParam1);

		if(-1 == g_stRespProcess.m_threadIdRespToApp)
		{
			// error
			return -1;
		}
	}

#ifdef MODBUS_STACK_TCPIP_ENABLED
	initEPollData();
#endif
	return 0;
} // End of initRespStructs

/**
 *
 * @fn void deinitRespStructs(void)
 *
 * @brief This function de-initializes data structures related to response processing.
 *
 * @param none
 *
 * @return Nothing
 */
void deinitRespStructs(void)
{
	// 3 steps:
	// Deinit response timeout mechanism
	// Deinit epoll mechanism
	// Deinit thread which posts responses to app
#ifdef MODBUS_STACK_TCPIP_ENABLED
	deinitTimeoutTrackerArray();
	deinitEPollData();
#endif

	// De-Initiate response thread
	{
		// terminate the thread
		Osal_Thread_Terminate(g_stRespProcess.m_threadIdRespToApp);
		// deallocate memory allocated for semaphore
		sem_destroy(&g_stRespProcess.m_semaphoreResp);
		if(g_stRespProcess.m_i32RespMsgQueId)
		{
			// deallocate memory allocated for message queue
			OSAL_Delete_Message_Queue(g_stRespProcess.m_i32RespMsgQueId);
		}
	}
} // End of deinitRespStructs

#ifdef MODBUS_STACK_TCPIP_ENABLED
/**
 * @fn void deinitTimeoutTrackerArray(void)
 *
 * @brief This function de-initializes the timeout tracker data structure and threads.
 *
 * @param none
 *
 * @return none
 */
void deinitTimeoutTrackerArray(void)
{
	// terminate the thread created for timeout tracker
	Osal_Thread_Terminate(g_oTimeOutTracker.m_threadIdTimeoutTimer);
	Osal_Thread_Terminate(g_oTimeOutTracker.m_threadTimeoutAction);
	// deallocate the memory
	sem_destroy(&g_oTimeOutTracker.m_semTimeout);
	if(g_oTimeOutTracker.m_iTimeoutActionQ)
	{
		// deallocate memory allocated for message queue created for timeout tracker
		OSAL_Delete_Message_Queue(g_stRespProcess.m_i32RespMsgQueId);
	}

	if(NULL != g_oTimeOutTracker.m_pstArray)
	{
		//free the memory allocated for pointer to array
		free(g_oTimeOutTracker.m_pstArray);
	}
	g_oTimeOutTracker.m_iSize = 0;
} // End of deinitTimeoutTrackerArray

/**
 *
 * @fn int addReqToList(stMbusPacketVariables_t *pstMBusRequesPacket)
 *
 * @brief This function adds request to list for tracking timeout.
 *
 * @param pstMBusRequesPacket [in] stMbusPacketVariables_t* pointer to structure holding
 * 									 information about the request sent on Modbus slave device
 *
 * @return [out] int 0 if function succeeds;
 *  				 -1 if function fails
 */
int addReqToList(stMbusPacketVariables_t *pstMBusRequesPacket)
{
	//check NULL pointer
	if(NULL == pstMBusRequesPacket)
	{
		return -1;
	}
	pstMBusRequesPacket->__next = NULL;
	// get the timeout tracker counter
	int iTimeoutTracker = getTimeoutTrackerCount();

	if((0 > iTimeoutTracker)
		|| (iTimeoutTracker > g_oTimeOutTracker.m_iSize))
	{
		printf("Error:addReqToList: %d index is greater than size %d",
				iTimeoutTracker,
				g_oTimeOutTracker.m_iSize);

		return -1;
	}
    // structure to pointer which holds the modbus slave request data
	struct stTimeOutTrackerNode *pstTemp = &g_oTimeOutTracker.m_pstArray[iTimeoutTracker];
	if(NULL == pstTemp)
	{
		printf("Error: Null timeout tracker node\n");
		return -1;
	}

	// Obtain the lock
	int expected = 0;
	do
	{
		expected = 0;
	} while(!atomic_compare_exchange_weak(&(pstTemp->m_iIsLocked), &expected, 1));

	// Lock is obtained
	pstMBusRequesPacket->__prev = pstTemp->m_pstLast;
	if(NULL == pstTemp->m_pstStart)
	{
		pstTemp->m_pstStart = pstMBusRequesPacket;
		pstTemp->m_pstLast = pstMBusRequesPacket;
	}
	else
	{
		pstTemp->m_pstLast->__next = pstMBusRequesPacket;
		pstTemp->m_pstLast = pstMBusRequesPacket;
	}
	pstMBusRequesPacket->m_iTimeOutIndex = iTimeoutTracker;
	// Done. Release the lock
	pstTemp->m_iIsLocked = 0;

	return 0;
} // End of ServerSessTcpAndCbThread

/**
 *@fn stMbusPacketVariables_t* markRespRcvd(uint8_t a_u8UnitID, uint16_t a_u16TransactionID)
 *
 * @brief This function searches a request with specific unit id (Modbus slave device id) and
 * transaction id (request id that was sent on Modbus slave device) in request list and
 * mark as response is received.
 *
 * @param a_u8UnitID 				[in] uint8_t Modbus slave device ID
 * @param a_u16TransactionID  		[in] uint16_t request id that was sent on Modbus slave device
 * @return stMbusPacketVariables_t 	[out] stMbusPacketVariables_t* pointer to structure holding
 * 										  information
 */
stMbusPacketVariables_t* markRespRcvd(uint8_t a_u8UnitID, uint16_t a_u16TransactionID)
{
	//Initialize to NULL;
	stMbusPacketVariables_t *pstTemp = NULL;
	// validate request id against transaction id
	if(a_u16TransactionID < MAX_REQUESTS)
	{
		eTransactionState expected = REQ_SENT_ON_NETWORK;
		// create a pointer to structure it holds the request for transaction id
		stMbusPacketVariables_t *pstTemp1 = &g_objReqManager.m_objReqArray[a_u16TransactionID];
		if(a_u8UnitID == pstTemp1->m_u8UnitID && a_u16TransactionID == pstTemp1->m_u16TransactionID)
		{

			if(true ==
					atomic_compare_exchange_strong(&pstTemp1->m_state, &expected, RESP_RCVD_FROM_NETWORK))
			{
				pstTemp = pstTemp1;//&g_objReqManager.m_objReqArray[a_u16TransactionID];
			}
		}
	}

	return pstTemp;
} // End of markRespRcvd

/**
 * @fn void* ServerSessTcpAndCbThread(void* threadArg)
 *
 * @brief This function is a thread routine for TCP and callback functions. This function
 * sets the thread priority, receives a request from message queue and then sends it
 * to the Modbus slave device. After the request is sent a response is received in global
 * message queue, function adds it in the response queue for further processing.
 * The thread continues till the flag to terminate the thread is not set.
 *
 * @param threadArg [in] void* thread argument
 *
 * @return [out] void pointer
 *
 */
void* ServerSessTcpAndCbThread(void* threadArg)
{

	Linux_Msg_t stScMsgQue = { 0 };
	uint8_t u8ReturnType = 0;
	stMbusPacketVariables_t *pstMBusRequesPacket = NULL;
	int32_t i32MsgQueIdSSTC = 0;
	int32_t i32RetVal = 0;
	IP_Connect_t stIPConnect;
	IP_address_t stTempIpAdd = {0};

	stLiveSerSessionList_t pstLivSerSesslist;

	pstLivSerSesslist = *((stLiveSerSessionList_t *)threadArg);
	i32MsgQueIdSSTC = pstLivSerSesslist.MsgQId;

	stIPConnect.m_bIsAddedToEPoll = false;
	stIPConnect.m_sockfd = 0;
	stIPConnect.m_retryCount = 0;
	stIPConnect.m_lastConnectStatus = SOCK_NOT_CONNECTED;

	memset(&stIPConnect.m_servAddr, '0', sizeof(stIPConnect.m_servAddr));
	// copy IP address
	stTempIpAdd.s_un.s_un_b.IP_1 = pstLivSerSesslist.m_u8IpAddr[0];
	stTempIpAdd.s_un.s_un_b.IP_2 = pstLivSerSesslist.m_u8IpAddr[1];
	stTempIpAdd.s_un.s_un_b.IP_3 = pstLivSerSesslist.m_u8IpAddr[2];
	stTempIpAdd.s_un.s_un_b.IP_4 = pstLivSerSesslist.m_u8IpAddr[3];
	stIPConnect.m_servAddr.sin_addr.s_addr = stTempIpAdd.s_un.s_addr;
	stIPConnect.m_servAddr.sin_port = htons(pstLivSerSesslist.m_u16Port);
	stIPConnect.m_servAddr.sin_family = AF_INET;
    // This field gives index into an array of established connections/sessions.
	// Keeping default value as -1 to indicate that connection is not yet established.
	stIPConnect.m_iRcvConRef = -1;

	// set thread priority
	set_thread_sched_param();

	while(false == g_bThreadExit)
	{
		memset(&stScMsgQue,00,sizeof(stScMsgQue));
		i32RetVal = 0;
		// get the message from message queue
		i32RetVal = OSAL_Get_Message(&stScMsgQue, i32MsgQueIdSSTC);

		if(i32RetVal > 0)
		{
			pstMBusRequesPacket = stScMsgQue.lParam;

			if(NULL != pstMBusRequesPacket)
			{
				//send the valid modbus packet to slave device
				u8ReturnType = Modbus_SendPacket(pstMBusRequesPacket, &stIPConnect);
				pstMBusRequesPacket->m_u8ProcessReturn = u8ReturnType;
				if(STACK_NO_ERROR == u8ReturnType)
				{
					pstMBusRequesPacket->m_state = REQ_SENT_ON_NETWORK;
				}
				else
				{
					pstMBusRequesPacket->m_state = REQ_PROCESS_ERROR;
					// add to error response to queue
					addToRespQ(pstMBusRequesPacket);
				}
			}
		}

		// check for thread exit
		if(g_bThreadExit)
		{
			break;
		}
	}
	// Close the socket descriptor
	if(stIPConnect.m_sockfd)
	{
		close(stIPConnect.m_sockfd);
	}
	return NULL;
}  // End of ServerSessTcpAndCbThread

/**
 * @fn void addToHandleRespQ(stTcpRecvData_t *a_pstReq)
 *
 * @brief This function adds a response to handle in response queue.
 *
 * @param a_pstReq [in] stTcpRecvData_t* response data received from Modbus slave device
 *
 * @return [out] none
 */
void addToHandleRespQ(stTcpRecvData_t *a_pstReq)
{
	if(NULL != a_pstReq)
	{
		// TCP IP message format
		// 2 bytes = TxID, 2 bytes = Protocol ID, 2 bytes = length, 1 byte = unit id
		// Holds the unit id
		uint8_t  u8UnitID = a_pstReq->m_readBuffer[6];

		// Get TxID
		uByteOrder_t ustByteOrder = {0};
		ustByteOrder.u16Word = 0;
		ustByteOrder.TwoByte.u8ByteTwo = a_pstReq->m_readBuffer[0];
		ustByteOrder.TwoByte.u8ByteOne = a_pstReq->m_readBuffer[1];

		//create pointer to structure to hold the response recieved
		stMbusPacketVariables_t *pstMBusRequesPacket = markRespRcvd(u8UnitID, ustByteOrder.u16Word);
		if(NULL != pstMBusRequesPacket)
		{
			// copy the packet received from read buffer
			memcpy_s(pstMBusRequesPacket->m_u8RawResp, sizeof(pstMBusRequesPacket->m_u8RawResp),
							a_pstReq->m_readBuffer,sizeof(a_pstReq->m_readBuffer));
			// Initialize response received timestamp
			timespec_get(&(pstMBusRequesPacket->m_objTimeStamps.tsRespRcvd), TIME_UTC);
			// Add to response queue for further processing
			addToRespQ(pstMBusRequesPacket);
		}
		else
		{
			//printf("addToHandleRespQ: not found: %d %d\n", u8UnitID, u16TransactionID);
		}
	}

	return;
} // End of addToHandleRespQ
#endif

/**
 *
 * @fn void set_thread_sched_param()
 *
 * @brief This function sets thread parameters. All threads call this function.
 * At present, all stack threads are configured with same parameters.
 *
 * @param none
 *
 * @return [out] none
 */
void set_thread_sched_param(void)
{
	// variable to set priority
	int iThreadPriority = 0;
	eThreadScheduler threadPolicy;
	int result;

	iThreadPriority = THREAD_PRIORITY;
	threadPolicy = THREAD_SCHEDULER;

	//set priority
	struct sched_param param;

	// Set thread priority to scheduler priority
	param.sched_priority = iThreadPriority;

	// set the schedule parameter
	result = pthread_setschedparam(pthread_self(), threadPolicy, &param);
	if(0 != result)
	{
		handle_error_en(result, "pthread_setschedparam");
		printf(" Cannot set thread priority, result : %d\n",result);
	}
	else
	{
		//printf("Modbus stack :: thread parameters: %d\n", iThreadPriority);
		// success
	}

}  // End of set_thread_sched_param
