#include <Arduino.h>
#include <BLEDevice.h>
#include <forward_list>

#pragma once

/* ************************************************************************** */
/**
 * @brief Callback used to receive service data from BLE ADV packets
 */
class BleAdvListenerCbk
{
public:
	virtual ~BleAdvListenerCbk() {}

	/**
	 * @brief Method called when ADV packet is received
	 * @param[in] address Address of advertised device
	 * @param[in] serviceDataUUID UUID of advertised service data
	 * @param[in] serviceData Service data from ADV packet
	 */
	virtual void onAdvData( BLEAddress *address, uint16_t serviceDataUUID, std::string &serviceData ) = 0;
};

/* ************************************************************************** */

class BleAdvListener
{
public:
	/**
	 * @brief Initialise BLE ADV listener
	 * @param[in] ptrBLEScan Pointer to BLE scan object - if not given, then it will be initialised
	 */
	void init( BLEScan *ptrBLEScan = nullptr );

	/**
	 * @brief Registers new callback called when ADV packet will be received
	 * @param[in] cbk Pointer to callback class
	 */
	void cbkRegister( BleAdvListenerCbk *cbk );

	/**
	 * @brief Method to handle everything needed - should be called in every loop() iteration
	 */
	void process();

	/**
	 * @brief Pauses or unpauses ADV listening
	 * @param[in] paused true for pause, false for unpause
	 */
	void setPaused( bool paused )
	{
		this->paused = paused;
	}

	/**
	 * @brief Returns information if scanning for ADV is already running
	 * @return Returns true if scan is running and false if not
	 */
	bool isScanRunning()
	{
		return scanRunning;
	}
private:
	BLEScan *pBLEScan = nullptr;

	bool     scanRunning = false;

	bool     bleStarted = false;

	bool     paused = false;

	time_t   nextScan = 0;

	std::forward_list<BleAdvListenerCbk *> regCbks; // list of registered callbacks

	/**
	 * @brief Sets scan complete state
	 */
	void setScanComplete();

	friend class MyAdvertisedDeviceCallbacks;
	friend void scanCompleteCbk( BLEScanResults foundDevices );
};

/* ************************************************************************** */

extern BleAdvListener bleAdvListener;

/* ************************************************************************** */
