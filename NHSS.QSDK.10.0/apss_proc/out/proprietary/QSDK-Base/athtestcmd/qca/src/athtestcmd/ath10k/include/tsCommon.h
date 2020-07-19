#if !defined(_HOST_COMMMON_H)
#define _HOST_COMMMON_H

#define LINEBUF_SIZE 256
#define KEY_SIZE_MAX 72
#define FILENAME_SIZE 64

#define FALSE 0
#define TRUE  1

#if 0
#define A_BOOL long
#define A_UINT32 unsigned long
#endif 

//
#if defined(_HOST_SIM_TESTING)
#define A_PRINTF 		printf
#define A_PRINTF_ALWAYS		printf
#define ATH_MAC_LEN  		6
#define SRAM_DATA
#define SRAM_TEXT

#define num2Str(x)   str(x)
#define str(x)       #x

#ifdef _DEBUG
#ifdef Linux
#define debug(msg)   fputs(__FILE__ ":" num2Str(__LINE__) " - " msg, stderr)
#else
#define debug	     printf
#endif //Linux
#else
#define debug(msg)
#endif //_DEBUG
#else
#define debug	     A_PRINTF_ALWAYS
#endif //defined(_HOST_SIM_TESTING)

#ifndef MAX_NUM_CHAINS
#define MAX_NUM_CHAINS		4
#endif

#endif //#if !defined(_HOST_COMMMON_H)
