

 
#include "ble.h"

const int16_t minPowLev = 0; // power level range settings, no app cares about this
const int16_t maxPowLev = 500;
const int16_t minPowInc = 1;
const int16_t maxResLev = 40;// resistance level range settings, no app cares about this
uint16_t speedOut = 100;
int16_t powerOut = 100;
int16_t powerOut_filter[BLE_POWER_FILTER_SAMPLES];
int16_t powerOut_filter_pos = 0;


#ifndef DISABLE_BLE
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>
#include <BLE2902.h>

#define cyclePowerService BLEUUID((uint16_t)0x1818) // Bicycle Powermeter uuid, as defined in gatt specifications
#define CHARACTERISTIC_UUID BLEUUID((uint16_t)0x2A63) // Cycling Power Measurement
#define batteryService BLEUUID((uint16_t)0x180F)

BLECharacteristic* pPowerMeasurement;
BLECharacteristic* pBatteryLevel;

BLEAdvertisementData advertisementData = BLEAdvertisementData();

#endif

int16_t cadenceIn;
int16_t powerIn;

void ble_Setup(void) {

  #ifndef DISABLE_BLE
    BLEDevice::init(BLE_DEV_NAME);
    
    InitBLEServer();
    #endif

    for(uint8_t iloop = 0; iloop < BLE_POWER_FILTER_SAMPLES; iloop++)
    {
      powerOut_filter[iloop] = 0;
    }
    powerOut_filter_pos = 0;

}

boolean ble_isConnected(void)
{
  return true;
}

/*
 * Publish the instantaneous power measurement.
 */
void ble_PublishPower(int16_t instantPwr, uint16_t cadence, uint32_t crankRevs, long millisLast) {
#ifndef DISABLE_BLE
  speedOut = (uint16_t)crankRevs;
  /* Perform average according config */
  powerOut_filter[powerOut_filter_pos] = instantPwr;
  powerOut_filter_pos = (powerOut_filter_pos + 1) % BLE_POWER_FILTER_SAMPLES;
  powerIn = 0;
  for(uint8_t iloop = 0; iloop < BLE_POWER_FILTER_SAMPLES; iloop++)
  {
    powerIn += powerOut_filter[iloop];
  }
  powerIn = powerIn/BLE_POWER_FILTER_SAMPLES;
  cadenceIn = cadence;
  
  //indoorBikeDataCharacteristicData[4] = (uint8_t)((cadenceIn * 2) & 0xff);        
  //indoorBikeDataCharacteristicData[5] = (uint8_t)((cadenceIn * 2) >> 8);          // cadence value
  uint8_t data[4] = {0}; // Declare and initialize the data buffer
  data[2] = (uint8_t)(constrain(powerIn, 0, 4000) & 0xff);
  data[3] = (uint8_t)(constrain(powerIn, 0, 4000) >> 8);  // power value, constrained to avoid negative values, although the specification allows for a sint16

  pPowerMeasurement->setValue(data, 4);
  pPowerMeasurement->notify();

  #endif
}
/*
 * Publish the battery status measurement.
 */
void ble_PublishBatt(uint8_t battPercent) {
  #ifndef DISABLE_BLE
    pBatteryLevel->setValue(&battPercent, 1);
    pBatteryLevel->notify();
  #endif
  #ifdef DEBUG
    Serial.printf("Battery level sent via BLE: %d %%\n", battPercent);
  #endif
}

#ifndef DISABLE_BLE
void InitBLEServer() {
  #ifdef DEBUG_SETUP
    Serial.println("Start BLE Server");
  #endif
  
  BLEDevice::init(BLE_DEV_NAME);
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(cyclePowerService);
  pPowerMeasurement = pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY);

  pPowerMeasurement->addDescriptor(new BLE2902());
  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(cyclePowerService);
  pAdvertising->addServiceUUID(batteryService);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  // Battery Service
  BLEService* pBatteryService = pServer->createService(BLEUUID((uint16_t)0x180F));
  pBatteryLevel = pBatteryService->createCharacteristic(BLEUUID((uint16_t)0x2A19), BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pBatteryLevel->addDescriptor(new BLE2902());
  uint8_t initialBattery = 100;
  pBatteryLevel->setValue(&initialBattery, 1);  // start value: 100%
  pBatteryService->start();

  BLEDevice::startAdvertising();
  #ifdef DEBUG_SETUP
    Serial.println("BLE Power Meter ready to connect");
  #endif
}
#endif
