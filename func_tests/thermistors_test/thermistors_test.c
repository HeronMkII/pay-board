#ifndef F_CPU
#define F_CPU 8000000UL
#endif

#include <uart/uart.h>
#include <spi/spi.h>
#include <util/delay.h>
#include <adc/adc.h>
#include <adc/pay.h> //Includes constants
#include "../../src/thermistors.h"

#include <conversions/conversions.h>

pin_info_t cs = {
    .port = &ADC_CS_PORT_PAY,
    .ddr = &ADC_CS_DDR_PAY,
    .pin = ADC_CS_PIN_PAY
};

adc_t adc = {
    .channels = 0x0FFF, // TODO: does this value matter?
    .cs = &cs
};

// Get the resistance of the thermistor given the voltage
// For equation, see: https://www.allaboutcircuits.com/projects/measuring-temperature-with-an-ntc-thermistor/
double thermis_voltage_to_resistance(double voltage) {
    return THERMIS_R_REF * (THERMIS_V_REF / voltage - 1);
}

//print temperature results from thermis_temperature array
int print_thermistor_temperatures(double * thermis_temperature, int count) {
    print("\nTemperature results:\n");
    for (uint8_t i = 0; i < count; i ++) {
        print("Channel %d: %d", count, thermis_temperature[i]);
    }
    return 0;
}

int main(void){
    init_uart();
    print("\n\nUART initialized\n");

    init_spi();
    print("SPI Initialized\n");

    init_adc(&adc);
    print("ADC Initialized\n");

    print("\nStarting test\n\n");

    //Find the temperature given the voltage output from the adc
    while (1) {
        //Read all of the thermistors' voltage from adc
        fetch_all(&adc);

        //TODO: ask difference between raw data and raw voltage
        //temperature for each thermistor-channel
        double thermis_temperature[ADC_CHANNELS];

        //Find resistance for each channel
        for (uint8_t i = 0; i < ADC_CHANNELS; i++) {
            uint16_t raw_data = read_channel(&adc, i);
            double voltage = adc_raw_data_to_raw_voltage(raw_data);

            //Convert adc voltage to resistance of thermistor
            double resistance = thermis_voltage_to_resistance(voltage);

            //Convert resistance to temperature of thermistor
            thermis_temperature[i] = thermis_resistance_to_temp(resistance);
        }

        print_thermistor_temperatures(&thermis_temperature, ADC_CHANNELS);
        _delay_ms(10000);
    }

    return 0;
}
