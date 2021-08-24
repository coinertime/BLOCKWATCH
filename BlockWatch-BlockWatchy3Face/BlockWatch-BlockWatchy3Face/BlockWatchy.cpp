#include "BlockWatchy.h"

DS3232RTC Watchy::RTC(false); 
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> Watchy::display(GxEPD2_154_D67(CS, DC, RESET, BUSY));


RTC_DATA_ATTR int guiState;
RTC_DATA_ATTR int menuIndex;
RTC_DATA_ATTR int currentFace=0;
RTC_DATA_ATTR bool WIFI_CONFIGURED;
RTC_DATA_ATTR bool BLE_CONFIGURED;
RTC_DATA_ATTR int currentBlockHeight;
RTC_DATA_ATTR long currentBlockEpoch;
RTC_DATA_ATTR int currentBitPrice;

//RTC_DATA_ATTR BlockData currentBlockData;
//RTC_DATA_ATTR MoscowData currentMoscowData;
//RTC_RODATA_ATTR char [] currentWallet;

const char currentWallet [] RTC_RODATA_ATTR= { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };

namespace TimeUtils {
  static unsigned long getTime() {
    return std::time(0);
  }
}

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

Watchy::Watchy(){} //constructor

void Watchy::init(String datetime){
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause(); //get wake up reason
    Wire.begin(SDA, SCL); //init i2c
          
     Serial.print("Current Block Height:");
     Serial.println(currentBlockHeight);
      Serial.print("Current Block Epoch:");
     Serial.println(currentBlockEpoch);

     if(currentWallet!=null){
      Serial.println("Have Wallet:" + String(currentWallet));
     }  


    switch (wakeup_reason)
    {
        #ifdef ESP_RTC
        case ESP_SLEEP_WAKEUP_TIMER: //ESP Internal RTC
            if(guiState == WATCHFACE_STATE){
                RTC.read(currentTime); 
                currentTime.Minute++;
                tmElements_t tm;
                tm.Month = currentTime.Month;
                tm.Day = currentTime.Day;
                tm.Year = currentTime.Year;
                tm.Hour = currentTime.Hour;
                tm.Minute = currentTime.Minute;
                tm.Second = 0;
                time_t t = makeTime(tm);
                RTC.set(t);
                RTC.read(currentTime);           
                showWatchFace(true); //partial updates on tick
            }
            break;        
        #endif
        case ESP_SLEEP_WAKEUP_EXT0: //RTC Alarm
            Serial.println("WakeTimer");
            RTC.alarm(ALARM_2); //resets the alarm flag in the RTC
            if(guiState == WATCHFACE_STATE){
                RTC.read(currentTime);
                showWatchFace(true); //partial updates on tick
            }
            break;
        case ESP_SLEEP_WAKEUP_EXT1: //button Press
            Serial.println("ButtonPress");
            handleButtonPress();
            break;
        default: //reset
            #ifndef ESP_RTC
            _rtcConfig(datetime);
            #endif
            //_bmaConfig();
            showWatchFace(false); //full update on reset
            break;
    }
    deepSleep();
}

void Watchy::deepSleep(){
  #ifndef ESP_RTC
  esp_sleep_enable_ext0_wakeup(RTC_PIN, 0); //enable deep sleep wake on RTC interrupt
  #endif  
  #ifdef ESP_RTC
  esp_sleep_enable_timer_wakeup(60000000);
  #endif 
  esp_sleep_enable_ext1_wakeup(BTN_PIN_MASK, ESP_EXT1_WAKEUP_ANY_HIGH); //enable deep sleep wake on button press
  esp_deep_sleep_start();
}

void Watchy::_rtcConfig(String datetime){
    if(datetime != NULL){
        const time_t FUDGE(30);//fudge factor to allow for upload time, etc. (seconds, YMMV)
        tmElements_t tm;
        tm.Year = getValue(datetime, ':', 0).toInt() - YEAR_OFFSET;//offset from 1970, since year is stored in uint8_t        
        tm.Month = getValue(datetime, ':', 1).toInt();
        tm.Day = getValue(datetime, ':', 2).toInt();
        tm.Hour = getValue(datetime, ':', 3).toInt();
        tm.Minute = getValue(datetime, ':', 4).toInt();
        tm.Second = getValue(datetime, ':', 5).toInt();

        time_t t = makeTime(tm) + FUDGE;
        RTC.set(t);

    }
    //https://github.com/JChristensen/DS3232RTC
    RTC.squareWave(SQWAVE_NONE); //disable square wave output
    //RTC.set(compileTime()); //set RTC time to compile time
    RTC.setAlarm(ALM2_EVERY_MINUTE, 0, 0, 0, 0); //alarm wakes up Watchy every minute
    RTC.alarmInterrupt(ALARM_2, true); //enable alarm interrupt
    RTC.read(currentTime);
}

void Watchy::setFace(int face){
    currentFace = face;
    guiState = WATCHFACE_STATE;
    showWatchFace(false);
}

void Watchy::handleButtonPress(){
  uint64_t wakeupBit = esp_sleep_get_ext1_wakeup_status();
  //Menu Button
  if (wakeupBit & MENU_BTN_MASK){
      Serial.println("Back Button");
    if(guiState == WATCHFACE_STATE){//enter menu state if coming from watch face
      showMenu(menuIndex, false);
    }else if(guiState == MAIN_MENU_STATE){//if already in menu, then select menu item
      switch(menuIndex)
      {
        case 0:
         Serial.println("Set Face 0");
          setFace(0);
          break;
        case 1:
         Serial.println("Set Face 1");
          setFace(1);
          break;          
        case 2:
         Serial.println("Set Face 2");
          setFace(2);
          break;
        case 3:
          setupQr();
          break;
        case 4:
          setupWifi();
          break;                    
        case 5:
          showUpdateFW();
          break;
        default:
          break;                              
      }
    }else if(guiState == FW_UPDATE_STATE){
      updateFWBegin();
    }
  }
  //Back Button
  else if (wakeupBit & BACK_BTN_MASK){
      Serial.println("Back Button");
    if(guiState == MAIN_MENU_STATE){//exit to watch face if already in menu
      RTC.alarm(ALARM_2); //resets the alarm flag in the RTC
      RTC.read(currentTime);
      showWatchFace(false);
    }else if(guiState == APP_STATE){
      showMenu(menuIndex, false);//exit to menu if already in app
    }else if(guiState == FW_UPDATE_STATE){
      showMenu(menuIndex, false);//exit to menu if already in app
    }
  }
  //Up Button
  else if (wakeupBit & UP_BTN_MASK){
    if(guiState == MAIN_MENU_STATE){//increment menu index
      menuIndex--;
      if(menuIndex < 0){
        menuIndex = MENU_LENGTH - 1;
      }    
      showMenu(menuIndex, true);
    }
  }
  //Down Button
  else if (wakeupBit & DOWN_BTN_MASK){
    if(guiState == MAIN_MENU_STATE){//decrement menu index
      menuIndex++;
      if(menuIndex > MENU_LENGTH - 1){
        menuIndex = 0;
      }
      showMenu(menuIndex, true);
    }
  }
  
  /***************** fast menu *****************/
  bool timeout = false;
  long lastTimeout = millis();
  pinMode(MENU_BTN_PIN, INPUT);
  pinMode(BACK_BTN_PIN, INPUT);
  pinMode(UP_BTN_PIN, INPUT);
  pinMode(DOWN_BTN_PIN, INPUT);
  while(!timeout){
      if(millis() - lastTimeout > 5000){
          timeout = true;
      }else{
          if(digitalRead(MENU_BTN_PIN) == 1){
            lastTimeout = millis();  
            if(guiState == MAIN_MENU_STATE){//if already in menu, then select menu item
                switch(menuIndex)
                {
                    case 0:
                    setFace(0);
                    break;
                    case 1:
                    setFace(1);
                    break;          
                    case 2:
                    setFace(2);
                    break;
                    case 3:
                    setupQr();
                    break;
                    case 4:
                    setupWifi();
                    break;                    
                    case 5:
                    showUpdateFW();
                    break;
                    default:
                    break;                              
                }
            }else if(guiState == FW_UPDATE_STATE){
                updateFWBegin();
            }
          }else if(digitalRead(BACK_BTN_PIN) == 1){
            lastTimeout = millis();
            if(guiState == MAIN_MENU_STATE){//exit to watch face if already in menu
            RTC.alarm(ALARM_2); //resets the alarm flag in the RTC
            RTC.read(currentTime);
            showWatchFace(false);
            break; //leave loop
            }else if(guiState == APP_STATE){
            showMenu(menuIndex, false);//exit to menu if already in app
            }else if(guiState == FW_UPDATE_STATE){
            showMenu(menuIndex, false);//exit to menu if already in app
            }            
          }else if(digitalRead(UP_BTN_PIN) == 1){
            lastTimeout = millis();
            if(guiState == MAIN_MENU_STATE){//increment menu index
            menuIndex--;
            if(menuIndex < 0){
                menuIndex = MENU_LENGTH - 1;
            }    
            showFastMenu(menuIndex);
            }            
          }else if(digitalRead(DOWN_BTN_PIN) == 1){
            lastTimeout = millis();
            if(guiState == MAIN_MENU_STATE){//decrement menu index
            menuIndex++;
            if(menuIndex > MENU_LENGTH - 1){
                menuIndex = 0;
            }
            showFastMenu(menuIndex);
            }         
          }
      }
  }
  display.hibernate();    
}

void Watchy::showMenu(byte menuIndex, bool partialRefresh){
    display.init(0, false); //_initial_refresh to false to prevent full update on init
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);

    int16_t  x1, y1;
    uint16_t w, h;
    int16_t yPos;

    const char *menuItems[] = {"Block Face", "Moscow Face", "QR Face", "Setup QR", "Setup WiFi", "Update Firmware"};
    for(int i=0; i<MENU_LENGTH; i++){
        yPos = 30+(MENU_HEIGHT*i);
        display.setCursor(0, yPos);
        if(i == menuIndex){
            display.getTextBounds(menuItems[i], 0, yPos, &x1, &y1, &w, &h);
            display.fillRect(x1-1, y1-10, 200, h+15, GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
            display.println(menuItems[i]);      
        }else{
            display.setTextColor(GxEPD_WHITE);
            display.println(menuItems[i]);
        }   
    }

    display.display(partialRefresh);
    //display.hibernate();

    guiState = MAIN_MENU_STATE;    
}

void Watchy::showFastMenu(byte menuIndex){
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);

    int16_t  x1, y1;
    uint16_t w, h;
    int16_t yPos;

    const char *menuItems[] = {"Block Face", "Moscow Face", "QR Face", "Setup QR", "Setup WiFi", "Update Firmware"};
    for(int i=0; i<MENU_LENGTH; i++){
        yPos = 30+(MENU_HEIGHT*i);
        display.setCursor(0, yPos);
        if(i == menuIndex){
            display.getTextBounds(menuItems[i], 0, yPos, &x1, &y1, &w, &h);
            display.fillRect(x1-1, y1-10, 200, h+15, GxEPD_WHITE);
            display.setTextColor(GxEPD_BLACK);
            display.println(menuItems[i]);      
        }else{
            display.setTextColor(GxEPD_WHITE);
            display.println(menuItems[i]);
        }   
    }

    display.display(true);

    guiState = MAIN_MENU_STATE;    
}

void Watchy::showWatchFace(bool partialRefresh){
  display.init(0, false); //_initial_refresh to false to prevent full update on init
  display.setFullWindow();
  drawWatchFace();
  display.display(partialRefresh); //partial refresh
  display.hibernate();
  guiState = WATCHFACE_STATE;
}

void Watchy::drawWatchFace(){
    Serial.println("Draw Call:"+static_cast<String>(currentFace));
         

          //get current watch face
         
          switch(currentFace){
            case 0: //Blockheight
              {
                display.setFont(&Square_1_27pt7b);
                BlockData bd; 
                bd = getBlockData();
                if(bd.live==true){
                  display.setTextColor(GxEPD_WHITE);
                  display.fillScreen(GxEPD_BLACK);
                }else{
                  display.setTextColor(GxEPD_BLACK);
                  display.fillScreen(GxEPD_WHITE);
                }
                 int16_t yPos=120;
                
                int16_t  x1, y1;
                uint16_t w, h;
               
                display.getTextBounds(static_cast<String>(bd.height), 0, yPos, &x1, &y1, &w, &h);
                int xPos = ((200-(w+(x1*2)))/2);
                Serial.print("x1:");
                Serial.print(x1);
                Serial.print(" Y1:");
                Serial.print(y1);
                Serial.print(" W:");
                Serial.print(w);
                Serial.print(" H:");
                Serial.print(h);
                Serial.print(" xPos:");
                Serial.println(xPos);
                display.setCursor(xPos, yPos);
                display.print(bd.height);
                
                int d_b=10;
                int d_h = 175;
                int s_w = 4;
                int b_d = (200 / d_b)-s_w;
                int s_x = s_w / 2;
                int complete = (bd.complete * 10);

                for(int i = 0;i<d_b;i++){
                  if(i>=complete){
                    display.drawRect(s_x, d_h, b_d, b_d, GxEPD_WHITE);
                  }else{
                    display.fillRect(s_x, d_h, b_d, b_d, GxEPD_WHITE);
                  }
                  s_x+=(s_w+b_d);
                }

              } 
               break;
           case 1:  //Moscow
            {
              display.setFont(&Square_1_27pt7b);
              MoscowData md; 
              md = getMoscowData();
              
              display.fillScreen(GxEPD_BLACK);
              display.setTextColor(GxEPD_WHITE);
              
              //display.drawRect(18, 0, MOSCOW_BMP_WIDTH,MOSCOW_BMP_HEIGHT, GxEPD_WHITE);
              display.drawBitmap(0, 0, MOSCOW_BMP_DATA, MOSCOW_BMP_WIDTH, MOSCOW_BMP_HEIGHT, GxEPD_WHITE);
              // if(md.live==true){
              //   display.setTextColor(GxEPD_WHITE);
              //   display.fillScreen(GxEPD_BLACK);
              // }else{
              //   display.setTextColor(GxEPD_BLACK);
              //   display.fillScreen(GxEPD_WHITE);
              // }
                          
              String st = static_cast<String>(md.time);
              st = (st.substring(0, 2) + ":" + st.substring(2, 4));
              int16_t yPos = 190;
              int16_t  x1, y1;
              uint16_t w, h;
               
              display.getTextBounds(st, 0, yPos, &x1, &y1, &w, &h);
              int xPos = ((200-(w+(x1*2)))/2);
              display.setCursor(xPos, yPos);
 
              display.print(st.c_str());
            }
              break;
            case 2:
            {
              qrCard();
            }
              break;
          }
          
          Serial.println("End Draw Seq");
}

void Watchy::qrCard(){
  Serial.print("Draw QR:");
  if(currentWallet!=null){
    Serial.println(String(currentWallet));


      display.fillScreen(GxEPD_WHITE);
      //display.fillRect(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, GxEPD_WHITE);

      
      // Start time
      uint32_t dt = millis();
      
      //display.updateWindow(0, 0, GxEPD_WIDTH, GxEPD_HEIGHT, false);
      
      // Create the QR code
      QRCode qrcode;
      uint8_t qrcodeData[qrcode_getBufferSize(4)];
      //char* qrChars = const_cast<char*>(currentWallet.c_str());
      qrcode_initText(&qrcode, qrcodeData, 4, 0, currentWallet);
      
      // Delta time
      dt = millis() - dt;
      Serial.print("QR Code Generation Time: ");
      Serial.print(dt);
      Serial.print(" Size:");
      Serial.print(qrcode.size);
      Serial.print("\n");
      
      display.drawImage(qrcodeData, 0, 0, 200, 200, false, false, false);

  // Top quiet zone
      Serial.println("Drew QR");
    }else{
      Serial.println(" wallet null");
  }
}

BlockData Watchy::getBlockData(){
    BlockData blockData;
        if(connectWiFi())
        {       
          Serial.println("Wifi Connected");
            HTTPClient http;
            http.setConnectTimeout(3000);//3 second max timeout
            String queryURL = String("http://api.blockcypher.com/v1/btc/main");
            http.begin(queryURL.c_str());
            int httpResponseCode = http.GET();
            if(httpResponseCode == 200) 
            {
              String payload = http.getString();
              JSONVar responseObject = JSON.parse(payload);
              int jheight = int(responseObject["height"]);
          
              //check if we need to save
              Serial.println("Json Height:" + static_cast<String>(jheight));
              
           
              if(!std::isnan(jheight) && jheight != currentBlockHeight){
                currentBlockHeight = jheight;
                currentBlockEpoch = TimeUtils::getTime();
                
              }
              
              blockData.live=true;
            }else{
              blockData.live=false;
            }
            
            http.end();
            
            //Shut off WifiRadio
            WiFi.mode(WIFI_OFF);
            btStop();
        }else{
            blockData.live=false;
        }
        if(currentBlockHeight<1){
          currentBlockHeight = blockData.height;
        }else{
          blockData.height = currentBlockHeight;
        }
        if(currentBlockEpoch<1){
          currentBlockEpoch = TimeUtils::getTime();
        }else{
          blockData.epoch = currentBlockEpoch;
        }
        
        Serial.println("BlockPoch:" + static_cast<String>(blockData.epoch));
        Serial.println("Epoch:" + static_cast<String>(TimeUtils::getTime()));
        
        blockData.elapsed = TimeUtils::getTime() - blockData.epoch;
        if(blockData.elapsed<0){
          blockData.epoch = TimeUtils::getTime();
          blockData.elapsed = 0;
        }

     
        if(blockData.live==false && blockData.elapsed > 600){
             //were disconnected, calculate height if we have elapsed a block
          while(blockData.elapsed >= 600){
            blockData.height++;
            blockData.elapsed -= 600;
            currentBlockHeight =  blockData.height;
            currentBlockEpoch+=600;
          }
        }
        
        blockData.complete =(double)blockData.elapsed / (double)600;
        
        return blockData;
}

MoscowData Watchy::getMoscowData(){
  MoscowData moscowData;
  if(connectWiFi())
  {       
    Serial.println("Wifi Connected");
      HTTPClient http;
      http.setConnectTimeout(3000);//3 second max timeout
      String queryURL = String("https://api.coindesk.com/v1/bpi/currentprice.json");
      http.begin(queryURL.c_str());
      int httpResponseCode = http.GET();
      if(httpResponseCode == 200) 
      {
        String payload = http.getString();
        JSONVar responseObject = JSON.parse(payload);
        int jPrice = int(responseObject["bpi"]["USD"]["rate_float"]);
    
        //check if we need to save
        Serial.println("Json Price:" + static_cast<String>(jPrice));
      
        if(!std::isnan(jPrice) && jPrice != currentBitPrice){
          currentBitPrice = jPrice;          
        }
        
        moscowData.live=true;
      }else{
        moscowData.live=false;
      }
      
      http.end();
      
      //Shut off WifiRadio
      WiFi.mode(WIFI_OFF);
      btStop();
  }else{
      moscowData.live=false;
  }

  if(currentBitPrice < 1){
    currentBitPrice = moscowData.bitPrice;
  }else{
      moscowData.bitPrice = currentBitPrice;
  }
  
  moscowData.time = moscowData.satoshis / moscowData.bitPrice;

  return moscowData;
}

namespace WS {
  WebServer server(80);

  int state=-1;

  void loop(){
     WS::server.handleClient();
  }
  void stop(){
    WS::server.stop();
    WS::state=-1;
  }
 
  void handleIndex(){
    Serial.println("handleIndex:");
    String wallet = WS::server.arg("wallet");
    Serial.println("GotWallet:" + wallet);
    if(wallet!=null && wallet.length() > 0){
      Serial.println("Setting Wallet");
      //currentWallet = const_cast<char*>(wallet.c_str());
      //WS::stop();
    }

    String html ="<html><head></head><body><a href='/stop'>Stop</a><form method='POST'><label for='wallet'>Wallet:</label><input type='text' name='wallet'><br><input type='submit' value='Set'></form><div>Wallet:";
   
    if(currentWallet!=null){
      Serial.println("Have Wallet:" + String(currentWallet));
      html += String(currentWallet);
    }

    html +=" </div></body></html>";
    WS::server.send(200, "text/html", html); 
  }
   void init(){
    WS::server.on("/", WS::handleIndex);
    WS::server.on("/stop", WS::stop);

    WS::server.begin();
    WS::state=0;
  }

}

void Watchy::setupQr(){

  Serial.println("Setup QR");
  if(connectWiFi())
  {       
    WS::init();
    //
    display.init(0, false); //_initial_refresh to false to prevent full update on init
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.println("Connect to");
    display.print("SSID: ");
    display.println(WiFi.SSID());
    display.print("IP: ");
    display.println(WiFi.localIP());
    display.display(false); 

    while(WS::state==0){
      WS::loop();
      //sleep(10);
    }

    WiFi.mode(WIFI_OFF);
    btStop();
  }
  Serial.println("Exit QR");
  showMenu(menuIndex, false);
}

void Watchy::setupWifi(){
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  wifiManager.setTimeout(WIFI_AP_TIMEOUT);
  wifiManager.setAPCallback(_configModeCallback);
  if(!wifiManager.autoConnect(WIFI_AP_SSID)) {//WiFi setup failed
    display.init(0, false); //_initial_refresh to false to prevent full update on init
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.println("Setup failed &");
    display.println("timed out!");
    display.display(false); //full refresh
    display.hibernate();
  }else{
    display.init(0, false);//_initial_refresh to false to prevent full update on init
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.println("Connected to");
    display.println(WiFi.SSID());
    display.display(false);//full refresh
    display.hibernate();
  }
  //turn off radios
  WiFi.mode(WIFI_OFF);
  btStop();

  guiState = APP_STATE;  
}

void Watchy::_configModeCallback (WiFiManager *myWiFiManager) {
  display.init(0, false); //_initial_refresh to false to prevent full update on init
  display.setFullWindow();
  display.fillScreen(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_WHITE);
  display.setCursor(0, 30);
  display.println("Connect to");
  display.print("SSID: ");
  display.println(WIFI_AP_SSID);
  display.print("IP: ");
  display.println(WiFi.softAPIP());
  display.display(false); //full refresh
  display.hibernate();
}

bool Watchy::connectWiFi(){
    if(WL_CONNECT_FAILED == WiFi.begin()){//WiFi not setup, you can also use hard coded credentials with WiFi.begin(SSID,PASS);
        WIFI_CONFIGURED = false;
    }else{
        if(WL_CONNECTED == WiFi.waitForConnectResult()){//attempt to connect for 10s
            WIFI_CONFIGURED = true;
        }else{//connection failed, time out
            WIFI_CONFIGURED = false;
            //turn off radios
            WiFi.mode(WIFI_OFF);
            btStop();
        }
    }
    return WIFI_CONFIGURED;
}

void Watchy::showUpdateFW(){
    display.init(0, false); //_initial_refresh to false to prevent full update on init
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.println("Please Visit");
    display.println("watchy.sqfmi.com");
    display.println("with a Bluetooth");
    display.println("enabled device");
    display.println(" ");
    display.println("Press menu button");
    display.println("again when ready");
    display.println(" ");
    display.println("Keep USB powered");
    display.display(false); //full refresh
    display.hibernate();

    guiState = FW_UPDATE_STATE;  
}

void Watchy::updateFWBegin(){
    display.init(0, false); //_initial_refresh to false to prevent full update on init
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.println("Bluetooth Started");
    display.println(" ");
    display.println("Watchy BLE OTA");
    display.println(" ");
    display.println("Waiting for");
    display.println("connection...");
    display.display(false); //full refresh

    BLE BT;
    BT.begin("Watchy BLE OTA");
    int prevStatus = -1;
    int currentStatus;

    while(1){
    currentStatus = BT.updateStatus();
    if(prevStatus != currentStatus || prevStatus == 1){
        if(currentStatus == 0){
        display.setFullWindow();
        display.fillScreen(GxEPD_BLACK);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(0, 30);
        display.println("BLE Connected!");
        display.println(" ");
        display.println("Waiting for");
        display.println("upload...");
        display.display(false); //full refresh
        }
        if(currentStatus == 1){
        display.setFullWindow();
        display.fillScreen(GxEPD_BLACK);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(0, 30);
        display.println("Downloading");
        display.println("firmware:");
        display.println(" ");
        display.print(BT.howManyBytes());
        display.println(" bytes");
        display.display(true); //partial refresh        
        }
        if(currentStatus == 2){
        display.setFullWindow();
        display.fillScreen(GxEPD_BLACK);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(0, 30);
        display.println("Download");
        display.println("completed!");
        display.println(" ");
        display.println("Rebooting...");
        display.display(false); //full refresh

        delay(2000);
        esp_restart();           
        }
        if(currentStatus == 4){
        display.setFullWindow();
        display.fillScreen(GxEPD_BLACK);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(0, 30);
        display.println("BLE Disconnected!");
        display.println(" ");
        display.println("exiting...");
        display.display(false); //full refresh
        delay(1000);
        break;
        }
        prevStatus = currentStatus;
    }
    delay(100);
    }

    //turn off radios
    WiFi.mode(WIFI_OFF);
    btStop();
    showMenu(menuIndex, false);
}

// time_t compileTime()
// {   
//     const time_t FUDGE(10);    //fudge factor to allow for upload time, etc. (seconds, YMMV)
//     const char *compDate = __DATE__, *compTime = __TIME__, *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
//     char compMon[3], *m;

//     strncpy(compMon, compDate, 3);
//     compMon[3] = '\0';
//     m = strstr(months, compMon);

//     tmElements_t tm;
//     tm.Month = ((m - months) / 3 + 1);
//     tm.Day = atoi(compDate + 4);
//     tm.Year = atoi(compDate + 7) - YEAR_OFFSET; // offset from 1970, since year is stored in uint8_t
//     tm.Hour = atoi(compTime);
//     tm.Minute = atoi(compTime + 3);
//     tm.Second = atoi(compTime + 6);

//     time_t t = makeTime(tm);
//     return t + FUDGE;        //add fudge factor to allow for compile time
// }
