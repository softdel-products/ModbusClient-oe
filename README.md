# modconn
**Modbus master stack "modconn" is a multi-threaded library that incorporates the functionalities of Modbus protocol. The multi-threaded library comes in form of a library (.so) on Linux platform.
Modbus master stack "modconn" is implemented in ‘C' language’ on Linux platform.
It supports 1. Modbus Master TCP, 2. Modbus Master RTU, 3. Basic features, 4. Advance features like multi-read and multi-write**

## Building modconn Stack on Linux Platform:
### This explains how to build the stack.
	1. System Requirements
		1.	OS: Ubuntu 16.04 or later
		2.	GCC: 5.4.0 or later
		3.	Safe string library
		
	2. git clone modconn stack from github: https://github.com/modconn/modconn
	
	3. Pre-requisite for compilation
		To build the library for Modbus TCP mode, a preprocessor “MODBUS_STACK_TCPIP_ENABLED” shall be added in “Release\Src\subdir.mk” (or “Debug\Src\subdir.mk”) file in “gcc” command as shown below:
			gcc -std=c11 -DMODBUS_STACK_TCPIP_ENABLED -I ….
			To build the library for Modbus RTU mode, the preprocessor “MODBUS_STACK_TCPIP_ENABLED” should be removed from above mentioned location.
			
	4. Compiling stack	
		Use make-files to compile stack source-code on Linux platform. The make-files can be found at path mentioned below:
			* To compile stack in debug mode - modconn\Debug\makefile
			* To compile stack in release mode - modconn\Release\makefile
		Use command “make clean all” on the terminal to compile the library.
		Post successful compilation, the output binary file (libModbusMasterStack.so) is created in the folder where the makefile is present (i.e. Debug or Release).
