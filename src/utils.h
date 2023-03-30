#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

static inline float read_temperature(char unit){
  printf("Reading temperature");
  // Assuming a max value of 3.3V
  const float conversionFactor = 3.3f / (1 << 12);

  float adc = (float)adc_read() * conversionFactor;
  float tempC = 27.0f - (adc - 0.706f) / 0.001721f;

  if(unit == 'C'){
    return tempC;
  } else {
    return tempC * 9 / 5 + 32;
  }
}

static inline uint64_t read_time(){
  printf("Reading time");
  return 0;
}

#endif
