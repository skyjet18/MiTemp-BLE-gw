#pragma once

//#define DEBUG_TO_SERIAL

#ifdef DEBUG_TO_SERIAL
#	define DEBUG_SNPRINTF( ... )    snprintf( __VA_ARGS__ )
#	define SERIAL_PRINTF( ... )      Serial.printf( __VA_ARGS__ )
#	define SERIAL_PRINT( ... )      Serial.print( __VA_ARGS__ )
#	define SERIAL_PRINTLN( ... )    Serial.println( __VA_ARGS__ )
#	define SERIAL_HEXDUMP( ... )    Serial_hexdump( __VA_ARGS__ )
#else
#	define SERIAL_PRINTF( ... )
#	define DEBUG_SNPRINTF( ... )
#	define SERIAL_PRINT( ... )
#	define SERIAL_PRINTLN( ... )
#	define SERIAL_HEXDUMP( ... )
#endif
