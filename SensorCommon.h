#pragma once

#include "Arduino.h"
#include <BLEDevice.h>

/* ************************************************************************** */
/**
 * @brief Callback used to get notification about data refresh
 */
class SensorDataChangeCbk
{
public:
	virtual ~SensorDataChangeCbk() {}

	/**
	 * @brief Method called when new data are received
	 * @param[in] address Address of device with refreshed data
	 * @param[in] alias Alias of device with refreshed data
	 * @param[in] tempNew Set to true, when temp was refreshed
	 * @param[in] humidityNew Set to true, when humidity was refreshed
	 * @param[in] batNew Set to true, when battery info was refreshed
	 */
	virtual void onData( BLEAddress *address, const char *alias, bool tempNew, bool humidityNew, bool batNew ) = 0;
};

/* ************************************************************************** */
/**
 * @brief Values from sensor and timestamps of last update
 */
struct SensorValues
{
	time_t       tempTimestamp  = 0;
	time_t       humidityTimestamp  = 0;
	time_t       batTimestamp  = 0;

	float       temp       = -100.0;
	float       humidity   = -1.0;
	float       bat        = -1.0;
	float       voltage    = -1.0;
};

/* ************************************************************************** */
