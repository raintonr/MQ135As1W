/**************************************************************************/
/*! 
    @file     MQ135As1W.ino
    @version  1.0.0
    @author   Jiri Jasa
    @company  DESO development, s.r.o.
              http://www.deso.cz

    This tool is able to read data from MQ135 gas sensors, received values are then send over 1-Wire as two DS18B20 sensors or one DS2438.
    If DS18B20 is used it's limited by max value you will set, see DS18B20_RATIO. Default ratio is 5.0, so it's possible to use reading 0 – 10000 ppm.
       Conversion formula = (Temp / 125.0) * 2000 * RATIO

    For D2438 calculation is more complicated, but you get much more precise result.
    Temperature + VAD values are used to transfer ppm, max value for this setup is 5628934.50 ppm.
       Conversion formula = (((Temp + 55.0) / 180) + (VAD * 100)) * 5760
    DS2438 also returns analog measurement in VDD, Vsens measurement is always 0.
    
    During first run Arduino generates it's own random 1-Wire address that is then stored in EEPROM
    Make sure to define either DS18B20 or DS2438, don't use both of them at the same time!

    Power consumption of Arduino + MQ135 is around 0.2 A, so definitely power it with external power supply.

    MQ135:
      https://www.aliexpress.com/wholesale?SearchText=MQ135

    Prerequisites:
      https://github.com/GeorgK/MQ135
      https://github.com/orgua/OneWireHub
      https://github.com/sirleech/TrueRandom
*/
/**************************************************************************/

//#define DEBUG

#include <EEPROM.h>
#include "OneWireHub.h"
#include "OneWireItem.h"
#include <TrueRandom.h>

// Define if to use DS2438 (other options removed in this version)
#define USE_DS2438

// Pin definition
#define PIN_A_MQ135   A0  // Pin for analog input from MQ135
#define PIN_ONE_WIRE  11  // 1-Wire pin

// Reading interval
// Note that OneWire data polling causes some delays, so don't expect exact timing defined in here.
// Better not to touch it
#define READING_INTERVAL 1000

// Keep a moving average over this many readings
#define MA_READINGS 48

int ma_total = 0;
int ma_output = 0;
int ma_init = false;

// Init delay, MQ135 needs some time to heat up, suggest to use delay 3 minutes => 3 * 60 * 1000
// If you don't find this useful, comment next line out
#ifdef DEBUG
  #define INIT_DELAY 5000
#else
  #define INIT_DELAY 180000
#endif

// Init Hub
OneWireHub hub = OneWireHub(PIN_ONE_WIRE);

// DS2438
#ifdef USE_DS2438
  #include "DS2438New.h"
  DS2438New *ds2438;
#endif

// 1W address
uint8_t addr[7] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// Function definition
void dumpAddress(char *prefix, OneWireItem *item, char *postfix);

// Setup call
void setup() {
    // Use serial only for debug, once deploying solution comment out Serial.begin line
    #ifdef DEBUG    
      Serial.begin(115200);
      Serial.println("OneWire-Hub MQ135 sensor");
    #endif

    pinMode(LED_BUILTIN, OUTPUT);

    // 1W address storred in EEPROM
    if (EEPROM.read(0) == '#') {
        Serial.println("Reading 1W address from EEPROM");
        for (int i = 1; i < 7; i++)
            addr[i] = EEPROM.read(i);
    }
    // 1W address not set, generate random value
    else {
        Serial.println("Generating random 1W address");
        for (int i = 1; i < 7; i++) {
            addr[i] = (i != 6) ? TrueRandom.random(256): TrueRandom.random(256);
            EEPROM.write(i, addr[i]);
        }
        EEPROM.write(0, '#');
    }

    // Init DS2438
    #ifdef USE_DS2438
      ds2438 = new DS2438New(DS2438New::family_code, addr[1], addr[2], addr[3], addr[4], addr[5], addr[6]);
      ds2438->setCurrent(0);
      ds2438->setVDDVoltage(0);
      ds2438->setTemperature((int8_t) 0);
      hub.attach(*ds2438);
    #endif

    // Log addresses
    #ifdef USE_DS2438
    #ifdef DEBUG
      dumpAddress("1-Wire DS2438 MQ135 device address: ", ds2438, "");
    #endif
    #endif

    #ifdef INIT_DELAY
      Serial.print("Init delay...");
      unsigned long delayEnd= millis() + INIT_DELAY;
      while (delayEnd > millis()) {
        digitalWrite(LED_BUILTIN, ((millis() % 100) < 50) ? 1 : 0);
      }
      Serial.println("done");
    #endif
}

// Loop call
int samples = 0;
void loop() {
    // Read value from sensor
    int mq135 = analogRead(PIN_A_MQ135);

    // Modify moving average accordingly
    if (!ma_init) {
      // First reading, just use the one we read
      ma_output = mq135;
      ma_total = ma_output * MA_READINGS;
      ma_init = true;
    } else {
      // Subtract one reading from total to give (MA_READINGS - 1) values totalled up.
      ma_total -= ma_output;
      // Add the value we just read
      ma_total += mq135;
      // And divide the total by readings to get the current average
      ma_output = ma_total / MA_READINGS;
    }

    #ifdef DEBUG
      Serial.print("MA value: "); Serial.print(ma_output);
      Serial.print(" Last raw value: "); Serial.println(mq135);
    #endif

    #ifdef USE_DS2438
      ds2438->setVADVoltage(ma_output);
    #endif

    // Polling one wire data
    unsigned long stopPolling = millis() + READING_INTERVAL;
    // Flash for 10ms
    unsigned long longFlash = millis() + 10;
    // Only Flash once every 30s
    boolean flash = (millis() % 30000) < READING_INTERVAL ? 1 : 0;
    while (stopPolling > millis()) {
        digitalWrite(LED_BUILTIN, ((millis() < longFlash) && flash) ? 1 : 0);
        hub.poll();
    }
}

// Prints the device address to console
void dumpAddress(char *prefix, OneWireItem *item, char *postfix) {
  Serial.print(prefix);
  
  for (int i = 0; i < 8; i++) {
      Serial.print(item->ID[i] < 16 ? "0":"");
      Serial.print(item->ID[i], HEX);
      Serial.print((i < 7)?".":"");
  }

  Serial.println(postfix);
}
