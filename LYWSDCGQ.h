#pragma once

#include "Arduino.h"
#include "BleAdvListener.h"
#include <BLEAddress.h>
#include <BLEScan.h>
#include "SensorCommon.h"
#include <forward_list>

/* ************************************************************************** */

/**
 * @brief Class with data from one LYWSDCGQ sensor
 */
class LYWSDCGQData : public BleAdvListenerCbk
{
private:
	BLEAddress   *address;

	const char  *alias;

	struct       SensorValues values;
	time_t        advTimestamp = 0;
	time_t        cbkWaitTime = 10;

	time_t       nextTempNotify = 0;
	time_t       nextHumidityNotify = 0;
	time_t       nextBatNotify = 0;

	std::forward_list<SensorDataChangeCbk *> *regCbks = nullptr; // list with registered callbacks

public:

	LYWSDCGQData( BLEAddress *address, const char *alias = nullptr )
	{
		this->address = address;
		this->alias = alias;

		memset( &values, 0, sizeof( struct SensorValues ) );
	}

	/**
	 * @brief Method called when ADV packet is received
	 * @param[in] address Address of advertised device
	 * @param[in] serviceDataUUID UUID of advertised service data
	 * @param[in] serviceData Service data from ADV packet
	 */
	void onAdvData( BLEAddress *address, uint16_t serviceDataUUID, std::string &serviceData );

	friend class LYWSDCGQ;
};

/* ************************************************************************** */
/**
 * @brief Base class for working with LYWSDCGQ sensors
 */
class LYWSDCGQ
{
public:
	/**
	 * @brief Initialise class.
	 * This method must be called once before any other calls (in setup() funcion)
	 *
	 * @param[in] cbkWaitTime Minimum time in seconds between two callback calls for the same sensor value update
	 */
	void init( time_t cbkWaitTime = 10 );

	/**
	 * @brief Registers new LYWSDCGQ devices MAC address that we want to get data from
	 * @param[in] address MAC address of device
	 * @param[in] alias Our device alias
	 */
	void deviceRegister( BLEAddress *address, const char *alias = nullptr );

	/**
	 * @brief Gets device data by alias
	 * @param[in] alias Alias of device we are interested in
	 * @param[out] values Values for sensor with given alias
	 * @return Returns true if device with entered alias was found (registered)
	 */
	bool getData( const char *alias, struct SensorValues *values );

	/**
	 * @brief Gets device data by MAC address
	 * @param[in] address Address of device we are interested in
	 * @param[out] values Values for sensor with given address
	 * @return Returns true if device with entered MAC was found (registered)
	 */
	bool getData( BLEAddress &address, struct SensorValues *values );

	/**
	 * @brief Registers new callback called on data refresh
	 * @param[in] cbk Pointer to callback class
	 */
	void cbkRegister( SensorDataChangeCbk *cbk );

private:

	time_t        cbkWaitTime = 10;

	std::forward_list<LYWSDCGQData *> regDevices; // list of registered devices

	std::forward_list<SensorDataChangeCbk *> regCbks; // list with registered callbacks
};

/* ************************************************************************** */

extern LYWSDCGQ lywsdcgq;

/* ************************************************************************** */
