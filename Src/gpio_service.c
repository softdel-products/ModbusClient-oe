/*
 ***************************************************************************************
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
 *	 gpio_service.c
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

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "gpio_service.h"
#include <string.h>
#include <stdlib.h>

static eRETURN_STATUS ExportGPIO(char *gpioPin)
{
	int fd = -1;

	/**Check input pointer */
	if(gpioPin == NULL)
	{
		return ERROR_INVALID_POINTER;
	}

	/**Open GPIO file*/
	fd = open("/sys/class/gpio/export", O_WRONLY);

	/**if fd is negative*/
	if(fd < 0)
	{
		return ERROR_INVALID_FD;
	}

	/**export GPIO*/
	if(write(fd, gpioPin, strlen(gpioPin)) < 0)
	{
		return ERROR_WRITE_GPIO;
	}

	/**Close fd*/
	close(fd);

	return NO_ERROR;
}

static eRETURN_STATUS UnexportGPIO(char *gpioPin)
{
	int fd = -1;

	/**Check input pointer */
	if(gpioPin == NULL)
	{
		return ERROR_INVALID_POINTER;
	}

	/**Open GPIO file*/
	fd = open("/sys/class/gpio/unexport", O_WRONLY);

	/**if fd is negative*/
	if(fd < 0)
	{
		return ERROR_INVALID_FD;
	}

	/**unexport GPIO*/
	if(write(fd, gpioPin, strlen(gpioPin)) < 0)
	{
		return ERROR_WRITE_GPIO;
	}

	/**Close fd*/
	close(fd);

	return NO_ERROR;
}

eRETURN_STATUS SetGPIODirection(char *gpioPin, char *value)
{
	int fd = -1;
	char gpio_path[100] = {0};

	/**Check input pointer */
	if(value == NULL || gpioPin == NULL)
	{
		return ERROR_INVALID_POINTER;
	}

	/**Create GPIO path*/
	sprintf(gpio_path, "/sys/class/gpio/gpio%s/direction", gpioPin);

	/**Open GPIO file*/
	fd = open(gpio_path, O_WRONLY);

	/**if fd is negative*/
	if(fd < 0)
	{
		printf("%s: Failed to open %s\n", __func__, gpio_path);
		return ERROR_INVALID_FD;
	}

	/**Set GPIO direction*/
	if(write(fd, value, strlen(value)) < 0)
	{
		printf("%s: Failed to set GPIO direction %s\n", __func__, gpioPin);
		return ERROR_WRITE_GPIO;
	}

	/**Close fd*/
	close(fd);

	return NO_ERROR;
}

eRETURN_STATUS ReadGPIO(char *gpioPin, char *value)
{
	int fd = -1;
	char gpio_path[100] = {0};

	/**Check input pointer */
	if(value == NULL || gpioPin == NULL)
	{
		return ERROR_INVALID_POINTER;
	}

	/**Create GPIO path*/
	sprintf(gpio_path, "/sys/class/gpio/gpio%s/value", gpioPin);

	/**Open GPIO file*/
	fd = open(gpio_path, O_RDONLY);

	/**if fd is negative*/
	if(fd < 0)
	{
		printf("%s: Failed to open GPIO %s\n", __func__, gpioPin);
		return ERROR_INVALID_FD;
	}

	/**Read GPIO direction*/
	if(read(fd, value, 3*sizeof(char)) < 0)
	{
		printf("%s: Failed to write on GPIO %s\n", __func__, gpioPin);
		return ERROR_READ_GPIO;
	}

	/**Close fd*/
	close(fd);

	return NO_ERROR;
}

eRETURN_STATUS WriteGPIO(char *gpioPin, char *value)
{
	int fd = -1;
	char gpio_path[100] = {0};

	/**Check input pointer */
	if(value == NULL || gpioPin == NULL)
	{
		return ERROR_INVALID_POINTER;
	}

	/**Create GPIO path*/
	sprintf(gpio_path, "/sys/class/gpio/gpio%s/value", gpioPin);

	/**Open GPIO file*/
	fd = open(gpio_path, O_WRONLY);

	/**if fd is negative*/
	if(fd < 0)
	{
		printf("%s: Failed to open GPIO %s\n", __func__, gpioPin);
		return ERROR_INVALID_FD;
	}

	/**Write GPIO direction*/
	if(write(fd, value, strlen(value)) < 0)
	{
		printf("%s: Failed to write on GPIO %s\n", __func__, gpioPin);
		return ERROR_WRITE_GPIO;
	}

	/**Close fd*/
	close(fd);

	return NO_ERROR;
}

eRETURN_STATUS InitDirPin(char *gpio)
{
	eRETURN_STATUS ret_val = NO_ERROR;
	char value[3]= {0};

	/**Unexport respective GPIO*/
	UnexportGPIO(gpio);
	/**Export respective GPIO*/
	ret_val = ExportGPIO(gpio);
	if(ret_val == NO_ERROR)
	{
		/**Set respective GPIO Direction*/
		ret_val = SetGPIODirection(gpio, "out");
		if(ret_val == NO_ERROR)
		{

			strncpy(value, "0", 2);

			/**Set respective GPIO Value*/
			ret_val = WriteGPIO(gpio, value);
		}
	}
	else
	{
		printf("GPIO SET Failed. = %d\n",ret_val);
		return ERROR_WRITE_GPIO;
	}

	return ret_val;
}

eRETURN_STATUS SetValuveDirPin(char *gpio, eRETURN_STATUS eGPIO_STATE)
{
	eRETURN_STATUS ret_val = NO_ERROR;
	char value[3]= {0};

	/**Set respective GPIO Direction*/
	ret_val = SetGPIODirection(gpio, "out");
	if(ret_val == NO_ERROR)
	{
		if(eGPIO_STATE == GPIO_HIGH)
		{
			strncpy(value, "1", 2);
		}
		else if(eGPIO_STATE == GPIO_LOW)
		{
			strncpy(value, "0", 2);
		}
		else
		{
			return INAVLID_GPIO_STATE;
		}
		/**Set respective GPIO Value*/
		ret_val = WriteGPIO(gpio, value);
	}

	return ret_val;
}

