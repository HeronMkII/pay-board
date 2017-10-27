/*

  ADC DEVELOPMENT CODE
  Written by Bruno and Dylan

  ADC7194


  TODO:
    * Decide on a config register default state
      * bit[23] to 0 - chop bit
      * bit[20] set to 0 — ref select
      * bit[18] should be 1 (psuedo bit)
      * bit[16] set to 0 — temp bit
      * bit[15:8] – channel select
      * bit[7] set to 0 – burnout
      * bit[6] set to 0 - ref detect
      * bit[4] set to 0 – buffer, might want to enable
      * bit[3] set to 1 - unipolar or bipolar (1 for unipolar)
      * bit[2:0] set to 000 - programmable gain

      0b0000 0100 0000 0000 0000 1000
      0x040008;
*/


/*
  Datasheet - Important Pages

  19 - register selection

  24 - configuration register - select gain
    - CON2-CON0 - gain select

  32 - overall communication, read/write operations to registers

  33 - single conversion mode

  34 - continuous conversion mode
*/




// TODO - We might want to trigger an interrupt on RDY (falling edge)
// RDY goes low when in continuous conversion mode, CS is low,
// and a new conversion has just completed

#include <stdbool.h>

#include <uart/log.h>
#include "adc.h"

int pga_gain = 1;

void init_adc(){
  // We're assuming that port_expander_init() has already been called.
  int i;

  // Set all the directions needed for sensor board functionality.
  for (i = 0; i < 4; i++){
    set_dir_a(SENSOR_PCB, i, 0);  //Set all the LED outputs
    clear_gpio_a(SENSOR_PCB, i);
  }
  for (i = 0; i < 2; i++){
    set_dir_b(SENSOR_PCB, i, 0); // Set ITF and ADC CS
    set_gpio_b(SENSOR_PCB, i);
  }

  // "Continuous conversion is the default power-up mode." (p. 32)

  // adc_transfer()

  // TODO - maybe write the default configuration register state we want?
}


/*
  Returns the number of BYTES in the specified register at the given address
  TODO - careful of data register + status information - pg. 20
*/
uint8_t num_register_bytes(uint8_t register_addr) {
  switch (register_addr) {
    case STATUS_ADDR:
      return 1;
      break;
    case MODE_ADDR:
      return 3;
      break;
    case CONFIG_ADDR:
      return 3;
      break;
    // TODO - could be 3 or 4? check datasheet (p.20)
    case DATA_ADDR:
      return 3;
      break;
    case ID_ADDR:
      return 1;
      break;
    case GPOCON_ADDR:
      return 1;
      break;
    case OFFSET_ADDR:
      return 3;
      break;
    case FULL_SCALE_ADDR:
      return 3;
      break;
    default:
      return 0;
      break;
  }
}


// TODO - check if a register if read-only or write-only?

/*
  Reads the current state of the specified ADC register.
*/
uint32_t read_ADC_register(uint8_t register_addr) {
  // Set CS low
  clear_gpio_b(SENSOR_PCB, ADC_CS);

  // Write the communication register byte to read from the register (p. 19)
  send_spi(COMM_BYTE_READ | (register_addr << 3));

  // Read the required number of bytes
  uint32_t data = 0;
  for (int i = 0; i < num_register_bytes(register_addr); i++) {
    data = data << 8;
    data = data | send_spi(0);
  }

  // Set CS high
  set_gpio_b(SENSOR_PCB, ADC_CS);

  return data;
}


/*
  Writes a new state to the specified ADC register.
*/
void write_ADC_register(uint8_t register_addr, uint32_t data) {
  // Set CS low
  clear_gpio_b(SENSOR_PCB, ADC_CS);

  // Write the communication register byte
  // to write to the register (p.19)
  send_spi(COMM_BYTE_WRITE | (register_addr << 3));

  // Write the required number of bytes
  for (int i = num_register_bytes(register_addr) - 1; i >= 0; i--) {
    send_spi( (uint8_t) (data >> (i * 8)) );
  }

  // Set CS high
  set_gpio_b(SENSOR_PCB, ADC_CS);
}


/*
  Sets the configuration register's bits for the specified ADC input channel.
  channel_num - one of 5, 6, 7
*/
void select_ADC_channel(uint8_t channel_num) {
  // Get the 4 bits for the channel (p. 26)
  uint8_t channel_bits = channel_num - 1;

  // Read from configuration register
  uint32_t config_data = read_ADC_register(CONFIG_ADDR);


  // Mask configuration bits to set the channel

  // TODO - is there an error in the data sheet (p. 25)?
  // CON16-CON8 -> should be CON15-CON8

  // Clear bits 15-12 (CH7-4, set to 0) (p.25)
  for (int i = 15; i >= 12; i--) {
    // TODO - refactor this into a function
    config_data = config_data & ~(1 << i);
  }

  // Set bits 15-12 to the appropriate gain
  config_data = config_data | (channel_bits << 12);


  // Write to configuration register
  write_ADC_register(CONFIG_ADDR, config_data);
}


/*
  Reads 24 bit raw data from the specified ADC channel.
  channel_num - one of 5, 6, 7
*/
uint32_t read_ADC_channel_raw_data(uint8_t channel_num) {
  select_ADC_channel(channel_num);

  // TODO - is it bad to block like this?
  while (true) {
    uint32_t status_data = read_ADC_register(STATUS_ADDR);
    // Break and continue function execution when RDY (bit 7) is 0 (p. 20)
    if ((status_data & (1 << 7)) == 0) {
      break;
    } else {
      print("Conversion not ready yet\n");
    }
  }

  // Read and return 24 bit data
  uint32_t read_data = read_ADC_register(DATA_ADDR);
  return read_data;
}


/*
  Reads the input voltage for the specified ADC channel,
  including applying the gain factor.
  channel_num - one of 5, 6, 7
*/
double read_ADC_channel(uint8_t channel_num) {
  // Read 24 bit raw data
  uint32_t read_data = read_ADC_channel_raw_data(channel_num);

  print("Read Data (Channel %d)\n", channel_num);
  print("%X = %d\n", read_data, read_data); // hexadecimal and decimal
  print("Gain = %d\n", pga_gain);

  // (p.31) Code = (2^N * AIN * Gain) / (V_REF)
  //     => AIN = (Code * V_REF) / (2^N * Gain)
  double AIN =  ((double) ( read_data * V_REF               )) /
                ((double) ( ((uint32_t) 1 << N) * pga_gain  ));
  print("AIN = %f\n", AIN);

  return AIN;
}


/*
  Converts the numerical gain to 3 gain select bits (p.25).
  gain - one of 1, 8, 16, 32, 64, 128
*/
uint8_t convert_gain_bits(uint8_t gain) {
  // Add breaks just in case
  switch (gain) {
    case 1:
      return 0b000;
      break;
    case 8:
      return 0b011;
      break;
    case 16:
      return 0b100;
      break;
    case 32:
      return 0b101;
      break;
    case 64:
      return 0b110;
      break;
    case 128:
      return 0b111;
      break;
    default:
      return 0b000;
      break;
  }
}


/*
  Sets the configuration register's bits for a specified programmable gain.
  gain - one of 1, 8, 16, 32, 64, 128 (2 and 4 are reserved/unavailable, p. 25)
*/
void set_PGA(uint8_t gain) {
  // Convert gain to 3 bits
  uint8_t gain_bits = convert_gain_bits(gain);

  // Read from configuration register
  uint32_t config_data = read_ADC_register(CONFIG_ADDR);


  // Mask configuration bits to set the gain

  // Clear bits 2-0 (set to 0)
  for (int i = 2; i >= 0; i--) {
    config_data = config_data & ~(1 << i);
  }

  // Set bits 2-0 to the appropriate gain
  config_data = config_data | gain_bits;


  // Write to configuration register
  write_ADC_register(CONFIG_ADDR, config_data);

  // Keep track of the set gain
  pga_gain = gain;
}