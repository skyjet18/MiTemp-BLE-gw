#include "mitemp_ble_gw_esp32.h"

#include <WebServer.h>
#include "BleAdvListener.h"
#include "LYWSD03MMC.h"
#include "LYWSDCGQ.h"

#define DEBUG_TO_SERIAL  // uncomment to disable debug output
#include "debug.h"

/* ************************************************************************** */

struct MyDevices
{
	BLEAddress     address;
	const char   *alias;
	bool           isLYWSD03MMC;
	const uint8_t  key[16];
} MyDevices[] = {
	{ BLEAddress("58:2D:34:XX:XX:XX"), "Round", false, { 0x00 } },
	{ BLEAddress("A4:C1:38:XX:XX:XX"), "Square", true, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10 } },
};

#define MY_DEVICES_COUNT (int)(sizeof( MyDevices ) / sizeof( struct MyDevices ))

/* ************************************************************************** */

const int lywsd03mmcDataRefresh = 180; // changing this value to 0 will enable passive only mode for LYWSD03MMC (in case you have decryption key for it)

/* ************************************************************************** */

const char *ssid     = "MyWifiName";
const char *password = "MyWifiPassword";

/* ************************************************************************** */

WebServer        web_server(80);

/* ************************************************************************** */

String handle_temp( void )
{
	String response = "";
	time_t  now = time( NULL );
	struct SensorValues values;

	if( web_server.args() == 0 )
	{

		for( int i = 0; i < MY_DEVICES_COUNT; i++ )
		{
			if( MyDevices[i].isLYWSD03MMC )
			{
				lywsd03mmc.getData( MyDevices[i].address, &values );
			}
			else
			{
				lywsdcgq.getData( MyDevices[i].address, &values );
			}

			char buff[100];
			snprintf( buff, 100, "%s, %ld, %.1f, %.1f, %.3f\n", MyDevices[i].alias, now - values.tempTimestamp, values.temp, values.humidity, values.bat );
			response += buff;
		}
	}
	else if( web_server.hasArg( "alias" ) == false )
	{
		response  = "Only alias argument is supported";
	}
	else
	{
		if( lywsdcgq.getData( web_server.arg( "alias" ).c_str(), &values ) == false &&
			lywsd03mmc.getData( web_server.arg( "alias" ).c_str(), &values ) == false )
		{
			response = ", , , ";
		}
		else
		{
			char buff[100];
			snprintf( buff, 100, "%ld, %.1f, %.1f, %.3f\n", now - values.tempTimestamp, values.temp, values.humidity, values.bat );
			response = buff;
		}
	}

	return response;
}

/* ************************************************************************** */

class LYWSD03MMCChangeCbk : public SensorDataChangeCbk
{
public:
	void onData( BLEAddress *address, const char *alias, bool tempNew, bool humidityNew, bool batNew )
	{
		struct SensorValues values;

		lywsd03mmc.getData( *address, &values );

		if( tempNew )
		{
			SERIAL_PRINTF( "New sensor data: alias=%s, sensor=LYWSD03MMC, temp=%.1f\n",
					alias, values.temp );
		}

		if( humidityNew )
		{
			SERIAL_PRINTF( "New sensor data: alias=%s, sensor=LYWSD03MMC, humidity=%.1f\n",
					alias, values.humidity );
		}

		if( batNew )
		{
			SERIAL_PRINTF( "New sensor data: alias=%s, sensor=LYWSD03MMC, bat=%.1f\n",
					alias, values.bat );
		}
	}
};

class LYWSDCGQChangeCbk : public SensorDataChangeCbk
{
public:
	void onData( BLEAddress *address, const char *alias, bool tempNew, bool humidityNew, bool batNew )
	{
		struct SensorValues values;
		char buff[160];

		lywsdcgq.getData( *address, &values );

		if( tempNew )
		{
			SERIAL_PRINTF( "New sensor data: alias=%s, sensor=LYWSDCGQ, temp=%.1f\n",
					alias, values.temp );
		}

		if( humidityNew )
		{
			SERIAL_PRINTF( "New sensor data: alias=%s, sensor=LYWSDCGQ, humidity=%.1f\n",
					alias, values.humidity );
		}

		if( batNew )
		{
			SERIAL_PRINTF( "New sensor data: alias=%s, sensor=LYWSDCGQ, bat=%.1f\n",
					alias, values.bat );
		}
	}
};

/* ************************************************************************** */

void setup()
{

	Serial.begin( 115200 );
	delay(500);

	SERIAL_PRINT( "Connecting to wifi name: " );
	SERIAL_PRINTLN(ssid);

	WiFi.mode(WIFI_STA);
	WiFi.setAutoReconnect( true );
	WiFi.setAutoConnect( true );
	WiFi.begin(ssid, password);

	delay(4000);

	for( int i = 0; i < 10; i++ )
	{
		if( WiFi.isConnected() )
		{
			SERIAL_PRINT( "WiFi connected with IP address " );
			SERIAL_PRINTLN( WiFi.localIP().toString() );

			SERIAL_PRINT( "Seconds needed to connect: " );
			SERIAL_PRINTLN( i );
			break;
		}

		delay( 1000 );
	}

	web_server.on("/", HTTP_GET, []() {
		web_server.send(200, "text/plain", handle_temp() );
	});

	web_server.onNotFound( []() {
		web_server.send( 404, "text/plain", "not found" );
	});

	web_server.begin();

	delay( 2000 );

	SERIAL_PRINTLN( "Starting BLE" );
	BLEDevice::init("");

	bleAdvListener.init();
	lywsd03mmc.init( lywsd03mmcDataRefresh );
    lywsdcgq.init( 60 );

	for( int i = 0; i < MY_DEVICES_COUNT; i++ )
	{
		if( MyDevices[i].isLYWSD03MMC )
		{
			lywsd03mmc.deviceRegister( &MyDevices[i].address, MyDevices[i].alias, MyDevices[i].key );
		}
		else
		{
			lywsdcgq.deviceRegister( &MyDevices[i].address, MyDevices[i].alias );
		}
	}

	lywsd03mmc.cbkRegister( new LYWSD03MMCChangeCbk() );
	lywsdcgq.cbkRegister( new LYWSDCGQChangeCbk() );

	SERIAL_PRINTLN( "Setup completed" );
}

/* ************************************************************************** */

void loop()
{
	web_server.handleClient();
	lywsd03mmc.process();
	bleAdvListener.process();
}

/* ************************************************************************** */
