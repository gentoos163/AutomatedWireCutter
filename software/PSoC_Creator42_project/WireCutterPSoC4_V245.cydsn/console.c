// Console is the generic interface to the command line.
// These functions should not need signficant modification, only
// to be called from the normal loop. Note that adding commands should
// be done in console commands.
//  Original code by @logicalelegance


#include <project.h>
#include <string.h>  // for NULL
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include "console.h"
#include "consoleIo.h"
#include "consoleCommands.h"
#include "tinyprintf.h"

#define MIN(X, Y)		(((X) < (Y)) ? (X) : (Y))
#define NOT_FOUND		(-1l)
#define INT16_MAX_STR_LENGTH 7 // -32767: six characters plus a NULL

// global variables
char mReceiveBuffer[CONSOLE_COMMAND_MAX_LENGTH];
uint32_t mReceivedSoFar;

// local functions
static int32_t ConsoleCommandEndline(const char receiveBuffer[], const  uint32_t filledLength);

static uint32_t ConsoleCommandMatch(const char* name, const char *buffer, uint8_t length);
static eCommandResult_T ConsoleParamFindN(const char * buffer, const uint8_t parameterNumber, uint32_t *startLocation);
static uint32_t ConsoleResetBuffer(char receiveBuffer[], const  uint32_t filledLength, uint32_t usedSoFar);

static eCommandResult_T ConsoleUtilHexCharToInt(char charVal, uint8_t* pInt); // this might be replaceable with *pInt = atoi(str)
static eCommandResult_T ConsoleUtilsIntToHexChar(uint8_t intVal, char* pChar); // this could be replaced with itoa (intVal, str, 16);

uint8_t parseLong(const char *str, long *val);

//##############################################################################
// ConsoleInit
// Initialize the console interface and all it depends on
void ConsoleInit(void)
{
	uint32_t i;
    
	ConsoleIoInit();
    ConsolePrintTitle();
    printf(CONSOLE_PROMPT);

    mReceivedSoFar = 0u;
	for ( i = 0u ; i < CONSOLE_COMMAND_MAX_LENGTH ; i++)
	{
		mReceiveBuffer[i] = '\0';
	}
}

//##############################################################################
void ConsolePrintTitle(void)
{
    printf("\nROBO_SCHNAPPI Command Console");
    printf(STR_ENDLINE);
    printf(STR_ENDLINE);
    
    ConsoleCommandHelp(mReceiveBuffer);
}

//##############################################################################
// ConsoleCommandMatch
// Look to see if the data in the buffer matches the command name given that
// the strings are different lengths and we have parameter separators
//
// 2017/2018 by Piotr Zapart
// added command length matching


static uint32_t ConsoleCommandMatch(const char* name, const char *buffer,uint8_t length)
{
	uint32_t i = 0u;
	uint32_t result = 0u; // match
    
    do
    {
        if ( buffer[i] == name[i] )
		{
			result = 1u;
		}
        else result = 0u;
		i++;  
    }	while ( ( 1u == result ) &&
		( i < CONSOLE_COMMAND_MAX_COMMAND_LENGTH )  &&
		( buffer[i] != PARAMETER_SEPARATER ) &&  ( buffer[i] != COMMAND_ENDLINE ) &&  ( buffer[i] != '\0' )
		);
 
    if (i != length) result = 0;
    
	return result;
}
//##############################################################################
// ConsoleResetBuffer
// In an ideal world, this would just zero out the buffer. However, thre are times when the
// buffer may have data beyond what was used in the last command.
// We don't want to lose that data so we move it to the start of the command buffer and then zero
// the rest.
static uint32_t ConsoleResetBuffer(char receiveBuffer[], const uint32_t filledLength, uint32_t usedSoFar)
{
	uint32_t i = 0;

	while (usedSoFar < filledLength)
	{
		receiveBuffer[i] = receiveBuffer[usedSoFar]; // move the end to the start
		i++;
		usedSoFar++;
	}
	for ( /* nothing */ ; i < CONSOLE_COMMAND_MAX_LENGTH ; i++)
	{
		receiveBuffer[i] = '\0';
	}
	return (filledLength - usedSoFar);
}
//##############################################################################
// ConsoleCommandEndline
// Check to see where in the buffer stream the endline is; that is the end of the command and parameters
static int32_t ConsoleCommandEndline(const char receiveBuffer[], const  uint32_t filledLength)
{
	uint32_t i = 0;
	int32_t result = NOT_FOUND; // if no endline is found, then return -1 (NOT_FOUND)

	while ( (COMMAND_ENDLINE != receiveBuffer[i]) && ( i < filledLength ) )
	{
		i++;
	}
	if ( i < filledLength )
	{
		result = i;
	}
	return result;
}

//##############################################################################
// ConsoleProcess
// Looks for new inputs, checks for endline, then runs the matching command.
// Call ConsoleProcess from a loop, it will handle commands as they become available
void ConsoleProcess(void)
{
	const sConsoleCommandTable_T* commandTable;
	uint32_t received = 0;
	uint32_t cmdIndex;
	int32_t  cmdEndline;
	int32_t  found;
    eCommandResult_T result;

	commandTable = ConsoleCommandsGetTable();

    	ConsoleIoReceive((uint8_t*)&(mReceiveBuffer[mReceivedSoFar]), ( CONSOLE_COMMAND_MAX_LENGTH - mReceivedSoFar ), &received);
    	if ( received > 0u )
    	{
    		mReceivedSoFar += received;
    		cmdEndline = ConsoleCommandEndline(mReceiveBuffer, mReceivedSoFar);
    		if ( cmdEndline >= 0 )  // have complete string, find command
    		{
    			cmdIndex = 0u;
    			found = NOT_FOUND;
    			while ( (NULL != commandTable[cmdIndex].name) && (NOT_FOUND == found) )
    			{
    				if ( ConsoleCommandMatch(commandTable[cmdIndex].name, mReceiveBuffer, commandTable[cmdIndex].nameLength) )
    				{
                        if (commandTable[cmdIndex].returnType == COMMAND_RET_TYPE_MSG)
                        {
                            printf(STR_ENDLINE);
                        }
    					result = commandTable[cmdIndex].execute(mReceiveBuffer);                       
                        
                        if (commandTable[cmdIndex].returnType == COMMAND_RET_TYPE_MSG)
                        {
                            if ( COMMAND_SUCCESS != result )
        					{
        						printf("Error: ");
        						printf(mReceiveBuffer);
                                printf(STR_ENDLINE);
        						printf("Help: ");
        						printf((char*)commandTable[cmdIndex].help);
        						printf(STR_ENDLINE);
        					}
                            else 
                            {
                                printf(STR_ENDLINE);
                                printf("ok");
                                printf(STR_ENDLINE);
                            }
                        }
    					found = cmdIndex;
    				}
    				else
    				{
    					cmdIndex++;
    				}
    			}
    			if ( ( cmdEndline != 0 ) && ( NOT_FOUND == found ) && commandTable[cmdIndex].returnType == COMMAND_RET_TYPE_MSG)
    			{
                    printf(STR_ENDLINE);
    				printf("Command not found.");
    				printf(STR_ENDLINE);
    			}
    			mReceivedSoFar = ConsoleResetBuffer(mReceiveBuffer, mReceivedSoFar, cmdEndline);
                if (commandTable[cmdIndex].returnType == COMMAND_RET_TYPE_MSG)
                {
                    printf(STR_ENDLINE);
    			    printf(CONSOLE_PROMPT);
                }
    		}
            else                                                                // no endline found and 
            {
                if ( mReceivedSoFar >= CONSOLE_COMMAND_MAX_LENGTH)              // buffer is full
                {
                	for ( uint8_t i = 0 ; i < CONSOLE_COMMAND_MAX_LENGTH ; i++) //clear the buffer
                	{
                		mReceiveBuffer[i] = '\0';   
                	}
                    mReceivedSoFar = 0;                                         // reset the received bytes count
                }
            }
        }
}
//##############################################################################
// ConsoleParamFindN
// Find the start location of the nth parametr in the buffer where the command itself is parameter 0
static eCommandResult_T ConsoleParamFindN(const char * buffer, const uint8_t parameterNumber, uint32_t *startLocation)
{
	uint32_t bufferIndex = 0;
	uint32_t parameterIndex = 0;
	eCommandResult_T result = COMMAND_SUCCESS;


	while ( (parameterNumber != parameterIndex) && (bufferIndex < CONSOLE_COMMAND_MAX_LENGTH) )
	{
		if (PARAMETER_SEPARATER == buffer[bufferIndex])
		{
			parameterIndex++;
		}
		bufferIndex++;
	}
	if  ( CONSOLE_COMMAND_MAX_LENGTH == bufferIndex )
	{
		result = COMMAND_PARAMETER_ERROR;
	}
	else
	{
		*startLocation = bufferIndex;
	}
	return result;
}
//##############################################################################
// ConsoleReceiveParamInt16
// Identify and obtain a parameter of type int16_t, sent in in decimal, possibly with a negative sign.
// Note that this uses atoi, a somewhat costly function. You may want to replace it, see ConsoleReceiveParamHexUint16
// for some ideas on how to do that.
eCommandResult_T ConsoleReceiveParamInt16(const char * buffer, const uint8_t parameterNumber, long* parameterInt)
{
	uint32_t startIndex = 0;
	uint32_t i;
	eCommandResult_T result;
	char charVal;
	char str[INT16_MAX_STR_LENGTH];
    
	result = ConsoleParamFindN(buffer, parameterNumber, &startIndex);

	i = 0;
	charVal = buffer[startIndex + i];
	while ( ( COMMAND_ENDLINE != charVal ) && ( PARAMETER_SEPARATER != charVal )
		&& ( i < INT16_MAX_STR_LENGTH ) )
	{
		str[i] = charVal;					// copy the relevant part
		i++;
		charVal = buffer[startIndex + i];
	}
	if ( i == INT16_MAX_STR_LENGTH)
	{
		result = COMMAND_PARAMETER_ERROR;
	}
	if ( COMMAND_SUCCESS == result )
	{
		str[i] = '\0';
        if (parseLong(str,parameterInt))        result |=COMMAND_SUCCESS;
        else                                    result = COMMAND_PARAMETER_ERROR;  
        
	}
	return result;
}
//##############################################################################
// ConsoleReceiveParamHexUint16
// Identify and obtain a parameter of type uint16, sent in as hex. This parses the number and does not use
// a library function to do it.
eCommandResult_T ConsoleReceiveParamHexUint16(const char * buffer, const uint8_t parameterNumber, uint16_t* parameterUint16)
{
	uint32_t startIndex = 0;
	uint16_t value = 0;
	uint32_t i;
	eCommandResult_T result;
	uint8_t tmpUint8;

	result = ConsoleParamFindN(buffer, parameterNumber, &startIndex);
	if ( COMMAND_SUCCESS == result )
	{
		// bufferIndex points to start of integer
		// next separator or newline or NULL indicates end of parameter
		for ( i = 0u ; i < 4u ; i ++)   // U16 must be less than 4 hex digits: 0xFFFF
		{
			if ( COMMAND_SUCCESS == result )
			{
				result = ConsoleUtilHexCharToInt(buffer[startIndex + i], &tmpUint8);
			}
			if ( COMMAND_SUCCESS == result )
			{
				value = (value << 4u);
				value += tmpUint8;
			}
		}
		if  ( COMMAND_PARAMETER_END == result )
		{
			result = COMMAND_SUCCESS;
		}
		*parameterUint16 = value;
	}
	return result;
}
//##############################################################################
// ConsoleSendParamHexUint16
// Send a parameter of type uint16 as hex.
// This does not use a library function to do it (though you could
// do itoa (parameterUint16, out, 16);  instead of building it up
eCommandResult_T ConsoleSendParamHexUint16(uint16_t parameterUint16)
{
	uint32_t i;
	char out[4u + 1u];  // U16 must be less than 4 hex digits: 0xFFFF, end buffer with a NULL
	eCommandResult_T result = COMMAND_SUCCESS;
	uint8_t tmpUint8;

	for ( i = 0u ; i < 4u ; i ++)   // U16 must be less than 4 hex digits: 0xFFFF
	{
		if ( COMMAND_SUCCESS == result )
		{
			tmpUint8 = ( parameterUint16 >> (12u - (i*4u)) & 0xF);
			result = ConsoleUtilsIntToHexChar(tmpUint8, &(out[i]));
		}
	}
	out[i] = '\0';
	printf(out);

	return COMMAND_SUCCESS;
}
//##############################################################################
// ConsoleSendParamInt16
// Send a parameter of type int16 using tinyprintf library
eCommandResult_T ConsoleSendParamInt16(int16_t parameterInt)
{
	printf("%d",parameterInt);

	return COMMAND_SUCCESS;
}
//##############################################################################
// ConsoleUtilHexCharToInt
// Converts a single hex character (0-9,A-F) to an integer (0-15)
static eCommandResult_T ConsoleUtilHexCharToInt(char charVal, uint8_t* pInt)
{
    eCommandResult_T result = COMMAND_SUCCESS;

    if ( ( '0' <= charVal ) && ( charVal <= '9' ) )
    {
        *pInt = charVal - '0';
    }
    else if ( ( 'A' <= charVal ) && ( charVal <= 'F' ) )
    {
        *pInt = 10u + charVal - 'A';
    }
    else if( ( 'a' <= charVal ) && ( charVal <= 'f' ) )
    {
        *pInt = 10u + charVal - 'a';
    }
	else if ( ( COMMAND_ENDLINE == charVal ) || ( PARAMETER_SEPARATER == charVal ) )
	{
		result = COMMAND_PARAMETER_END;

	}
    else
    {
        result = COMMAND_PARAMETER_ERROR;
    }

    return result;
}
//##############################################################################
// ConsoleUtilsIntToHexChar
// Converts an integer nibble (0-15) to a hex character (0-9,A-F)
static eCommandResult_T ConsoleUtilsIntToHexChar(uint8_t intVal, char* pChar)
{
    eCommandResult_T result = COMMAND_SUCCESS;

    if ( intVal <= 9u )
    {
        *pChar = intVal + '0';
    }
    else if ( ( 10u <= intVal ) && ( intVal <= 15u ) )
    {
        *pChar = intVal - 10u + 'A';
    }
    else
    {
        result = COMMAND_PARAMETER_ERROR;
    }

    return result;
}
//##############################################################################
uint8_t parseLong(const char *str, long *val)
{
    char *temp;
    uint8_t rc = 1;
    errno = 0;
    *val = strtol(str, &temp, 0);

    if (temp == str || *temp != '\0' ||
        ((*val == LONG_MIN || *val == LONG_MAX) && errno == ERANGE))
        rc = 0;

    return rc;
}
//##############################################################################


