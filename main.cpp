/*
 * Master-node.cpp
 *
 * Created: 28.08.2019 12:08:38
 * Author : Tobias
 */ 

#include <avr/interrupt.h> 
#include <util/delay.h>
#include <avr/io.h>
#include <util/delay_basic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include "drivers/power_management.h"
#include "drivers/Timer.h"
#include "drivers/RN2483.h"
#include "drivers/RTC.h"
#include "drivers/LED_driver.h"
#include "drivers/LoRa_cfg.h"
#include "drivers/ADC.h"
#include "drivers/WString.h"
#include "drivers/WDT.h"
#include "drivers/EEPROM.h"
#include "drivers/i2cmaster.h"
#include "drivers/mpu6050.h"
#include "drivers/EEPROM.h"
#include "drivers/MemoryAdresses.h"
#include "drivers/interrupt_button.h"



/* Since FILES, and FDEV doesn't work in C++, a workaround had to be made to enable printf:
   This is considered a bug by the WinAvr team, however has not been fixed.
*/

extern "C" {
	#include "drivers/Debug.h"
};
enum MESSAGE_PORTS {SYNC = 1, EVENT = 2, APPEND_DATA=4, KEEP_ALIVE=8};
enum STATE {IDLE, ALIVE_TRANSMIT, DATA_TRANSMIT, SLEEP};
STATE cur_state;
STATE prev_state;

bool joined = false;
bool gyro_data = false;
bool fifo_started = false;
bool dummy_msg = false;
bool sent = false;

uint8_t radio_buf[70];
uint8_t* downlink_buf;
int16_t x;
int16_t y;
int16_t z;
int16_t temperature;
uint32_t timestamp;
uint32_t alive_timestamp;

adc AnalogIn;
LED_driver Leds;
RN2483 radio;
RTC rtc;
Timer timer;

int main (void){
	sei();
	wdt_set_to_16s();
	wdt_INT_enable();
	wdt_reset();
	Leds.toogle(GREEN);
	USART_init();
	radio.print_dev_eui();
	interrupt_button_init();
	WDT_reset();
	while (!joined){
		joined = radio.init_OTAA(appEui,appKey,devEui);
		WDT_reset();
	}
	Leds.toogle(GREEN);
	Leds.toogle(RED);
	radio.set_duty_cycle(0);
	radio.set_DR(0);
	radio.set_RX_window_size(1000);
	radio.sleep();
	WDT_reset();
	mpu6050_reset();
	mpu6050_init();
	mpu6050_normalPower_mode();
	mpu6050_set_sensitivity(TWO_G);
	WDT_reset();
	mpu6050_set_interrupt_mot_thrshld(250);
	mpu6050_get_interrupt_status();
	mpu6050_enable_motion_interrupt();
	mpu6050_enable_pin_interrupt();
	mpu6050_lowPower_mode();
	//mpu6050_reset();
	WDT_reset();
	interrupt_button_enable();
	rtc.set_alarm_period(900);
	rtc.start_alarm();
	cur_state = ALIVE_TRANSMIT;
	Leds.toogle(RED);
	WDT_reset();
	while (true){
		switch (cur_state){
			case DATA_TRANSMIT:
				WDT_reset();
				wdt_set_to_24s();
				Leds.reset();
				Leds.turn_on(YELLOW);
				Leds.turn_on(GREEN);
				gyro_data = false;
				//Send the data:
				Leds.turn_on(YELLOW);
				AnalogIn.enable();
				radio.wake();
				mpu6050_tempSensorEnabled();
				mpu6050_getConvTempData(&temperature);
				mpu6050_tempSensorDisabled();
				uint16_t size;
				mpu6050_get_FIFO_length(&size);
				//Read and send max 960 bytes of data: 8s recording.
				radio_buf[0]=(uint8_t)((timestamp>>24) & 0xFF);
				radio_buf[1]=(uint8_t)((timestamp>>16) & 0xFF);
				radio_buf[2]=(uint8_t)((timestamp>>8) & 0xFF);
				radio_buf[3]=(uint8_t)(timestamp & 0xFF);
				radio_buf[4] = AnalogIn.get_battery_lvl();
				radio_buf[5] = AnalogIn.get_light_lvl();
				radio_buf[6] = (uint8_t)temperature;
				AnalogIn.disable();
				sent = false;
				WDT_off();
				while (!sent){
					sent = radio.TX_bytes(radio_buf, 7, EVENT);
				}
				mpu6050_get_FIFO_length(&size);
				while(size>60){
					sent = false;
					for (uint8_t i = 0; i<=47;i = i + 6){
						mpu6050_FIFO_pop(&x, &y, &z);
						radio_buf[i]=(uint8_t)((x>>8) & 0xFF);
						radio_buf[i+1]=(uint8_t)(x & 0xFF);
						radio_buf[i+2]=(uint8_t)((y>>8) & 0xFF);
						radio_buf[i+3]=(uint8_t)(y & 0xFF);
						radio_buf[i+4]=(uint8_t)((z>>8) & 0xFF);
						radio_buf[i+5]=(uint8_t)(z & 0xFF);
					}
					while (!sent){
						sent = radio.TX_bytes(radio_buf, 48 , APPEND_DATA);
					}
					mpu6050_get_FIFO_length(&size);
				}
				wdt_set_to_8s();
				radio.sleep();
				WDT_reset();
				mpu6050_FIFO_reset();
				mpu6050_enable_motion_interrupt();
				mpu6050_enable_pin_interrupt();
				mpu6050_lowPower_mode();
				WDT_reset();
				cur_state = IDLE;
				break;
			
				
			case ALIVE_TRANSMIT:
				WDT_reset();
				wdt_set_to_24s();
				dummy_msg = false;
				Leds.reset();
				Leds.turn_on(RED);
				Leds.turn_on(GREEN);
				mpu6050_disable_pin_interrupt();
				AnalogIn.enable();
				radio.wake();
				mpu6050_normalPower_mode();
				mpu6050_tempSensorEnabled();
				mpu6050_getConvTempData(&temperature);
				mpu6050_tempSensorDisabled();
				mpu6050_getConvAccData(&x,&y,&z);
				alive_timestamp = rtc.get_epoch();
				radio_buf[0]=(uint8_t)((alive_timestamp>>24) & 0xFF);
				radio_buf[1]=(uint8_t)((alive_timestamp>>16) & 0xFF);
				radio_buf[2]=(uint8_t)((alive_timestamp>>8) & 0xFF);
				radio_buf[3]=(uint8_t)(alive_timestamp & 0xFF);
				radio_buf[4] = AnalogIn.get_battery_lvl();
				radio_buf[5] = AnalogIn.get_light_lvl();
				radio_buf[6] = (uint8_t)temperature;
				radio_buf[7]=(uint8_t)((x>>8) & 0xFF);
				radio_buf[8]=(uint8_t)(x & 0xFF);
				radio_buf[9]=(uint8_t)((y>>8) & 0xFF);
				radio_buf[10]=(uint8_t)(y & 0xFF);
				radio_buf[11]=(uint8_t)((z>>8) & 0xFF);
				radio_buf[12]=(uint8_t)(z & 0xFF);
				AnalogIn.disable();
				sent = false;
				// Set DR to 5, try to send on the highest and then decrease if fail.
				WDT_off();
				for (uint8_t DR = 6; DR > 0; DR--){
					//radio.set_DR(DR-1);
					for (uint8_t i = 0; i<3;i++){
						sent = radio.TX_bytes(radio_buf, 13, KEEP_ALIVE);
						if (sent){break;}
					}
					if (sent){break;}
				}
				wdt_set_to_8s();
				radio.sleep();
				mpu6050_enable_motion_interrupt();
				mpu6050_enable_pin_interrupt();
				mpu6050_lowPower_mode();
				interrupt_button_enable();
				WDT_reset();
				cur_state = IDLE;
				break;
				
			case IDLE:
				wdt_set_to_8s();
				if(gyro_data){
					cur_state = DATA_TRANSMIT;
				}
				else if (rtc.get_alarm_status() || dummy_msg){
					cur_state = ALIVE_TRANSMIT;
				}
				else if (radio.unread_downlink()){
					mpu6050_disable_pin_interrupt();
					mpu6050_normalPower_mode();
					downlink_buf = radio.read_downlink_buf();
					uint8_t port = radio.get_downlink_port();
					if (port == 3){
						printf("port %d PL %d\n", port, downlink_buf[0]);
						mpu6050_set_interrupt_mot_thrshld(downlink_buf[0]);
					}
					else if (port == 5){
						printf("Config received on port %d\n", port);
						switch (downlink_buf[0])
						{
						case TWO_G:
							mpu6050_set_sensitivity(TWO_G);
							printf("Sensitivity set to 2G\n");
							break;
						case FOUR_G:
							mpu6050_set_sensitivity(FOUR_G);
							printf("Sensitivity set to 4G\n");
							break;
						case EIGHT_G:
							mpu6050_set_sensitivity(EIGHT_G);
							printf("Sensitivity set to 8G\n");
							break;
						case SIXTEEN_G:
							mpu6050_set_sensitivity(SIXTEEN_G);
							printf("Sensitivity set to 16G\n");
							break;
						default:
							break;
						}
						
					}
					mpu6050_lowPower_mode();
					mpu6050_enable_pin_interrupt();
				}
				else{
					cur_state = SLEEP;
					WDT_reset();
					WDT_off();
				}
				Leds.reset();
				break;
				
			case SLEEP:
				enable_power_down();
				sleep_enable();
				sleep_mode();
				cur_state = IDLE;
				break;
				
			default:
				printf("Something went wrong... \n");
				break;
		}
	}
}
	

ISR(INT0_vect){
	cli();
	mpu6050_disable_pin_interrupt();
	sei();
	sleep_disable();
	mpu6050_FIFO_stop();
	mpu6050_disable_interrupt();
	Leds.turn_on(YELLOW);
	uint8_t interrupt = mpu6050_get_interrupt_status();
	if ((interrupt & (1 << 6)) && !fifo_started){
		fifo_started = true;
		timestamp = rtc.get_epoch();
		mpu6050_normalPower_mode();
		mpu6050_FIFO_reset();
		mpu6050_FIFO_enable();
		mpu6050_enable_FIFO_OVF_interrupt();
		mpu6050_enable_pin_interrupt();
	}
	else {
		fifo_started = false;
		gyro_data = true;
	}
};

ISR(INT1_vect){
	cli();
	dummy_msg = true;
	interrupt_button_disable();
	sei();
};


