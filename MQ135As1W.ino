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

#include <EEPROM.h>
#include "MQ135.h"
#include "OneWireHub.h"
#include "OneWireItem.h"
#include <TrueRandom.h>

// Define if to use DS2438 or DS18B20, comment out not needed type, default DS2438, DS2401 mode not integrated at the moment
//#define USE_DS18B20
#define USE_DS2438
//#define USE_DS2401

// Pin definition
#define PIN_D_MQ135    2  // Pin for digital input from MQ135
#define PIN_A_MQ135   A0  // Pin for analog input from MQ135
#define PIN_ONE_WIRE   8  // 1-Wire pin

// Reading interval
// Note that OneWire data polling causes some delays, so don't expect exact timing defined in here.
// Better not to touch it
#define READING_INTERVAL 5000

// Init delay, MQ135 needs some time to heat up, suggest to use delay 3 minutes => 3 * 60 * 1000
// If you don't find this useful, comment next line out
#define INIT_DELAY 180000

#ifdef USE_DS18B20
  /*
   * DS18B20 can encode max 2000 values in range 0 – 125 °C, so the max ppm would be 2000
   * With this ratio you can extend it (for example make it to 5.0), but you will loose precision on lower values
   * Ratio 5.0 will support ppm range 0 – 10000
   *    Conversion => (Temp / 125.0) * 2000 * RATIO
   * In Loxone use correction and for input value 2 use "1" and for dispay value 16 * DS18B20_RATIO (default 80)
   */
  #define DS18B20_RATIO              5.0F
  #define DS18B20_MAX_VALUE       2000.0F // Do not touch
  #define DS18B20_MAX_TEMPERATURE  125.0F // Do not touch
#endif

#ifdef USE_DS2438
  /*
   * DS2438 can encode 5760 values in range –55 – 125 °C
   * SW will use all of these to get highest possible precision
   * Temp value will be used to transfer modulo of ppm (5760)
   * VAD value will be used to transfer divison of ppm (5760)
   * With this conversion, max usable ppm is 5628934.50 ppm which should be enough...
   *    Conversion => (((Temp + 55.0) / 180) + (VAD * 100)) * 5760
   * In Loxone use Formula I1 = Temperature, I2 = VAD, Formula = (((I1 + 55.0) / 180) + (I2 * 100)) * 5760
   * 
   * VDD value contains analog measurement
   * Vsens value is always 0
   */
  #define DS2438_MAX_VALUE          5760 // Do not touch
  #define DS2438_MAX_VALUE_F     5760.0F // Do not touch
  #define DS2438_MIN_TEMPERATURE  -55.0F // Do not touch
  #define DS2438_MAX_TEMPERATURE  125.0F // Do not touch
  #define DS2438_TEMP_RANGE       180.0F // Do not touch
  #define DS2438_VAD_STEPS          1023 // Do not touch
#endif

// Init Hub
OneWireHub hub = OneWireHub(PIN_ONE_WIRE);

// Init MQ135 library
MQ135 mq = MQ135(PIN_A_MQ135);

// DS18B20
#ifdef USE_DS18B20
  #include "DS18B20.h"
  DS18B20 *ds18b20;
#endif

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
    Serial.begin(115200);
    Serial.println("OneWire-Hub MQ135 sensor");

    pinMode(LED_BUILTIN, OUTPUT);

    // Init digital input
    #ifdef USE_DS2401
      pinMode(PIN_D_MQ135, INPUT);
    #endif;

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

    // Init DS18B20 1W
    #ifdef USE_DS18B20
      ds18b20 = new DS18B20(DS18B20::family_code, addr[1], addr[2], addr[3], addr[4], addr[5], addr[6]);
      hub.attach(*ds18b20);
    #endif

    // Init DS2438
    #ifdef USE_DS2438
      ds2438 = new DS2438New(DS2438New::family_code, addr[1], addr[2], addr[3], addr[4], addr[5], addr[6]);
      ds2438->setCurrent(0);
      hub.attach(*ds2438);
    #endif

    // Log addresses
    #ifdef USE_DS18B20
      dumpAddress("1-Wire DS18B20 MQ135 device address: ", ds18b20, "");
    #endif
    #ifdef USE_DS2438
      dumpAddress("1-Wire DS2438 MQ135 device address: ", ds2438, "");
    #endif

    #ifdef INIT_DELAY
      delay(INIT_DELAY);
    #endif
}

// Loop call
void loop() {
    // Read value from sensor
    float ppm = mq.getPPM();
    Serial.print("Reading PPM: "); Serial.println(ppm);

    #ifdef USE_DS18B20
      // Assign DS18B20 data
      uint16_t ds1820 = (ppm < (DS18B20_MAX_VALUE * DS18B20_RATIO)) ? (uint16_t)(ppm / DS18B20_RATIO) : DS18B20_MAX_VALUE;
      ds18b20->setTemperature((float)((ds1820 / DS18B20_MAX_VALUE) * DS18B20_MAX_TEMPERATURE));
    #endif

    #ifdef USE_DS2438
      // Assign DS2438 data
      uint16_t ds2438temp = (uint32_t)ppm % DS2438_MAX_VALUE;
      uint16_t ds2438vad = min((uint16_t)((uint32_t)ppm / DS2438_MAX_VALUE), 1000);
      ds2438->setTemperature(((ds2438temp / DS2438_MAX_VALUE_F) * DS2438_TEMP_RANGE) + DS2438_MIN_TEMPERATURE);
      ds2438->setVADVoltage(ds2438vad);
      ds2438->setVDDVoltage(analogRead(PIN_A_MQ135));
    #endif

    // Polling one wire data
    unsigned long stopPolling = millis() + READING_INTERVAL;
    while (stopPolling > millis())
        hub.poll();
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
