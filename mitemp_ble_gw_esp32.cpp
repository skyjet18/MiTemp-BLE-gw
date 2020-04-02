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
	bool           needConnect;
} MyDevices[] = {
	{ BLEAddress("58:2D:34:XX:XX:XX"), "Round", false },
	{ BLEAddress("A4:C1:38:XX:XX:XX"), "Square", true },
};

#define MY_DEVICES_COUNT (int)(sizeof( MyDevices ) / sizeof( struct MyDevices ))

/* ************************************************************************** */

BLEClient* pClient = nullptr;

/* ************************************************************************** */

const char *ssid     = "MyWifiName";
const char *password = "MyWifiPassword";

/* ************************************************************************** */

WebServer        web_server(80);

/* ************************************************************************** */

String handle_temp( void )
{
	String response = "";

	if( web_server.args() == 0 )
	{
		time_t   timestamp;
		float   temp;
		float   hum;
		float   bat;

		for( int i = 0; i < MY_DEVICES_COUNT; i++ )
		{
			if( MyDevices[i].needConnect )
			{
				lywsd03mmc.getData( MyDevices[i].address, &timestamp, &temp, &hum, &bat );
			}
			else
			{
				lywsdcgq.getData( MyDevices[i].address, &timestamp, &temp, &hum, &bat );
			}

			char buff[100];
			snprintf( buff, 100, "%s, %ld, %.1f, %.1f, %.3f\n", MyDevices[i].alias, timestamp, temp, hum, bat );
			response += buff;
		}
	}
	else if( web_server.hasArg( "alias" ) == false )
	{
		response  = "Only alias argument is supported";
	}
	else
	{
		time_t   timestamp;
		float   temp;
		float   hum;
		float   bat;

		if( lywsdcgq.getData( web_server.arg( "alias" ).c_str(), &timestamp, &temp, &hum, &bat ) == false &&
			lywsd03mmc.getData( web_server.arg( "alias" ).c_str(), &timestamp, &temp, &hum, &bat ) == false )
		{
			response = ", , , ";
		}
		else
		{
			char buff[100];
			snprintf( buff, 100, "%ld, %.1f, %.1f, %.3f\n", timestamp, temp, hum, bat );
			response = buff;
		}
	}

	return response;
}

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
	lywsd03mmc.init( 180 );

	for( int i = 0; i < MY_DEVICES_COUNT; i++ )
	{
		if( MyDevices[i].needConnect )
		{
			lywsd03mmc.deviceRegister( &MyDevices[i].address, MyDevices[i].alias );
		}
		else
		{
			lywsdcgq.deviceRegister( &MyDevices[i].address, MyDevices[i].alias );
		}
	}

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
