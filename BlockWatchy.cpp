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
RTC_DATA_ATTR bool useNetwork=true;

RTC_DATA_ATTR bool loaded=false;

char currentWallet [] RTC_RODATA_ATTR= { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };

namespace TimeUtils {
  static unsigned long getTime() {
    return std::time(0);
  }
}

namespace FileStor{

  static String storFilePath = "/stor.json";
  static String walletFilePath = "/stor.wallet"; 

  static void begin(){
    bool began = SPIFFS.begin();
    if(!began){
      Serial.println("Attepting to format SPIFFS ");  
      if(!SPIFFS.begin(true)){ 
        Serial.println("An Error has occurred while mounting SPIFFS");  
      }
    }

    Serial.print('SPIFFS MOUNTED: ');
    Serial.println(began);
    Serial.print("Stor File Exists: ");  
    Serial.println(SPIFFS.exists(storFilePath));
    Serial.print("Wallet File Exists: ");  
    Serial.println(SPIFFS.exists(walletFilePath));
    
  }
   // Loads the configuration from a file
  static void loadConfiguration() {
    Serial.println("LoadConfig");

    FileStor::begin();
    
    // Open file for reading
    File storFile = SPIFFS.open(storFilePath);

    String storData;
    while(storFile.available()){
        storData+=char(storFile.read());
    }
    storFile.close();

    Serial.println("Read Json:" + storData);
    JSONVar loadData = JSON.parse(storData);
    
    int height = int(loadData["height"]);
    if(!std::isnan(height)){
      currentBlockHeight = height;
    }
    
    long epoch = long(loadData["epoch"]);
    if(!std::isnan(epoch) && epoch < TimeUtils::getTime()){
      currentBlockEpoch = epoch;
    }

    int price =int(loadData["price"]);
    if(!std::isnan(price)){
     currentBitPrice = price;
    }
   
    int face = int(loadData["face"]);
    if(!std::isnan(face) && face >=0 && face<=2){
      currentFace = face;
    }
    // JSONVar walletData = loadData["wallet"];
    // if(walletData != null){
    //   String walletString = JSON.stringify(walletData);
    //   if(walletString.length() > 0){
    //     strcpy(currentWallet, walletString.c_str());
    //   }
    // }

   File walletFile = SPIFFS.open(walletFilePath);

    String walletData;
    while(walletFile.available()){
        walletData+=char(walletFile.read());
    }
    
    walletFile.close();
    
    Serial.print('WalletData:');
    Serial.println(walletData);
    
    if(walletData !=null && walletData.length() > 0){
      strcpy(currentWallet,walletData.c_str());
    }
  
  //Serial.println("Loaded height:" + static_cast<String>(FileStor::data.height)+" epoch:" + static_cast<String>(FileStor::data.epoch)+" price:" + static_cast<String>(FileStor::data.price));
    
    loaded = true;
  }
  
  // Saves the configuration to a file
  static void saveConfiguration() {
    Serial.println("SaveConfig");
    // Delete existing file, otherwise the configuration is appended to the file
    
    
    FileStor::begin();
    // Open file for writing
    if(SPIFFS.exists(storFilePath)){
      SPIFFS.remove(storFilePath);
    }
    File storFile = SPIFFS.open(storFilePath, FILE_WRITE);
    if (!storFile) {
      Serial.println("Failed to create file");
      return;
    }

    JSONVar jData;

    jData["height"] = currentBlockHeight;
    jData["epoch"] = currentBlockEpoch;
    jData["price"] = currentBitPrice;
    jData["face"] = currentFace;

    String sData = JSON.stringify(jData);
    Serial.println("Serialized Json:" + sData);
    // Serialize JSON to file
    storFile.print(sData);
  
    // Close the file
    storFile.close();
  } 

  static void saveWallet(){
    Serial.println("Save Wallet");
    // Delete existing file, otherwise the configuration is appended to the file
  
    FileStor::begin();
    if(SPIFFS.exists(walletFilePath)){
      SPIFFS.remove(walletFilePath);
    }
    File walletFile = SPIFFS.open(walletFilePath, FILE_WRITE);
    if (!walletFile) {
      Serial.println("Failed to create file");
      return;
    }

    walletFile.print(currentWallet);
    walletFile.close();
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

namespace WS {
  WebServer server(80);

  int state=-1;
  unsigned long started;
  void stop(){
    WS::server.stop();
    WS::state=-1;
    WS::started = 0;
  }
  void loop(){
    if(TimeUtils::getTime() - started > WIFI_AP_TIMEOUT){
      WS::stop();
    }else{
     WS::server.handleClient();
    }
  }


  void handleIndex(){
    Serial.println("handleIndex:");
    String wallet = WS::server.arg("wallet");
    Serial.println("GotWallet:" + wallet);
    String error;
    if(wallet!=null && wallet.length() > 0){
      if(wallet.length()>12
        && (wallet.substring(0,3)=="bc1"||wallet[0]=='1'||wallet[0]==3)){
        Serial.println("Setting Wallet");
        
        strcpy(currentWallet,wallet.c_str());
        FileStor::saveWallet();
        
        String html ="<html><head><style>\r\n";
        html+= "body { background-color: black; color:white; }\r\n";
        html+="</style></head><body>";
        html+="<center>Wallet Saved: " + String(currentWallet) + "</center>";
        html +="</body></html>";
        WS::server.send(200, "text/html", html); 
        WS::stop();
      }else{
        error="Invalid Bitcoin Address";
      }
    }

    String html ="<html><head><style>\r\n";
    html+= "body { background-color: black; color:white; }\r\n";
    html+= "form{ position:absolute; left: 50%; top: 25%; transform:translateX(-50%); }\r\n";
    html+= "input{ display:block; margin-top:.25rem; }\r\n";
    html+= ".error{	color:red; display:block; position:absolute; left: 50%; top: 5%; transform:translateX(-50%); }\r\n";
    html+="</style></head><body>";
    if(error!=null){
      Serial.println(error);
      html+="<div class='error'>"+error+"</div>";
    }
    html+="<form method='POST'>";
    html+="<label for='wallet'>Current Wallet:";
    String cWal = String(currentWallet);
    if(cWal.length()>0){
      Serial.print("Sending Current Wallet:");
      Serial.println(cWal);
      html+=cWal;
    }else{
      Serial.print("Stored Wallet Empty:");
      Serial.println(currentWallet);
    }
    html+="</label><input type='text' name='wallet' >";
    html+="<input type='submit' value='Set'></form>";
   
    html +="</body></html>";
    WS::server.send(200, "text/html", html); 
  }
  
  void init(){
    WS::server.on("/", WS::handleIndex);
    WS::server.on("/stop", WS::stop);

    WS::server.begin();
    WS::state=0;
    WS::started = TimeUtils::getTime();
  }

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

    if(loaded==false){
      FileStor::loadConfiguration();
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
  esp_sleep_enable_timer_wakeup(120000000);
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

void Watchy::showLoading(){
    display.init(0, false); //_initial_refresh to false to prevent full update on init
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);

    int16_t  x1, y1;
    uint16_t w, h;
    int16_t yPos;
    display.getTextBounds("Loading...", 0, 0, &x1, &y1, &w, &h);
    display.setCursor(100 - (w/2),100 - (h/2));
    display.print("Loading...");

    display.display(false); //partial refresh
}

void Watchy::setFace(int face){
    useNetwork=true;
    currentFace = face;
    guiState = WATCHFACE_STATE;
    showLoading();
    showWatchFace(false);
}

void Watchy::handleButtonPress(){
  uint64_t wakeupBit = esp_sleep_get_ext1_wakeup_status();
  //Menu Button
  if (wakeupBit & MENU_BTN_MASK){
    Serial.println("Menu Button");
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
        default:
          break;                              
      }
    }
  }
  //Back Button
  else if (wakeupBit & BACK_BTN_MASK){
      Serial.println("Back Button");
    if(guiState == MAIN_MENU_STATE){//exit to watch face if already in menu
      RTC.alarm(ALARM_2); //resets the alarm flag in the RTC
      RTC.read(currentTime);
      showLoading();
      showWatchFace(false);
    }else if (guiState = QR_STATE){
      WS::stop();
      showMenu(menuIndex, false);
    }else if(guiState == APP_STATE){
      showMenu(menuIndex, false);//exit to menu if already in app
    }else{
      showMenu(menuIndex, false);//exit to menu if already in app
    }
  }
  //Up Button
  else if (wakeupBit & UP_BTN_MASK){
    Serial.println("Up Button");
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
    Serial.println("Down Button");
    if(guiState == MAIN_MENU_STATE){//decrement menu index
      menuIndex++;
      if(menuIndex > MENU_LENGTH - 1){
        menuIndex = 0;
      }
      showMenu(menuIndex, true);
    }
  }else{

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
                    default:
                    break;                              
                }
            }
          }else if(digitalRead(BACK_BTN_PIN) == 1){
            lastTimeout = millis();
            if(guiState == MAIN_MENU_STATE){//exit to watch face if already in menu
            RTC.alarm(ALARM_2); //resets the alarm flag in the RTC
            RTC.read(currentTime);
            showLoading();
            showWatchFace(false);
            break; //leave loop
            }else if(guiState == APP_STATE){
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

    const char *menuItems[] = {"Block Face", "Moscow Face", "QR Face", "Setup QR", "Setup WiFi"};
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

    String percentage = ((String)getBatteryPercentage())+'%';

    display.getTextBounds(percentage, 0, 190, &x1, &y1, &w, &h);
    display.setCursor(190 - w, 190);
    display.print(percentage);

    display.display(partialRefresh);
    //display.hibernate();

    guiState = MAIN_MENU_STATE;    
}

float Watchy::getBatteryVoltage(){
    float voltage = analogRead(ADC_PIN) / 4096.0 * 7.23;
    Serial.print('Voltage:');
    Serial.println(voltage);
    return voltage;
}

int Watchy::getBatteryPercentage(){
    int percent = (getBatteryVoltage()*100.0)-320;
    Serial.print('Percentage:');
    Serial.println(percent);
    return percent>0?(percent<100?percent:100):0;
}

void Watchy::showFastMenu(byte menuIndex){
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);

    int16_t  x1, y1;
    uint16_t w, h;
    int16_t yPos;

    const char *menuItems[] = {"Block Face", "Moscow Face", "QR Face", "Setup QR", "Setup WiFi"};
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

    display.setTextColor(GxEPD_WHITE);
    String percentage = ((String)getBatteryPercentage())+'%';
    
    display.getTextBounds(percentage, 0, 190, &x1, &y1, &w, &h);
    display.setCursor(190 - w, 190);
    display.print(percentage);

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
            case 4:
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
                    display.drawRect(s_x, d_h, b_d, b_d, (bd.live==true?GxEPD_WHITE:GxEPD_BLACK));
                  }else{
                    display.fillRect(s_x, d_h, b_d, b_d, (bd.live==true?GxEPD_WHITE:GxEPD_BLACK));
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
              
              if(md.live==true){
                  display.fillScreen(GxEPD_BLACK);
                  display.setTextColor(GxEPD_WHITE);
                  display.drawBitmap(0, 0, MOSCOW_BMP_DATA, MOSCOW_BMP_WIDTH, MOSCOW_BMP_HEIGHT, GxEPD_WHITE);
              }else{
                display.fillScreen(GxEPD_WHITE);
                display.setTextColor(GxEPD_BLACK);
                display.drawBitmap(0, 0, MOSCOW_BMP_DATA, MOSCOW_BMP_WIDTH, MOSCOW_BMP_HEIGHT, GxEPD_BLACK);
              }
              
           
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
              qrFace();
            }
              break;
          }
          
          Serial.println("End Draw Seq");
}

void Watchy::drawArray(QRCode qr, uint8_t* data){
  int mx = GxEPD2_154_D67::HEIGHT/qr.size;

  int dy = 0;
  
  bool pSet=false;
  for (uint8_t y = 0; y < qr.size; y++) {
    int dx = 0;
    for (uint8_t x = 0; x < qr.size; x++) {
      pSet= qrcode_getModule(&qr, x, y);
      display.fillRect(dx, dy, mx, mx, pSet?GxEPD_BLACK:GxEPD_WHITE);
      dx+=mx;
    }
    dy+=mx;
  }
}

void Watchy::qrFace(){
  Serial.println("Draw QR:");
  if(currentWallet!=null && String(currentWallet).length()>0){
      display.fillScreen(GxEPD_WHITE);
            
      // Create the QR code
      QRCode qrcode;
      uint8_t qrcodeData[qrcode_getBufferSize(4)];
      char* qrChars = const_cast<char*>(currentWallet);
      qrcode_initText(&qrcode, qrcodeData, 4, 0, qrChars);
      
      drawArray(qrcode, qrcodeData);
  }else{
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.println("Setup QR Code");
  }
}
//https://blockchain.info/q/addressbalance
BlockData Watchy::getBlockData(){
    BlockData blockData;
    //if were approaching the 
  
        if(useNetwork==true){
          if(connectWiFi()){       
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
                FileStor::saveConfiguration();
              }
              
              blockData.live=true;
            }else{
              blockData.live=false;
            }
           
            http.end();
            
            //Shut off WifiRadio
            WiFi.mode(WIFI_OFF);
            btStop();
            useNetwork=false;
        }else{
            useNetwork=true;
            blockData.live=false;
        }
      }else{
        useNetwork=true;
        blockData.live=true;
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

        if(useNetwork==false && blockData.elapsed>=500){
          useNetwork = true;
        } 
    
        if(blockData.live==false && blockData.elapsed > 600){
             //were disconnected, calculate height if we have elapsed a block
          while(blockData.elapsed >= 600){
            blockData.height++;
            blockData.elapsed -= 600;
            currentBlockHeight =  blockData.height;
            currentBlockEpoch += 600;
          }
        }
        
        blockData.complete =(double)blockData.elapsed / (double)600;
        
        return blockData;
}

MoscowData Watchy::getMoscowData(){
  MoscowData moscowData;
  if(useNetwork==true){
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
            FileStor::saveConfiguration();
          }
          
          moscowData.live=true;
        }else{
          moscowData.live=false;
        }
        
        http.end();
        
        //Shut off WifiRadio
        WiFi.mode(WIFI_OFF);
        btStop();
        useNetwork=false;
    }else{
        useNetwork=true;
        moscowData.live=false;
    }
  }else{
    useNetwork=true;
    moscowData.live=true;
  }

  if(currentBitPrice < 1){
    currentBitPrice = moscowData.bitPrice;
  }else{
      moscowData.bitPrice = currentBitPrice;
  }
  
  moscowData.time = moscowData.satoshis / moscowData.bitPrice;

  return moscowData;
}



void Watchy::setupQr(){
  showLoading();

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
    display.hibernate();

    guiState = QR_STATE;

    while(WS::state==0){
      WS::loop();
      //sleep(10);
    }

    WiFi.mode(WIFI_OFF);
    btStop();
  }

  if(String(currentWallet).length() > 0){
    display.init(0, false); //_initial_refresh to false to prevent full update on init
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.println("Wallet Saved:");
    display.println(String(currentWallet));
    display.display(false); //full refresh
    display.hibernate();
  }else{
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
  }
  // Serial.println("Exit QR");
  // showMenu(menuIndex, false); 
}

void Watchy::setupWifi(){
  showLoading();

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
