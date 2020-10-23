// Smart Battery Monitor
// works, but without real EPROM copy/recall functionallity, Timer,
// native bus-features: none

#ifndef ONEWIRE_DS2438NEW_H
#define ONEWIRE_DS2438NEW_H

#include "OneWireItem.h"

#ifndef ONEWIRE_DS2438_H
constexpr uint8_t MemDS2438[64] = {
                //  memory[0] = REG0_MASK_IAD | REG0_MASK_CA | REG0_MASK_EE | REG0_MASK_AD;
                0x09, 0x20, 0x14, 0xAC, 0x00, 0x40, 0x01, 0x00,
                0xEC, 0xAB, 0x23, 0x58, 0xFF, 0x08, 0x00, 0xFC,
                0x00, 0x00, 0x00, 0x00, 0x6D, 0x83, 0x03, 0x02,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
#endif

class DS2438New : public OneWireItem {
  private:

    static constexpr uint8_t PAGE_COUNT     { 8 }; // how much of the real 8 pages should be emulated, use at least 1, max 8
    static constexpr uint8_t PAGE_SIZE      { 8 }; //

    static constexpr uint8_t MEM_SIZE       { PAGE_COUNT * PAGE_SIZE };

    // Register Addresses
    static constexpr uint8_t REG0_MASK_IAD  { 0x01 }; // enable automatic current measurements
    static constexpr uint8_t REG0_MASK_CA   { 0x02 }; // enable current accumulator (page7, byte 4-7)
    static constexpr uint8_t REG0_MASK_EE   { 0x04 }; // shadow accu to eeprom
    static constexpr uint8_t REG0_MASK_AD   { 0x08 }; // 1: battery voltage, 0: ADC-GPIO
    static constexpr uint8_t REG0_MASK_TB   { 0x10 }; // temperature busy flag
    static constexpr uint8_t REG0_MASK_NVB  { 0x20 }; // eeprom busy flag
    static constexpr uint8_t REG0_MASK_ADB  { 0x40 }; // adc busy flag

    uint8_t memory[MEM_SIZE];  // this mem is the "scratchpad" in the datasheet., no EEPROM implemented
    uint8_t crc[PAGE_COUNT+1]; // keep the matching crc for each memory-page, reading can be very timesensitive

    uint8_t vadVoltage[2];
    uint8_t vddVoltage[2];

    void calcCRC(uint8_t page);
    void updateVoltage(uint8_t page);

public:

    static constexpr uint8_t family_code    { 0x26 };

    DS2438New(uint8_t ID1, uint8_t ID2, uint8_t ID3, uint8_t ID4, uint8_t ID5, uint8_t ID6, uint8_t ID7);

    void     duty(OneWireHub * hub) final;

    void     clearMemory(void);

    bool     writeMemory(const uint8_t* source, uint8_t length, uint8_t position = 0);
    bool     readMemory(uint8_t* destination, uint8_t length, uint8_t position = 0) const;

    void     setTemperature(float temp_degC);  // can vary from -55 to 125deg
    void     setTemperature(int8_t temp_degC);
    int8_t   getTemperature(void) const;

    void     setVADVoltage(uint16_t voltage_10mV); // unsigned 10 bit
    uint16_t getVADVoltage(void) const;

    void     setVDDVoltage(uint16_t voltage_10mV); // unsigned 10 bit
    uint16_t getVDDVoltage(void) const;

    void     setCurrent(int16_t value);  // signed 11 bit
    int16_t  getCurrent(void) const;
};

#endif
