#include "LYWSD03MMC.h"
#include "debug.h"
#include "mbedtls/ccm.h"

/* ************************************************************************** */

// The remote service we wish to connect to.
static BLEUUID serviceUUID("ebe0ccb0-7a0a-4b0c-8a1a-6ff2997da3a6");

// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("ebe0ccc1-7a0a-4b0c-8a1a-6ff2997da3a6");

// Characteristic to set communication interval
static BLEUUID charUUID_SetIntervalComm("ebe0ccd8-7a0a-4b0c-8a1a-6ff2997da3a6");

LYWSD03MMC lywsd03mmc;

/* ************************************************************************** */
/**
 * @brief Dummy client callbacks - does nothing but is needed for notifications to work
 */
class DummyClientCallback : public BLEClientCallbacks
{
	void onConnect( BLEClient* pclient )
	{
	}

	void onDisconnect( BLEClient* pclient )
	{
	}
};

/* ************************************************************************** */
/**
 * @brief Decrypts encrypted ADV service data
 * @param[in] serviceData Received encrypted service data
 * @param[in] key Key to decrypt data
 * @param[out] decryptedData Decrypted data (if success)
 * @return Returns true on success or false on failure
 */
bool LYWSD03MMCData::decryptServiceData( std::string &serviceData, const uint8_t *key, uint8_t decryptedData[16] )
{
	const uint8_t *v = (const uint8_t *) serviceData.c_str();

	if( !(v[0] & 0x08) )
	{
		SERIAL_PRINTF("Payload of size %u is not encrypted\n", serviceData.length() );

		uint8_t len = (uint8_t) serviceData.length();
		serviceData.copy( (char *) decryptedData, (len - 11) > 16 ? 16 : (len - 11), 11 );

		return true;
	}

	if( key == nullptr )
	{
		return false;
	}

	if( serviceData.length() < 22 && serviceData.length() > 23 )
	{
		SERIAL_PRINTF("Payload size %u is not supported for decryption\n", serviceData.length() );
		return false;
	}

	const unsigned char authData = 0x11;
	int     offset = 0;
	size_t  datasize = 4;
	uint8_t iv[16];

	if( serviceData.length() == 23 )
	{
		datasize = 5;  // temperature or humidity
		offset = 1;
	}

	memcpy( iv, v + 5, 6);                // MAC address reversed
	memcpy( iv + 6, v + 2, 3);            // sensor type (2) + packet id (1)
	memcpy( iv + 9, v + 15 + offset, 3);  // payload counter

	mbedtls_ccm_context ctx;
	mbedtls_ccm_init(&ctx);

	if( mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 16 * 8 ) != 0 )
	{
		mbedtls_ccm_free(&ctx);
		return false;
	}

	if( mbedtls_ccm_auth_decrypt( &ctx, datasize, iv, 12, &authData, 1,
								 v + 11, decryptedData, v + 18 + offset, 4 ) != 0 )
	{
		mbedtls_ccm_free(&ctx);
		return false;
	}

	mbedtls_ccm_free(&ctx);
	return true;
}

/* ************************************************************************** */
/**
 * @brief Method called when ADV packet is received
 * @param[in] address Address of advertised device
 * @param[in] serviceDataUUID UUID of advertised service data
 * @param[in] serviceData Service data from ADV packet
 */
void LYWSD03MMCData::onAdvData( BLEAddress *address, uint16_t serviceDataUUID, std::string &serviceData )
{
	if( this->address->equals( *address ) == false )
	{
		return;
	}

	SERIAL_PRINTF("Found device: %s alias: %s\n", address->toString().c_str(), alias );

	bool tempNew = false;
	bool humidityNew = false;
	bool batNew = false;

	advTimestamp = time( NULL );

	if( serviceDataUUID == 0xFE95 )
	{
		uint8_t serviceDataPerfix[4];
		serviceData.copy( (char*) serviceDataPerfix, 4, 0 );

		/* check for data prefix (0x58 == encrypted, 0x50 == not encrypted) */
		if( (serviceDataPerfix[0] != 0x50 || serviceDataPerfix[1] != 0x30) &&
				(serviceDataPerfix[0] != 0x58 || serviceDataPerfix[1] != 0x58) )
		{
			SERIAL_PRINTF("Frame control data 0x%02X 0x%02X doesn't match expected values\n", serviceDataPerfix[0], serviceDataPerfix[1] );
			return;
		}

		if( serviceDataPerfix[2] != 0x5B || serviceDataPerfix[3] != 0x05 )
		{
			SERIAL_PRINTF("Device type 0x%02X 0x%02Xxdoesn't match expectet value for LYWSD03MMC sensor\n", serviceDataPerfix[2], serviceDataPerfix[3] );
	//		return; // Frame control is ok, so maybe other type of sensor with the same data format
		}

		uint8_t tempData[16];

		if( decryptServiceData( serviceData, this->key, tempData ) == false )
		{
			SERIAL_PRINTF("Failed to decrypt service data from device %s\n", address->toString().c_str() );
			return;
		}


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

				// emulate voltage -> 3.1V = 100%, 2.1V = 0%
				values.voltage = 2.1 + (values.bat / 100.0);
				values.batTimestamp = advTimestamp;
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
	}
	else
	{
		SERIAL_PRINTF("Received service data with not interested UUID %u\n", serviceDataUUID );
		return;
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
 * @brief Initialise class. Call it if you don't have initialised bluetooth client. Method will initialise one client for you.
 * This method must be called once before any other calls (in setup() funcion)
 *
 * @param[in] refreshTime Time in seconds in which data will be automaticaly refreshed (0 = no automatic refresh)
 * @param[in] cbkWaitTime Minimum time in seconds between two callback calls for the same sensor value update
 */
void LYWSD03MMC::init( time_t refreshTime, time_t cbkWaitTime )
{
	BLEClient *pClient = BLEDevice::createClient();
	pClient->setClientCallbacks( new DummyClientCallback() );

	init( pClient, refreshTime, cbkWaitTime );
}

/* ************************************************************************** */
/**
 * @brief Initialise class. Call it if you have already initialised bluetooth client.
 * This method must be called once before any other calls (in setup() function).
 *
 * @param[in] client Bluetooth client class
 * @param[in] refreshTime Time in seconds in which data will be automaticaly refreshed (0 = no automatic refresh)
 * @param[in] cbkWaitTime Minimum time in seconds between two callback calls for the same sensor value update
 */
void LYWSD03MMC::init( BLEClient *client, time_t refreshTime, time_t cbkWaitTime )
{
	bleClient = client;
	this->refreshTime = refreshTime;
	this->cbkWaitTime = cbkWaitTime;
}

/* ************************************************************************** */
/**
 * @brief Sets data to sensor in @actDevice
 * @param[in] temp Actual temperature
 * @param[in] humidity Actual humidity
 * @param[in] voltage Actual voltage
 */
void LYWSD03MMC::setData( float temp, float humidity, float voltage )
{
	if( actDevice )
	{
		SERIAL_PRINTF("Received data for %s: temp = %.1f : humidity = %.0f : voltage = %f\n",
				actDevice->alias, temp, humidity, voltage );

		actDevice->values.tempTimestamp = actDevice->values.humidityTimestamp = actDevice->values.batTimestamp= time( NULL );
		actDevice->values.temp = temp;
		actDevice->values.humidity = humidity;
		actDevice->values.voltage = voltage;

		// emulate battery percentage -> 3.1V = 100%, 2.1V = 0%
		if( voltage > 3.1 )
		{
			voltage = 3.1;
		}
		actDevice->values.bat = 100.0 - ((3.1 - voltage) * 100.0);

		if( actDevice->values.bat < 0.0 )
		{
			actDevice->values.bat = 0.0;
		}

		for( auto it = regCbks.cbegin(); it != regCbks.cend(); it++ )
    	{
   			(*it)->onData( actDevice->address, actDevice->alias, true, true, true );
    	}
	}
}

/* ************************************************************************** */
/**
 * @brief Callback called when notification from sensor is received
 * @param[in] pBLERemoteCharacteristic Registered characteristics
 * @param[in] data Data received in notification
 * @param[in] dataLength Length of received data
 * @param[in] isNotify True when notification was received
 */
void LYWSD03MMC::notifyCallback( BLERemoteCharacteristic* pBLERemoteCharacteristic,
		uint8_t *data, size_t dataLength, bool isNotify )
{
	float   temp;
	float   voltage;
	float   humidity;

	temp = (data[0] | (data[1] << 8)) * 0.01; //little endian
	humidity = (float) data[2];
	voltage = (data[3] | (data[4] << 8)) * 0.001; //little endian

	lywsd03mmc.setData( temp, humidity, voltage );
	lywsd03mmc.state = ST_HAVE_DATA_CONNECTED;
}

/* ************************************************************************** */
/**
 * @brief Returns remote characteristics
 * @param[in] xcharUUID characteristics UUID
 * @return Returns remote characteristics or nullptr if error occured
 */
BLERemoteCharacteristic *LYWSD03MMC::getMainCharacteristic( BLEUUID &xcharUUID )
{
	BLERemoteService        *remService;
	BLERemoteCharacteristic *remCharacteristic;

	if( bleClient->isConnected() == false )
	{
		return nullptr;
	}

	// Obtain a reference to the service we are after in the remote BLE server.
	if( (remService = bleClient->getService(serviceUUID) ) == nullptr )
	{
		SERIAL_PRINTLN("Failed to get service");
		return nullptr;
	}

	// Obtain a reference to the characteristic in the service of the remote BLE server.
	if( (remCharacteristic = remService->getCharacteristic(xcharUUID)) == nullptr )
	{
		SERIAL_PRINTLN("Failed to get characteristic");
		return nullptr;
	}

	return remCharacteristic;
}

/* ************************************************************************** */
/**
 * @brief Returns remote characteristics
 * @param[in] xcharUUID characteristics UUID
 * @return Returns remote characteristics or nullptr if error occured
 */
int LYWSD03MMC::registerNotification( bool doRegister )
{
	BLERemoteCharacteristic *remCharacteristic;

	if( (remCharacteristic = getMainCharacteristic( charUUID )) == nullptr )
	{
		SERIAL_PRINTLN("Failed to find remote characteristic UUID");
		return -1;
	}

	remCharacteristic->registerForNotify( doRegister ? notifyCallback : nullptr );

	return 0;
}

/* ************************************************************************** */
/**
 * @brief Sets BLE communication interval to 500ms (for battery save)
 */
void LYWSD03MMC::setCommunicationInterval()
{
	// Comunicacion interval = 0x01F4 = 500ms
	uint8_t setCommInterval[] = { 0xf4, 0x01 };
	BLERemoteCharacteristic *remCharacteristic;

	if( (remCharacteristic = getMainCharacteristic( charUUID_SetIntervalComm )) == nullptr )
	{
		SERIAL_PRINTLN("Failed to find remote characteristic UUID_SetIntervalComm");
		return;
	}

	// Write the value of the characteristic.
	remCharacteristic->writeValue( setCommInterval, 2, true );
}

/* ************************************************************************** */
/**
 * @brief Enables receiving of notifications from sensor
 * @param[in] doEnable true for enable, false for disable
 * @return Returns 0 on success or <0 if error occured
 */
int LYWSD03MMC::enableNotifications( bool doEnable )
{
	BLERemoteCharacteristic *remCharacteristic;
	BLERemoteDescriptor     *remDescriptor;

	uint8_t notificationOn[]  = {0x1, 0x0};
	uint8_t notificationOff[] = {0x0, 0x0};

	if( (remCharacteristic = getMainCharacteristic( charUUID )) == nullptr )
	{
		SERIAL_PRINTLN("Failed to find remote characteristic UUID");
		return -1;
	}

	if( (remDescriptor = remCharacteristic->getDescriptor( BLEUUID((uint16_t) 0x2902) )) == nullptr )
	{
		return -2;
	}

	remDescriptor->writeValue( doEnable ? notificationOn : notificationOff, 2, true );

	return 0;
}

/* ************************************************************************** */
/**
 * @brief Connects to sensor with parameteris in @actDevice
 */
void LYWSD03MMC::connectSensor()
{
	if( bleClient->connect( *actDevice->address ) == true )
	{
//		setCommunicationInterval();
		registerNotification();
		enableNotifications();
	}
}

/* ************************************************************************** */
/**
 * @brief Disconnects form already connected sensor
 */
void LYWSD03MMC::disconnectSensor()
{
	enableNotifications( false );
	registerNotification( false );
	bleClient->disconnect();
}

/* ************************************************************************** */
/**
 * @brief Method to handle everything needed - should be called in every loop() iteration
 */
void LYWSD03MMC::process()
{
	if( bleAdvListener.isScanRunning() == true )
	{
		return;
	}

	time_t actTime = time( NULL );

	switch( state )
	{
		case ST_NOT_CONNECTED :
		{
		}
		break;

		case ST_WAITING_FOR_DATA :
		{
			if( (actTime - connStart) > connTimeout )
			{
				disconnectSensor();
				state = ST_NOT_CONNECTED;
				bleAdvListener.setPaused( false );

				SERIAL_PRINTF("Disconnected from sensor %s due timeout\n", actDevice->alias );
				return;
			}
		}
		break;

		case ST_HAVE_DATA_CONNECTED :
		{
			disconnectSensor();
			state = ST_NOT_CONNECTED;
			bleAdvListener.setPaused( false );

			SERIAL_PRINTF("Disconnected from sensor %s after data received\n", actDevice->alias );
			return;
		}
		break;
	}

	if( state == ST_NOT_CONNECTED )
	{
		LYWSD03MMCData *actDevice = nullptr;

		/* find device that needs data refresh */
		for( auto it = regDevices.cbegin(); it != regDevices.cend(); it++ )
		{
			if( (*it)->nextRefresh && (*it)->nextRefresh < actTime )
			{
				/* check the time of last ADV packet to see, if the device is "near" */
				if( (*it)->advTimestamp >= 0 && (actTime - (*it)->advTimestamp) < maxAdvTimeout )
				{
					actDevice = *it;
					break;
				}
			}
		}

		if( actDevice != nullptr )
		{
			SERIAL_PRINTF("Connecting and requesting data from sensor %s ...\n", actDevice->alias );

			this->actDevice = actDevice;
			connStart = actTime;

			if( refreshTime )
			{
				actDevice->nextRefresh = actTime + refreshTime;
			}
			else
			{
				actDevice->nextRefresh = 0;
			}

			bleAdvListener.setPaused( true );
			connectSensor();
			state = ST_WAITING_FOR_DATA;
		}
	}
}

/* ************************************************************************** */
/**
 * @brief Registers new LYWSD03MMC devices MAC address that we want to get data from
 * @param[in] address MAC address of device
 * @param[in] alias Our device alias
 * @param[in] key Key for decrypting ADV packets in passive mode
 */
void LYWSD03MMC::deviceRegister( BLEAddress *address, const char *alias, const uint8_t *key )
{
	LYWSD03MMCData *data = new LYWSD03MMCData( address, alias, key );

	if( refreshTime )
	{
		data->nextRefresh = 1 + std::distance( regDevices.cbegin(), regDevices.cend() ) * (connTimeout * 2);
	}

	data->cbkWaitTime = cbkWaitTime;
	data->regCbks = &regCbks;
	bleAdvListener.cbkRegister( data );

	regDevices.push_front( data );
}

/* ************************************************************************** */
/**
 * @brief Forces data refresh of device by alias
 * @param[in] alias Alias of device we are interested in
 */
void LYWSD03MMC::forceRefresh( const char *alias )
{
	for( auto it = regDevices.cbegin(); it != regDevices.cend(); it++ )
	{
		if( (*it)->alias && strcmp( (*it)->alias, alias ) == 0 )
		{
			(*it)->nextRefresh = time( NULL );
			break;
		}
	}
}

/* ************************************************************************** */
/**
 * @brief Forces data refresh of device by MAC
 * @param[in] address Address of device we are interested in
 */
void LYWSD03MMC::forceRefresh( BLEAddress &address )
{
	for( auto it = regDevices.cbegin(); it != regDevices.cend(); it++ )
	{
		if( (*it)->address->equals( address ) == true )
		{
			(*it)->nextRefresh = time( NULL );
			break;
		}
	}
}

/* ************************************************************************** */
/**
 * @brief Gets device data by alias
 * @param[in] alias Alias of device we are interested in
 * @param[out] values Values for sensor with given alias
 * @return Returns true if device with entered alias was found (registered)
 */
bool LYWSD03MMC::getData( const char *alias, struct SensorValues *values )
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
bool LYWSD03MMC::getData( BLEAddress &address, struct SensorValues *values )
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
void LYWSD03MMC::cbkRegister( SensorDataChangeCbk *cbk )
{
	regCbks.push_front( cbk );
}

/* ************************************************************************** */
