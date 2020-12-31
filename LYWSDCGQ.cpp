#include "LYWSDCGQ.h"

#include "Arduino.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "debug.h"

/* ************************************************************************** */

LYWSDCGQ lywsdcgq;

/* ************************************************************************** */
/**
 * @brief Method called when ADV packet is received
 * @param[in] address Address of advertised device
 * @param[in] serviceDataUUID UUID of advertised service data
 * @param[in] serviceData Service data from ADV packet
 */
void LYWSDCGQData::onAdvData( BLEAddress *address, uint16_t serviceDataUUID, std::string &serviceData )
{
	if( this->address->equals( *address ) == false )
	{
		return;
	}

	SERIAL_PRINTF("Found device: %s alias: %s\n", address->toString().c_str(), alias );

	uint8_t tempData[32];
	size_t  tempDataLen = 0;

	size_t sdLength;

	advTimestamp = time( NULL );

	if( serviceDataUUID != 0xFE95 )
	{
		SERIAL_PRINTF("Received service data with not interested UUID %u\n", serviceDataUUID );
		return;
	}

	if( (sdLength = serviceData.length()) > 11 )
	{
		serviceData.copy( (char*) tempData, sdLength - 11, 11 );
		tempDataLen = sdLength - 11;
	}

	if( !tempDataLen )
	{
		SERIAL_PRINTF("We don't have enough service data\n");
		return;
	}

	uint8_t serviceDataPerfix[4];
	serviceData.copy( (char*) serviceDataPerfix, 4, 0 );

	if( serviceDataPerfix[0] != 0x50 || serviceDataPerfix[1] != 0x20 )
	{
		SERIAL_PRINTF("Frame control data 0x%02X 0x%02X doesn't match expected values\n", serviceDataPerfix[0], serviceDataPerfix[1] );
		return;
	}

	if( serviceDataPerfix[2] != 0xAA || serviceDataPerfix[3] != 0x01 )
	{
		SERIAL_PRINTF("Device type 0x%02X 0x%02X doesn't match expectet value for LYWSDCGQ sensor\n", serviceDataPerfix[2], serviceDataPerfix[3] );
		return;
	}

	bool tempNew = false;
	bool humidityNew = false;
	bool batNew = false;

	switch( tempData[0] )
	{
		case 0x04:
		{
			if( advTimestamp > nextTempNotify )
			{
				tempNew = true;
				nextTempNotify = advTimestamp + cbkWaitTime;
			}

			values.temp = ((tempData[4] << 8) | tempData[3]) / 10.0;
			values.tempTimestamp = advTimestamp;
		}
		break;

		case 0x06:
		{
			if( advTimestamp > nextHumidityNotify )
			{
				humidityNew = true;
				nextHumidityNotify = advTimestamp + cbkWaitTime;
			}

			values.humidity = ((tempData[4] << 8) | tempData[3]) / 10.0;
			values.humidityTimestamp = advTimestamp;
		}
		break;

		case 0x0A:
		{
			if( advTimestamp > nextBatNotify )
			{
				batNew = true;
				nextBatNotify = advTimestamp + cbkWaitTime;
			}

			values.bat = (float) tempData[3];
			values.batTimestamp = advTimestamp;

			// emulate voltage -> 3.1V = 100%, 2.1V = 0%
			values.voltage = 2.1 + (values.bat / 100.0);

		}
		break;

		case 0x0D:
		{
			if( advTimestamp > nextTempNotify )
			{
				tempNew = true;
				nextTempNotify = advTimestamp + cbkWaitTime;
			}

			if( advTimestamp > nextHumidityNotify )
			{
				humidityNew = true;
				nextHumidityNotify = advTimestamp + cbkWaitTime;
			}

			values.temp = ((tempData[4] << 8) | tempData[3]) / 10.0;
			values.humidity = ((tempData[6] << 8) | tempData[5]) / 10.0;

			values.humidityTimestamp = advTimestamp;
			values.tempTimestamp = advTimestamp;
		}
		break;
	}

	if( tempNew || humidityNew || batNew )
	{
		for( auto it = regCbks->cbegin(); it != regCbks->cend(); it++ )
		{
			(*it)->onData( address, alias, tempNew, humidityNew, batNew );
		}
	}
}

/* ************************************************************************** */
/**
 * @brief Initialise class.
 * This method must be called once before any other calls (in setup() funcion)
 *
 * @param[in] cbkWaitTime Minimum time in seconds between two callback calls for the same sensor value update
 */
void LYWSDCGQ::init( time_t cbkWaitTime )
{
	this->cbkWaitTime = cbkWaitTime;
}

/* ************************************************************************** */
/**
 * @brief Registers new LYWSDCGQ devices MAC address that we want to get data from
 * @param[in] address MAC address of device
 * @param[in] alias Our device alias
 */
void LYWSDCGQ::deviceRegister( BLEAddress *address, const char *alias )
{
	LYWSDCGQData *data = new LYWSDCGQData( address, alias );

	data->cbkWaitTime = cbkWaitTime;
	data->regCbks = &regCbks;

	bleAdvListener.cbkRegister( data );
	regDevices.push_front( data );
}

/* ************************************************************************** */
/**
 * @brief Gets device data by alias
 * @param[in] alias Alias of device we are interested in
 * @param[out] values Values for sensor with given alias
 * @return Returns true if device with entered alias was found (registered)
 */
bool LYWSDCGQ::getData( const char *alias, struct SensorValues *values )
{
	for( auto it = regDevices.cbegin(); it != regDevices.cend(); it++ )
	{
		if( (*it)->alias && strcmp( (*it)->alias, alias ) == 0 )
		{
			memcpy( values, &(*it)->values, sizeof( struct SensorValues ) );

			return true;
		}
	}

	return false;
}

/* ************************************************************************** */
/**
 * @brief Gets device data by address
 * @param[in] address Address of device we are interested in
 * @param[out] values Values for sensor with given address
 * @return Returns true if device with entered alias was found (registered)
 */
bool LYWSDCGQ::getData( BLEAddress &address, struct SensorValues *values )
{
	for( auto it = regDevices.cbegin(); it != regDevices.cend(); it++ )
	{
		if( (*it)->address->equals( address ) == true )
		{
			memcpy( values, &(*it)->values, sizeof( struct SensorValues ) );

			return true;
		}
	}

	return false;
}

/* ************************************************************************** */
/**
 * @brief Registers new callback called on data refresh
 * @param[in] cbk Pointer to callback class
 */
void LYWSDCGQ::cbkRegister( SensorDataChangeCbk *cbk )
{
	regCbks.push_front( cbk );
}

/* ************************************************************************** */
