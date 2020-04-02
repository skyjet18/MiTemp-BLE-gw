#pragma once

#include "Arduino.h"
#include "BleAdvListener.h"
#include <BLEAddress.h>
#include <BLEScan.h>
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

	time_t        timestamp = 0;

	float        temp = -100.0;

	float        hum  = -1.0;

	float        bat  = -1.0;

public:

	LYWSDCGQData( BLEAddress *address, const char *alias = nullptr )
	{
		this->address = address;
		this->alias = alias;
	}

	/**
	 * @brief Method called when ADV packet is received
	 * @param[in] address Address of advertised device
	 * @param[in] serviceData Service data from ADV packet
	 */
	void onAdvData( BLEAddress *address, std::string &serviceData );

	/**
	 * @brief Dumps data from one sensor to serial output (used for debugging)
	 */
	void dumpToSerial();

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
	 * @brief Registers new LYWSDCGQ devices MAC address that we want to get data from
	 * @param[in] address MAC address of device
	 * @param[in] alias Our device alias
	 */
	void deviceRegister( BLEAddress *address, const char *alias = nullptr );

	/**
	 * @brief Gets device data by MAC address
	 * @param[in] address Address of device we are interested in
	 * @param[out] timestamp How many seconds ago was data refreshed
	 * @param[out] temp Last temperature received
	 * @param[out] hum Last humidity received
	 * @param[out] bat Remaining battery capacity in %
	 * @return Returns true if device with entered MAC was found (registered)
	 */
	bool getData( BLEAddress &address, time_t *timestamp, float *temp = nullptr, float *hum = nullptr, float *bat = nullptr );

	/**
	 * @brief Gets device data by alias
	 * @param[in] address Address of device we are interested in
	 * @param[out] timestamp How many seconds ago was data refreshed
	 * @param[out] temp Last temperature received
	 * @param[out] hum Last humidity received
	 * @param[out] bat Remaining battery capacity in %
	 * @return Returns true if device with entered alias was found (registered)
	 */
	bool getData( const char *room, time_t *timestamp, float *temp = nullptr, float *hum = nullptr, float *bat = nullptr );
private:

	std::forward_list<LYWSDCGQData *> regDevices; // list of registered devices

};

/* ************************************************************************** */

extern LYWSDCGQ lywsdcgq;

/* ************************************************************************** */
