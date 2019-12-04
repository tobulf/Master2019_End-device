/*
 * power_management.h
 *
 * Created: 25.11.2019 16:29:13
 *  Author: Tobias
 */ 


#ifndef POWER_MANAGEMENT_H_
#define POWER_MANAGEMENT_H_
#include <avr/io.h>


void enable_power_save();
void enable_power_down();
void enable_standby();
void enable_idle();
void enable_adc_NR();



#endif /* POWER_MANAGEMENT_H_ */