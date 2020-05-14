#pragma once

#include "Arduino.h"
#include <BLEDevice.h>
#include "BleAdvListener.h"
#include "SensorCommon.h"
#include <forward_list>

/* ************************************************************************** */
/**
 * @brief Class with data from one LYWSD03MMC sensor
 */
class LYWSD03MMCData : public BleAdvListenerCbk
{
private:
	BLEAddress  *address;

	const char *alias;

	const uint8_t *key;

	time_t       advTimestamp = -1; // timestamp of last ADV packet received
	time_t       nextRefresh = 0;   // next planed data refresh
	time_t       cbkWaitTime = 0;
	time_t       nextTempNotify = 0;
	time_t       nextHumidityNotify = 0;
	time_t       nextBatNotify = 0;

	struct SensorValues values;

	std::forward_list<SensorDataChangeCbk *> *regCbks = nullptr; // list with registered callbacks
public:

	LYWSD03MMCData( BLEAddress *address, const char *alias = nullptr, const uint8_t *key = nullptr )
	{
		this->address = address;
		this->alias = alias;
		this->key = key;

		memset( &values, 0, sizeof( struct SensorValues ) );
	}

	/**
	 * @brief Method called when ADV packet is received
	 * @param[in] address Address of advertised device
	 * @param[in] serviceData Service data from ADV packet
	 */
	void onAdvData( BLEAddress *address, std::string &serviceData );

	/**
	 * @brief Decrypts encrypted ADV service data
	 * @param[in] serviceData Received encrypted service data
	 * @param[in] key Key to decrypt data
	 * @param[out] decryptedData Decrypted data (if success)
	 * @return Returns true on success or false on failure
	 */
	bool decryptServiceData( std::string &serviceData, const uint8_t *key, uint8_t decryptedData[16] );

	friend class LYWSD03MMC;
};

/* ************************************************************************** */

/**
 * @brief Base class for working with LYWSD03MMC sensors
 */
class LYWSD03MMC
{
public:

	/**
	 * @brief Initialise class. Call it if you have already initialised bluetooth client.
	 * This method must be called once before any other calls (in setup() function).
	 *
	 * @param[in] client Bluetooth client class
	 * @param[in] refreshTime Time in seconds in which data will be automaticaly refreshed (0 = no automatic refresh)
	 * @param[in] cbkWaitTime Minimum time in seconds between two callback calls for the same sensor value update
	 */
	void init( BLEClient *client, time_t refreshTime = 300, time_t cbkWaitTime = 10 );

	/**
	 * @brief Initialise class. Call it if you don't have initialised bluetooth client. Method will initialise one client for you.
	 * This method must be called once before any other calls (in setup() funcion)
	 *
	 * @param[in] refreshTime Time in seconds in which data will be automaticaly refreshed (0 = no automatic refresh)
	 * @param[in] cbkWaitTime Minimum time in seconds between two callback calls for the same sensor value update
	 */
	void init( time_t refreshTime = 300, time_t cbkWaitTime = 10 );

	/**
	 * @brief Method to handle everything needed - should be called in every loop() iteration
	 */
	void process();

	/**
	 * @brief Registers new LYWSD03MMC devices MAC address that we want to get data from
	 * @param[in] address MAC address of device
	 * @param[in] alias Our device alias
	 * @param[in] key Key for decrypting ADV packets in passive mode
	 */
	void deviceRegister( BLEAddress *address, const char *alias = nullptr, const uint8_t *key = nullptr );

	/**
	 * @brief Forces data refresh of device by alias
	 * @param[in] alias Alias of device we are interested in
	 */
	void forceRefresh( const char *alias );

	/**
	 * @brief Forces data refresh of device by MAC
	 * @param[in] address Address of device we are interested in
	 */
	void forceRefresh( BLEAddress &address );

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
	enum
	{
		ST_NOT_CONNECTED,       // we are not connected do any device
		ST_WAITING_FOR_DATA,    // we are connected to device and we are waiting for notification data
		ST_HAVE_DATA_CONNECTED, // we are connected and we have received notification data
	} state;

	BLEClient *bleClient = nullptr;

	std::forward_list<LYWSD03MMCData *> regDevices; // list with registered devices

	std::forward_list<SensorDataChangeCbk *> regCbks; // list with registered callbacks

	// actual device we are working with (due to library limitation we can be connected only to one device at a time)
	LYWSD03MMCData *actDevice = nullptr;

	time_t connStart;
	time_t connTimeout = 15;
	time_t maxAdvTimeout = 30;
	time_t refreshTime;
	time_t cbkWaitTime;

	/**
	 * @brief Callback called when notification from sensor is received
	 * @param[in] pBLERemoteCharacteristic Registered characteristics
	 * @param[in] data Data received in notification
	 * @param[in] dataLength Length of received data
	 * @param[in] isNotify True when notification was received
	 */
	static void notifyCallback( BLERemoteCharacteristic *pBLERemoteCharacteristic,
			uint8_t *data, size_t dataLength, bool isNotify );

	/**
	 * @brief Registers @notifyCallback for notifications from sensor
	 * @param[in] doRegister true for register, false for unregister
	 * @return Returns 0 on success or <0 if error occured
	 */
	int  registerNotification( bool doRegister = true );

	/**
	 * @brief Sets BLE communication interval to 500ms (for battery save)
	 */
	void setCommunicationInterval();

	/**
	 * @brief Enables receiving of notifications from sensor
	 * @param[in] doEnable true for enable, false for disable
	 * @return Returns 0 on success or <0 if error occured
	 */
	int  enableNotifications( bool doEnable = true );

	/**
	 * @brief Connects to sensor with parameteris in @actDevice
	 */
	void connectSensor();

	/**
	 * @brief Disconnects form already connected sensor
	 */
	void disconnectSensor();

	/**
	 * @brief Sets data to sensor in @actDevice
	 * @param[in] temp Actual temperature
	 * @param[in] humidity Actual humidity
	 * @param[in] voltage Actual voltage
	 */
	void setData( float temp, float humidity, float voltage );

	/**
	 * @brief Returns remote characteristics
	 * @param[in] xcharUUID characteristics UUID
	 * @return Returns remote characteristics or nullptr if error occured
	 */
	BLERemoteCharacteristic *getMainCharacteristic( BLEUUID &xcharUUID );
};

/* ************************************************************************** */

extern LYWSD03MMC lywsd03mmc;

/* ************************************************************************** */
