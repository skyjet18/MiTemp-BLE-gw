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
 * @param[in] serviceData Service data from ADV packet
 */
void LYWSDCGQData::onAdvData( BLEAddress *address, std::string &serviceData )
{
	if( this->address->equals( *address ) == false )
	{
		return;
	}

	SERIAL_PRINTF("Found device: %s alias: %s\n",
			address().toString().c_str(), alias );

	uint8_t tempData[32];
	size_t  tempDataLen = 0;

	size_t sdLength;

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

	switch( tempData[0] )
	{
		case 0x04:
		{
			temp = ((tempData[4] << 8) | tempData[3]) / 10.0;
			timestamp = time( NULL );
		}
		break;

		case 0x06:
		{
			hum = ((tempData[4] << 8) | tempData[3]) / 10.0;
			timestamp = time( NULL );
		}
		break;

		case 0x0A:
		{
			bat = (float) tempData[3];
		}
		break;

		case 0x0D:
		{
			temp = ((tempData[4] << 8) | tempData[3]) / 10.0;
			hum = ((tempData[6] << 8) | tempData[5]) / 10.0;
			timestamp = time( NULL );
		}
		break;
	}
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

	bleAdvListener.cbkRegister( data );
	regDevices.push_front( data );
}

/* ************************************************************************** */
/**
 * @brief Gets device data by alias
 * @param[in] address Address of device we are interested in
 * @param[out] timestamp How many seconds ago was data refreshed
 * @param[out] temp Last temperature received
 * @param[out] hum Last humidity received
 * @param[out] bat Remaining battery capacity in %
 * @return Returns true if device with entered alias was found (registered)
 */
bool LYWSDCGQ::getData( const char *alias, time_t *timestamp, float *temp, float *hum, float *bat )
{
	for( auto it = regDevices.cbegin(); it != regDevices.cend(); it++ )
	{
		if( (*it)->alias && strcmp( (*it)->alias, alias ) == 0 )
		{
			if( timestamp )
			{
				*timestamp = time( NULL ) - (*it)->timestamp;
			}

			if( temp )
			{
				*temp = (*it)->temp;
			}

			if( hum )
			{
				*hum = (*it)->hum;
			}

			if( bat )
			{
				*bat = (*it)->bat;
			}

			return true;
		}
	}

	return false;
}

/* ************************************************************************** */
/**
 * @brief Gets device data by MAC address
 * @param[in] address Address of device we are interested in
 * @param[out] timestamp How many seconds ago was data refreshed
 * @param[out] temp Last temperature received
 * @param[out] hum Last humidity received
 * @param[out] bat Remaining battery capacity in %
 * @return Returns true if device with entered MAC was found (registered)
 */
bool LYWSDCGQ::getData( BLEAddress &address, time_t *timestamp, float *temp, float *hum, float *bat )
{
	for( auto it = regDevices.cbegin(); it != regDevices.cend(); it++ )
	{
		if( (*it)->address->equals( address ) == true )
		{
			if( timestamp )
			{
				*timestamp = time( NULL ) - (*it)->timestamp;
			}

			if( temp )
			{
				*temp = (*it)->temp;
			}

			if( hum )
			{
				*hum = (*it)->hum;
			}

			if( bat )
			{
				*bat = (*it)->bat;
			}

			return true;
		}
	}

	return false;
}

/* ************************************************************************** */
