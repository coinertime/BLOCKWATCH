#ifndef WATCHY_H
#define WATCHY_H

#include <Arduino.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <DS3232RTC.h>
#include <GxEPD2_BW.h>
#include <Wire.h>
#include "FreeMonoBold9pt7b.h"
#include "DSEG7_Classic_Bold_53.h"
#include "Square_1_27pt7b.h" 
#include "moscowbmp.c"
#include "BLE.h"
#include "bma.h"
#include "config.h"    
#include <ctime>
#include <cstdlib>
#include "qrcode.h"
#include <SPIFFS.h>
#include <string.h>
 
typedef struct BlockData{
    int height=696251;
    int elapsed = 0;
    double complete = 0.0;
    long epoch=0;
    bool live=false;
}BlockData;

typedef struct MoscowData{
  int bitPrice = 48503;
  int time = 2061;
  long epoch = 0;
  bool live = false;
  long satoshis = 100000000;
}MoscowData;

class Watchy {
    public:
        static DS3232RTC RTC;
        static GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display;
        tmElements_t currentTime;
    public:
        Watchy();
        void init(String datetime = "");
        void reset();
        void deepSleep();

        void handleButtonPress();
        void showMenu(byte menuIndex, bool partialRefresh);
        void showFastMenu(byte menuIndex);
        void showLoading();
        void setFace(int face);
        void setupQr();
        void setupWifi();
        bool connectWiFi();
        BlockData getBlockData();
        MoscowData getMoscowData();
        float getBatteryVoltage();
        int getBatteryPercentage();

        void showWatchFace(bool partialRefresh);
        virtual void drawWatchFace(); //override this method for different watch faces
        void drawArray(QRCode qr, uint8_t* data);
        void qrFace();
    private:
        void _rtcConfig(String datetime);    
        
        static void _configModeCallback(WiFiManager *myWiFiManager);
        static uint16_t _readRegister(uint8_t address, uint8_t reg, uint8_t *data, uint16_t len);
        static uint16_t _writeRegister(uint8_t address, uint8_t reg, uint8_t *data, uint16_t len);
};

extern RTC_DATA_ATTR int guiState;
extern RTC_DATA_ATTR int menuIndex;
extern RTC_DATA_ATTR int currentFace;
extern RTC_DATA_ATTR int currentBlockHeight;
extern RTC_DATA_ATTR int currentBitPrice;
extern RTC_DATA_ATTR bool WIFI_CONFIGURED;
extern RTC_DATA_ATTR bool BLE_CONFIGURED;
//extern RTC_DATA_ATTR BlockData currentBlockData;

extern char currentWallet [] RTC_RODATA_ATTR;

#endif
