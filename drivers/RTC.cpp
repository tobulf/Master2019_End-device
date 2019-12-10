/*
 * RTC.cpp
 *
 * Created: 11.09.2019 11:16:19
 *  Author: Tobias
 */ 

#include "RTC.h"


uint32_t seconds = 10000;
uint16_t millis = 0;
uint16_t millis_error = 0;
uint16_t micros = 0;
uint8_t OVF1 = 0;
uint8_t cnt1 = 0;
bool OVF2 = false;


RTC::RTC(){
	/* initialize timer 1 */
	TIMSK0 |= (1 << TOIE0);
	TCCR0B |= (1 << CS00);
	TCCR0B &= ~((0 << CS02) | (0 << CS01));
	/*Initialize timer 2*/
	ASSR |= (1 << AS2);
	TCCR2A = 0;
	TIMSK2 |= 0b00000001;
	TCCR2B = 0b00000100;  
	
	time = 1000;
};

void RTC::set_time(uint64_t epoch){
	
};

uint32_t RTC::get_epoch(void){
	return ((seconds*1000)+millis);
};


ISR(TIMER0_OVF_vect){
	cli();
	OVF1 ++;
	if (OVF1 == 7){
		millis++;
		micros = 0;
		OVF1 = 0;
		cnt1++;
		if (cnt1 == 52){
			millis--;
			cnt1=0;
		}
	}
	else  {
		micros = micros + 139;
	}
	sei();
};

ISR(TIMER2_OVF_vect){
	cli();
	if (OVF2){
		seconds++;
		millis = 0;
		micros = 0;
		OVF1 = 0;
		cnt1 = 0;
		OVF2 = false;
		
	}
	else {
		OVF2 = true;
		}
	sei();
};