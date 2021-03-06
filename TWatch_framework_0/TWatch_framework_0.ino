// An Arduino based framework for the Lilygo T-Watch 2020
// Much of the code is based on the sample apps for the
// T-watch that were written and copyrighted by Lewis He.
//(Copyright (c) 2019 lewis he)

#include "config.h"
#include "esp32-hal-cpu.h"
#include <Preferences.h>
#include "FS.h"
#include <SPIFFS.h>
#include <time.h>
#include <SD.h>

#define FORMAT_SPIFFS_IF_FAILED true

#define MAX30105_ADDRESS            0x57
#define PCF8563_ADDRESS             0x51
#define BMA423_ADDRESS              0x19
#define AXP202_ADDRESS              0x35
#define FT6206_ADDRESS              0X38
#define MPU6050_ADDRESS             0x68
#define MPR121_ADDRESS              0x5A
#define DRV2605_ADDRESS             0x5A


char diagnostics[64];

TTGOClass *ttgo;
Preferences preferences;

AXP20X_Class *power;

static Adafruit_DRV2605 *drv;
static TinyGPSPlus *gps;

uint32_t targetTime = 0;       // for next 1 second display update


uint8_t hh, mm, ss, mmonth, dday; // H, M, S variables
uint16_t yyear; // Year is 16 bit int

int qarter1 = 0;
int qarter2 = 0;
int qarter3 = 0;
int qarter4 = 0;

bool rtcIrq = false;
bool power_irq = false;
bool power_button_irq = false;

bool display_on = false;

int seconds_counter = 0;

BMA *sensor;
bool step_irq = false;
//RTC_DATA_ATTR int step_counter_save = 0;
int step_counter_initial = 0;
unsigned int step_save = 0;
int step_started = 0;
int no_step_counter = 0;
int step_counter = 0;
unsigned int weight_factor_10 = 0;
float current_weight = 0;
int current_pull = 0;
int current_push = 0;



//******************************************** print wakeup reason ***************************
//Method to print the reason by which ESP32
//has been awaken from sleep
void print_wakeup_reason()
{
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : 
    {
        Serial.printf("Wakeup was not caused by deep sleep\n"); 
        //preferences.putUInt("step_save", 0);
        break;
    }
  }
}
//********************************************************************************************

void setup() 
{

    //******************************* PREFERENCES ************************************************
    // Open Preferences with twatch namespace. Each application module, library, etc
    // has to use a namespace name to prevent key name collisions. We will open storage in
    // RW-mode (second parameter has to be false).
    // Note: Namespace name is limited to 15 chars.
    preferences.begin("twatch", false);
    // Remove all preferences under the opened namespace
    //preferences.clear();
    // Or remove the step_save key only
    //preferences.remove("step_save");

    // Get the step_save value, if the key does not exist, return a default value of 0
    // Note: Key name is limited to 15 chars.
    step_save = preferences.getUInt("step_save", 0);
    preferences.end();
    //******************************* preferences ************************************************
  
    //initSetup();
    ttgo = TTGOClass::getWatch();
    ttgo->begin();
    ttgo->openBL(); // Turn on the backlight
    display_on = true;

    

    //ttgo->trunOnGPS();
    //ttgo->gps_begin();
    //gps = ttgo->gps;
    drv = ttgo->drv;

        
    ttgo->tft->setTextFont(1);
    ttgo->tft->fillScreen(TFT_BLACK);
    ttgo->tft->setTextColor(TFT_YELLOW, TFT_BLACK); // Note: the new fonts do not draw the background colour
    //Initialize lvgl
    ttgo->lvgl_begin();

    //************************************* SERIAL ****************************
    Serial.begin(115200);
    delay(1000); //Take some time to open up the Serial Monitor
    //*******************************
    //Print the wakeup reason for ESP32
    print_wakeup_reason();
    //*******************************
    getXtalFrequencyMhz();
    Serial.println();
    Serial.print("Xtal Frequency is: ");
    Serial.print(getXtalFrequencyMhz()); //Get Xtal clock
    Serial.print(" Mhz");
    setCpuFrequencyMhz(20); //Set CPU clock to 80MHz fo example
    Serial.println();
    Serial.print("CPU Frequency is: ");
    Serial.print(getCpuFrequencyMhz()); //Get CPU clock
    Serial.print(" Mhz");
    Serial.println();
    Serial.print("APB Frequency is: ");
    Serial.print(getApbFrequency()/1000000); //Get APB clock
    Serial.println(" Mhz");
    //************************************* serial ****************************

    //************************************* SD ****************************
    if (!ttgo->sdcard_begin()) 
    {
        Serial.println("SD init failed");
    } 
    else 
    {
        Serial.println("SD init OK");

        uint8_t cardType = SD.cardType();

        Serial.print("SD Card Type: ");
        if(cardType == CARD_MMC){
            Serial.println("MMC");
        } else if(cardType == CARD_SD){
            Serial.println("SDSC");
        } else if(cardType == CARD_SDHC){
            Serial.println("SDHC");
        } else {
            Serial.println("UNKNOWN");
        }

        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        Serial.printf("SD Card Size: %lluMB\n", cardSize);
    
        listDir(SD, "/", 0);
        //createDir(SD, "/mydir");
        //listDir(SD, "/", 0);
        //removeDir(SD, "/mydir");
        //listDir(SD, "/", 2);
        //writeFile(SD, "/hello.txt", "Hello ");
        //appendFile(SD, "/hello.txt", "World!\n");
        //readFile(SD, "/hello.txt");
        //deleteFile(SD, "/foo.txt");
        //deleteFile(SD, "/hello.txt");
        //deleteFile(SD, "/test.txt");
        //renameFile(SD, "/hello.txt", "/foo.txt");
        //readFile(SD, "/foo.txt");
        //testFileIO(SD, "/test.txt");
        Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
        Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
    }
    //************************************* sd ****************************


    //********************************** SPIFFS ********************************
    if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED))
    {
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    else
    {
        Serial.println("SPIFFS Mount OK");
    }
    //********************************** spiffs ********************************

    //************************************* POWER ************************************************
    power = ttgo->power;
    pinMode(AXP202_INT, INPUT);
    attachInterrupt(AXP202_INT, [] {power_irq = true;}, FALLING);
    // ADC monitoring must be enabled to use the AXP202 monitoring function
    power->adc1Enable(AXP202_VBUS_VOL_ADC1
                      | AXP202_VBUS_CUR_ADC1
                      | AXP202_BATT_CUR_ADC1
                      | AXP202_BATT_VOL_ADC1,
                      true);
    power->enableIRQ(AXP202_PEK_SHORTPRESS_IRQ
                     | AXP202_VBUS_REMOVED_IRQ
                     | AXP202_VBUS_CONNECT_IRQ
                     | AXP202_CHARGING_IRQ, true
                    );
    power->clearIRQ();

    // Use ext0 for external wakeup
    esp_sleep_enable_ext0_wakeup((gpio_num_t)AXP202_INT, LOW);
    //************************************* power ************************************************

    //*********************************** POWER BUTTON *******************************
    pinMode(AXP202_INT, INPUT_PULLUP);
    attachInterrupt(AXP202_INT, [] {power_button_irq = true;}, FALLING);
    // Must be enabled first, and then clear the interrupt status,
    // otherwise abnormal
    power->enableIRQ(AXP202_PEK_SHORTPRESS_IRQ,
                     true);
    //  Clear interrupt status
    power->clearIRQ();
    //*********************************** power button *******************************

    //********************************* STEP COUNTER *********************************************
    step_counter_initial = step_save;

    sensor = ttgo->bma;

    // Accel parameter structure
    Acfg cfg;
    cfg.odr = BMA4_OUTPUT_DATA_RATE_100HZ;
    cfg.range = BMA4_ACCEL_RANGE_2G;
    cfg.bandwidth = BMA4_ACCEL_NORMAL_AVG4;

    cfg.perf_mode = BMA4_CONTINUOUS_MODE;

    // Configure the BMA423 accelerometer
    sensor->accelConfig(cfg);

    // Enable BMA423 accelerometer
    // Warning : Need to use steps, you must first enable the accelerometer
    // Warning : Need to use steps, you must first enable the accelerometer
    // Warning : Need to use steps, you must first enable the accelerometer
    sensor->enableAccel();

    pinMode(BMA423_INT1, INPUT);
    attachInterrupt(BMA423_INT1, [] {
        // Set interrupt to set irq value to 1
        step_irq = 1;
    }, RISING); //It must be a rising edge

    // Enable BMA423 step count feature
    sensor->enableFeature(BMA423_STEP_CNTR, true);
    // Enable BMA423 isTilt feature
    sensor->enableFeature(BMA423_TILT, true);
    // Enable BMA423 isDoubleClick feature
    sensor->enableFeature(BMA423_WAKEUP, true);

    // Reset steps
    //sensor->resetStepCounter();

    // Turn on step interrupt
    sensor->enableStepCountInterrupt();
    sensor->enableTiltInterrupt();
    // It corresponds to isDoubleClick interrupt
    sensor->enableWakeupInterrupt();
    
    //********************************* step counter *********************************************


    //******************************** MOTOR *******************************
    //pinMode(4, OUTPUT); 
    //******************************** motor *******************************

    //**********************************************************************
    snprintf(diagnostics, sizeof(diagnostics), "jrnl: reset                ");
    //**********************************************************************

}

void loop() 
{

    if (targetTime < millis()) 
    {
        targetTime = millis() + 1000;
        displayTime(); // 
        //zoomzoom();

        preferences.begin("twatch", false);
        weight_factor_10 = preferences.getUInt("weight", 0);
        current_pull = preferences.getUInt("pull", 0);
        current_push = preferences.getUInt("push", 0);
        current_weight = ((float)weight_factor_10)/10.0;
        preferences.end();

        // activate motor every 5 minutes
        seconds_counter++;
        if(seconds_counter >= 180)
        {
            seconds_counter = 0;
            //digitalWrite(4, HIGH);
            //delay(30);
            //digitalWrite(4, LOW);
        }
        //*******************************

        no_step_counter++;
        if((step_started == 1) && (no_step_counter > 60))
        {
            step_started = 0;

            // read current time-date
            RTC_Date tnow = ttgo->rtc->getDateTime();
            hh = tnow.hour;
            mm = tnow.minute;
            ss = tnow.second;
            dday = tnow.day;
            mmonth = tnow.month;
            yyear = tnow.year;
        
            char message[128];
        
            snprintf(message, 128, "%04d.%02d.%02d %02d:%02d stp fin\r\n", yyear, mmonth, dday, hh, mm);
            //appendFile(SPIFFS, "/journal.txt", message);
            appendFile(SD, "/journal.txt", message);

            //**********************************************************************
            snprintf(diagnostics, sizeof(diagnostics), "jrnl: stp fin                ");
            //**********************************************************************
        }

        if((no_step_counter > 3) && (step_counter > 0))
            step_counter = 0;
    }

    int16_t x, y;
    if (ttgo->getTouch(x, y)) 
    {
        while (ttgo->getTouch(x, y)) {} // wait for user to release
    
        // This is where the app selected from the menu is launched
        // If you add an app, follow the variable update instructions
        // at the beginning of the menu code and then add a case
        // statement on to this switch to call your paticular
        // app routine.
    
        switch (modeMenu()) // Call modeMenu. The return is the desired app number
        { 
            case 0: // Zero is the clock, just exit the switch
              break;
            case 1:
              appJob();
              break;
            case 2:
              step_control_menu();
              break;
            case 3:
              appWeight();
              break;
            case 4:
              appSetTime();
              break;
            case 5:
              appGraph();
              break;
            case 6:
              pull_menu();
              break;
            case 7:
              push_menu();
              break;
            case 8:
              journal_menu();
              break;
            case 9:
              sleep_menu();
              break;
        }
        displayTime();
    }


    //*********************************** STEP IRQ ******************************************
    if (step_irq) 
    {
        step_irq = 0;
        bool  rlst = false;
        do 
        {
            // Read the BMA423 interrupt status,
            // need to wait for it to return to true before continuing
            rlst =  sensor->readInterrupt();
        } while (!rlst);

        // Check if it is a step interrupt
        if (sensor->isStepCounter()) 
        {
          
            // Get step data from register
            uint32_t step = sensor->getCounter();
            step_save = step_counter_initial + step;
            preferences.begin("twatch", false);
            preferences.putUInt("step_save", step_save);
            preferences.end();

            no_step_counter = 0;
            step_counter++;
            if((step_started == 0) && (step_counter > 3))
            {
                step_started = 1;

                // read current time-date
                RTC_Date tnow = ttgo->rtc->getDateTime();
                hh = tnow.hour;
                mm = tnow.minute;
                ss = tnow.second;
                dday = tnow.day;
                mmonth = tnow.month;
                yyear = tnow.year;
            
                char message[128];
            
                snprintf(message, 128, "%04d.%02d.%02d %02d:%02d stp sta\r\n", yyear, mmonth, dday, hh, mm);
                //appendFile(SPIFFS, "/journal.txt", message);
                appendFile(SD, "/journal.txt", message);

                //**********************************************************************
                snprintf(diagnostics, sizeof(diagnostics), "jrnl: stp sta");
                //**********************************************************************
            }
        }

        if (sensor->isDoubleClick())
        {
            if(display_on)
            {
                // turn off display
                power->setPowerOutPut(AXP202_LDO2, false);
                //ttgo->closeBL();
                display_on = false;
            }
            else
            {
                // turn on display
                power->setPowerOutPut(AXP202_LDO2, true);
                //ttgo->openBL();
                display_on = true;
                //**********************************************************************
                snprintf(diagnostics, sizeof(diagnostics), "dsp on: dbl clck                ");
                //**********************************************************************
            }
        }

        if (sensor->isTilt())
        {
            if(display_on)
            {
                // turn off display
                //power->setPowerOutPut(AXP202_LDO2, false);
                //ttgo->closeBL();
                //display_on = false;
            }
            else
            {
                // turn on display
                //power->setPowerOutPut(AXP202_LDO2, true);
                //display_on = true;
            }
        }
    }
    //*********************************** step irq ******************************************


    //******************************* POWER BUTTON IRQ ******************************************
    if (power_button_irq) 
    {
        power_button_irq = false;
        ttgo->power->readIRQ();
        if (ttgo->power->isPEKShortPressIRQ()) 
        {
            ttgo->power->clearIRQ();
            if(display_on)
            {
                // turn off display
                //power->setPowerOutPut(AXP202_LDO2, false);
    
                // enter deep sleep mode
                display_on = false;
                esp_deep_sleep_start();
                
            }
            else
            {
                // turn on display
                //power->setPowerOutPut(AXP202_LDO2, true);
                ttgo->openBL();
                display_on = true;
            }
        }
    }
    //******************************* power button irq ******************************************
    
}
//**********************************************************************************************
//**********************************************************************************************
//**********************************************************************************************
void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void createDir(fs::FS &fs, const char * path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}

void removeDir(fs::FS &fs, const char * path){
    Serial.printf("Removing Dir: %s\n", path);
    if(fs.rmdir(path)){
        Serial.println("Dir removed");
    } else {
        Serial.println("rmdir failed");
    }
}

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if(!file){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("File renamed");
    } else {
        Serial.println("Rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\n", path);
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
    }
}

void testFileIO(fs::FS &fs, const char * path){
    File file = fs.open(path);
    static uint8_t buf[512];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if(file){
        len = file.size();
        size_t flen = len;
        start = millis();
        while(len){
            size_t toRead = len;
            if(toRead > 512){
                toRead = 512;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;
        Serial.printf("%u bytes read for %u ms\n", flen, end);
        file.close();
    } else {
        Serial.println("Failed to open file for reading");
    }


    file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }

    size_t i;
    start = millis();
    for(i=0; i<2048; i++){
        file.write(buf, 512);
    }
    end = millis() - start;
    Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
    file.close();
}
