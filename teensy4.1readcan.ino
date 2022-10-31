#include "Arduino.h"
#include <chrono>
#include <FlexCAN_T4.h>
#include <TimeLib.h>
#include <SD.h>
#include <SPI.h>

#define TIME_HEADER  "T"   // Header tag for serial time sync message

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> can1;
CAN_message_t msg;
const int chipSelect = BUILTIN_SDCARD;
using namespace std::chrono;

void setup(void) {
  setSyncProvider(getTeensy3Time);
  Serial.begin(115200);
  while (!Serial);  // Wait for Arduino Serial Monitor to open
  delay(100);
  if (timeStatus()!= timeSet) {
    Serial.println("Unable to sync with the RTC");
  } else {
    Serial.println("RTC has set the system time");
  }
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    while (1) {
      // No SD card, so don't do anything more - stay stuck here
    }
  }
  Serial.println("card initialized.");
  can1.begin();
  can1.setBaudRate(250000);
  can1.setMaxMB(64);
  can1.enableFIFO();
  can1.enableFIFOInterrupt();
  can1.onReceive(canSniff);
}

uint64_t get_RTC_periods()
{
    uint32_t hi1 = SNVS_HPRTCMR, lo1 = SNVS_HPRTCLR;
    while (true)
    {
        uint32_t hi2 = SNVS_HPRTCMR, lo2 = SNVS_HPRTCLR;
        if (lo1 == lo2 && hi1 == hi2)
        {
            return (uint64_t)hi2 << 32 | lo2;
        }
        hi1 = hi2;
        lo1 = lo2;
    }
}

// this is all you need to do to prepare the built in system_clock for usage with the RTC (resolution ~30.5µs)
uint64_t ns_rtc()
{
    uint64_t ns = get_RTC_periods() * (1E9 / 32768);
    return ns;
}




unsigned long processSyncMessage() {
  unsigned long pctime = 0L;
  const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013 

  if(Serial.find(TIME_HEADER)) {
     pctime = Serial.parseInt();
     return pctime;
     if( pctime < DEFAULT_TIME) { // check the value is a valid time (greater than Jan 1 2013)
       pctime = 0L; // return 0 to indicate that the time is not valid
     }
  }
  return pctime;
}


time_t getTeensy3Time()
{
  return Teensy3Clock.get();
}

char *uint64_to_string(uint64_t input)
{
    static char result[21] = "";
    // Clear result from any leftover digits from previous function call.
    memset(&result[0], 0, sizeof(result));
    // temp is used as a temporary result storage to prevent sprintf bugs.
    char temp[21] = "";
    char c;
    uint8_t base = 10;

    while (input) 
    {
        int num = input % base;
        input /= base;
        c = '0' + num;

        sprintf(temp, "%c%s", c, result);
        strcpy(result, temp);
    } 
    return result;
}

void canSniff(const CAN_message_t &msg) {
    uint64_t timestamp = ns_rtc();
    Serial.println(timestamp);
    String current_t = uint64_to_string(timestamp);
    File dataFile = SD.open("datalog.txt", FILE_WRITE);
    String dataString = "";
    dataString += "(";
    dataString += current_t;
    dataString += ")  ";
    dataString += "can1";
    dataString += "  "; 
    dataString += String(msg.id,HEX);
    dataString += " ";   
    dataString += "[";
    dataString += String(msg.len);
    dataString += "]";
    dataString += " ";
    for ( uint8_t i = 0; i < msg.len; i++ ) {
      String str = "";
      str = String(msg.buf[i],HEX);
      if (str == "0"){
        str += str;
      }
      str.toUpperCase();
      dataString += str;
      dataString += " ";
    }
    //Serial.println(dataString);
    time_t tt = Teensy3Clock.get();
    //Serial.print("RTC Timestamp: ");
    //Serial.println(tt);
    // if the file is available, write to it:
    if (dataFile) {
      dataFile.println(dataString);
      dataFile.close();
      // print to the serial port too:
      //Serial.println(dataString);
    } else {
    // if the file isn't open, pop up an error:
      Serial.println("error opening datalog.txt");
    }
}


void loop() {
  if (Serial.available()) {
    time_t t = processSyncMessage();
    if (t != 0) {
      Teensy3Clock.set(t); // set the RTC
      setTime(t);
    }
  }
  can1.events();
}
