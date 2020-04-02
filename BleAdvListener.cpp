#include "BleAdvListener.h"

#include "Arduino.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "debug.h"

/* ************************************************************************** */

#define SCAN_TIME  6

BleAdvListener bleAdvListener;

/* ************************************************************************** */
/**
 * @brief Class for receiving and forwarding BLE ADV informations
 */
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
		if (!advertisedDevice.haveServiceData())
		{
			return;
		}

		int count = advertisedDevice.getServiceDataCount();
		BLEAddress address = advertisedDevice.getAddress();

		for( auto it = bleAdvListener.regCbks.cbegin(); it != bleAdvListener.regCbks.cend(); it++ )
    	{
    		for (int i = 0; i < count; i++)
    		{
    			std::string serviceData = advertisedDevice.getServiceData(i);

    			(*it)->onAdvData( &address, serviceData );
    		}
    	}
    }
};

/* ************************************************************************** */
/**
 * @brief Initialise BLE ADV listener
 * @param[in] ptrBLEScan Pointer to BLE scan object - if not given, then it will be initialised
 */
void BleAdvListener::init( BLEScan *ptrBLEScan )
{
	pBLEScan = ptrBLEScan ? ptrBLEScan : BLEDevice::getScan();

	pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true );
	pBLEScan->setActiveScan(false);
	pBLEScan->setInterval(400);
	pBLEScan->setWindow(150);
	bleStarted = true;
}

/* ************************************************************************** */
/**
 * @brief Registers new callback called when ADV packet will be received
 * @param[in] cbk Pointer to callback class
 */
void BleAdvListener::cbkRegister( BleAdvListenerCbk *cbk )
{
	regCbks.push_front( cbk );
}

/* ************************************************************************** */
/**
 * @brief Sets scan complete state
 */
void BleAdvListener::setScanComplete()
{
	scanRunning = false;
	nextScan = time( NULL ) + 1;
}

/* ************************************************************************** */
/**
 * @brief Callback for setting scan complete state
 * @param[in] foundDevices Results from scanning
 */
void scanCompleteCbk( BLEScanResults foundDevices )
{
	bleAdvListener.setScanComplete();
}

/* ************************************************************************** */
/**
 * @brief Method to handle everything needed - should be called in every loop() iteration
 */
void BleAdvListener::process()
{
	if( paused == false )
	{
		if( bleStarted == true && scanRunning == false && time(NULL) > nextScan )
		{
			pBLEScan->start(SCAN_TIME, scanCompleteCbk, false);
			scanRunning = true;
		}
	}
}

/* ************************************************************************** */
