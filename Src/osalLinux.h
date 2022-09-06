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

#ifndef INC_OSALLINUX_H_
#define INC_OSALLINUX_H_

#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include "API.h"

// Maximum received priority supported
#define MAX_RECV_PRIORITY -100000

typedef pthread_t  Thread_H;
typedef pthread_mutex_t*  Mutex_H;

/**
 @struct thread_Create_t
 @brief
   The OSAL Create Thread parameters for various OS
*/
typedef struct
{
    size_t dwStackSize;    //stack size
    void* lpStartAddress;  // start address
    void* lpParameter;     // thread parameter
    Thread_H * lpThreadId; // linux thread id

}thread_Create_t;

/**
 @struct Linux_Msg_t
 @brief
   The OSAL Create msg for various OS
*/
typedef struct Linux_Msg
{
	long mtype;      // message type, must be > 0
	void* wParam;    // pointer for handles
	void* lParam;    // long pointer
}Linux_Msg_t;

/**
 @struct Post_Thread_Msg
 @brief
   The OSAL Post Thread msg parameters for various OS
*/
typedef struct Post_Thread_Msg
{
	int idThread;		// Message queue ID
	long  MsgType;      // message type, must be > 0
	void* wParam;       // pointer for handles
	void* lParam;       // long pointer
}Post_Thread_Msg_t;

// The OSAL API will allocate memory from the heap.
void *OSAL_Malloc(int32_t i32MemorySize);
// The OSL API will free the memory allocated in heap
void OSAL_Free(void *pvPointer);
// The OSAL Thread create API will generate Thread for various OS
Thread_H Osal_Thread_Create(thread_Create_t *pThreadParam);
// The OSAL Thread Terminate API will terminate Thread.
bool Osal_Thread_Terminate(Thread_H pThreadTerminate);
// The OSAL Mutex create API will generate Mutex
Mutex_H Osal_Mutex(void);
// The OSAL API Releases the Mutex acquired by process
int32_t Osal_Wait_Mutex(Mutex_H pMtxHandle);
// The OSAL API Releases the Mutex acquired by process
int32_t Osal_Release_Mutex(Mutex_H pMtxHandle);
// close the mutex
int32_t Osal_Close_Mutex( Mutex_H pMtxHandle);
// Copies a message to message queue
int32_t OSAL_Init_Message_Queue();
// Copies a message to message queue
bool OSAL_Post_Message(Post_Thread_Msg_t *pstPostThreadMsg);
// Copies a message from message queue
bool OSAL_Get_Message(Linux_Msg_t *pstQueueMsg, int   msqid);
// Copies a message from message queue
int32_t OSAL_Get_NonBlocking_Message(Linux_Msg_t *pstQueueMsg, int   msqid);
// Delete a message from message queue.
bool OSAL_Delete_Message_Queue(int MsgQId);

#endif // INC_OSALLINUX_H_
