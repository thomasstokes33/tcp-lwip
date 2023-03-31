#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

#include "hardware/timer.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

bool adc_initialised = false;

static void init_adc(){
  adc_init();
  adc_set_temp_sensor_enabled(true);
  adc_select_input(4);
  adc_initialised = true;
}

static inline float read_temperature(char unit){

  if(!adc_initialised){
    init_adc();
  }

  // Assuming a max value of 3.3V
  const float conversionFactor = 3.3f / (1 << 12);

  float adc = (float)adc_read() * conversionFactor;
  
  float tempC = 27.0f - (adc - 0.706f) / 0.001721f;
  if(unit == 'C'){
    printf("Temperature: %f C\n", tempC);
    return tempC;
  } else {
    float tempF = tempC * 9 / 5 + 32;
    printf("Temperature: %f F\n", tempF);
    return tempF;
  }
}

static inline uint64_t read_time(){
  uint64_t t = time_us_64();
  printf("Time: %lld\n", t);
  return t;
}

#endif
