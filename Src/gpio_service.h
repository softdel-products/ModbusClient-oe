 /***************************************************************************************
 *
 *                   Copyright (c) by SoftDEL Systems Ltd.
 *
 *   This software is copyrighted by and is the sole property of SoftDEL
 *   Systems Ltd. All rights, title, ownership, or other interests in the
 *   software remain the property of  SoftDEL Systems Ltd. This software
 *   may only be used in accordance with the corresponding license
 *   agreement. Any unauthorized use, duplication, transmission,
 *   distribution, or disclosure of this software is expressly forbidden.
 *
 *   This Copyright notice may not be removed or modified without prior
 *   written consent of SoftDEL Systems Ltd.
 *
 *   SoftDEL Systems Ltd. reserves the right to modify this software
 *   without notice.
 *
 *   SoftDEL Systems Ltd.						india@softdel.com
 *   3rd Floor, Pentagon P4,					http://www.softdel.com
 *	 Magarpatta City, Hadapsar
 *	 Pune - 411 028
 *
 *
 *   FILE
 *	 gpio_service.h
 *
 *   AUTHORS
 *	 Kailas Vadak
 *
 *   DESCRIPTION
 *
 *   RELEASE HISTORY
 *
 *
 *************************************************************************************/

#ifndef LED_BOOTSWITCH_SERVICE_H_
#define LED_BOOTSWITCH_SERVICE_H_

#define GPIO_EXPORT __attribute__ ((visibility ("default")))

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include "ERRORCODE.h"
/*
typedef enum{
	//NO_ERROR,
	//ERROR_INVALID_FD,
	ERROR_READ_GPIO,
	ERROR_WRITE_GPIO,
	//ERROR_INVALID_POINTER,
	GPIO_HIGH,
	GPIO_LOW,
	INAVLID_GPIO_STATE,
	//FAILED
}eRETURN_STATUS;
*/
char DirCtrlPin[3];
GPIO_EXPORT ERRORCODE SetGPIODirection(char *gpioPin, char *value);
GPIO_EXPORT ERRORCODE ReadGPIO(char *gpioPin, char *value);
GPIO_EXPORT ERRORCODE SetValuveDirPin(char *gpio, ERRORCODE eGPIO_STATE);


#endif /* LED_BOOTSWITCH_SERVICE_H_ */
