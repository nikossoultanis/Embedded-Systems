#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <header.h>

#define PIOA_ID 2
#define TC0_ID 17

void FIQ_handler(void);

PIO* pioa = NULL;
AIC* aic = NULL;
TC* tc = NULL;

#define BUT_IDLE 0
#define BUT_PRESSED 1
#define LEFT 0
#define RIGHT 1


unsigned int PIOA_int, TC_int; 
unsigned int Channel_0_CCR;
unsigned int previous_button_state = BUT_IDLE;
unsigned long Channel_0_RC;
unsigned int clk_pulse_counter;

unsigned int b1 = 0;    // Player 1 button - normal
unsigned int b1_st = 0; // -//- previous state
unsigned int b1n = 0;   // Player 1 button - nitro
unsigned int b1n_st = 0;// -//- previous state
unsigned int p1_scored = 0; // set to 6 if Player 1 Scored, counts led flashes for p1
unsigned int sc1 = 0;   // Player 1 score
  
unsigned int b2 =0;    // Player 2 button - normal
unsigned int b2_st = 0; // -//- previous state
unsigned int b2n = 0;   // Player 2 button - nitro
unsigned int b2n_st = 0;// -//- previous state
unsigned int p2_scored = 0; // set to 6 if Player 2 scored, counts led flashes for p2
unsigned int sc2 = 0;   // Player 2 score

unsigned int GAME_RUNNING = 0;  // is game running
unsigned int NITRO = 0; // is nitro active
unsigned int LED_STATE = 0; // saves which LED is on
unsigned int STEP = 0;      // if nitro is off, set to 1 to get to next led
unsigned int DIRECTION = 1; // Ball direction, 0 is left, 1 is right

int interrupts = 0;  // counts TC_INT interrupts, 5 = 1sec

int main( int argc, const char* argv[] ){

	unsigned int gen;
	STARTUP;
	tc->Channel_0.RC  = 1638;      // 200 millisec timer
	tc->Channel_0.CMR = 2084;
	tc->Channel_0.IDR = 0xFF;
	tc->Channel_0.IER = 0x10;
	tc->Channel_0.CCR = 0x02;   // stop timer
	aic->FFER = (1<<PIOA_ID) | (1<<TC0_ID);
	aic->IECR = (1<<PIOA_ID) | (1<<TC0_ID);
	pioa->PUER = 0x3003;    // pull-up enabled
	pioa->ODR = 0x3003;     // pin 13, 12, 1, 0 as input, pull-up enabled
	pioa->CODR = 0xFFC;    // clear output pins 2 to 11
	pioa->OER = 0xFFC;     // enable pin 2 to 11 as outputs
	gen = pioa->ISR; 
	pioa->PER = 0x3FFF;    // enable PIOA pins 0-13 
	gen = tc->Channel_0.SR;
	aic->ICCR = (1<<PIOA_ID)|(1<<TC0_ID); 
	pioa->IER = 0x3003;    // pins 12, 13, 0, 1 cause an interrupt
	while( (tmp = getchar()) != ’e’)
	{
		if (sc1 == 3 || sc2 == 3)
		{
			GAME_RUNNING = 0;
			Channel_0_CCR = 0x02;
		}
	}
	aic->IDCR = (1<<PIOA_ID) | (1<<TC0_ID); 
	tc->Channel_0.CCR = 0x02; 
	CLEANUP;
	return 0;
}

void FIQ_handler()
{
	unsigned int data_in = 0;
	unsigned int fiq = 0;
	unsigned int data_out;
	fiq = aic->IPR;

	if (fiq & (1 << PIOA_ID) ) { 
		data_in = pioa->ISR;
		aic->ICCR = ( 1 << PIOA_ID );
		data_in = pioa->PDSR;
		if ( data_in & 0x3003 ) {
			if (GAME_RUNNING == 0) {
				GAME_RUNNING = 1;
				pioa->CODR = data_out & 0x00; // turn off all leds;
				pioa->SODR = data_out | 0x40; // turn on middle led;
				Channel_0_CCR = 0x05;         // start timer
			}
		}

		if ( data_in & 0x01 ) { 
			if (b1 == BUT_IDLE) {
				b1 = BUT_PRESSED;
				if (data_in & 0x04) {
					DIRECTION = 0;
					NITRO = 0;
				}
		    } else if (b1 == BUT_PRESSED) { b1 = BUT_IDLE; }

        } else if ( data_in & 0x02 ) {
			if (b1n == BUT_IDLE) {
			    b1n = BUT_PRESSED;
				if (data_in & 0x04) {
					DIRECTION = 0;
					NITRO = 1;
				}
			} else if (b1n == BUT_PRESSED) { b1n = BUT_IDLE; }

		} else if ( data_in & 0x2000 ) { 
			if (b2 == BUT_IDLE) {
				b2 = BUT_PRESSED;
				if (data_in & 0x800) {
					DIRECTION = 1;
					NITRO = 0;
				}
		    } else if (b2 == BUT_PRESSED) { b2 = BUT_IDLE; }
		
        } else if ( data_in & 0x1000 ) {
			if (b2n == BUT_IDLE) {
			    b2n = BUT_PRESSED;
				if (data_in & 0x800) {
					DIRECTION = 1;
					NITRO = 1;
				}
			} else if (b2n == BUT_PRESSED) { b2n = BUT_IDLE; }
		} 
	}
	
	// TIMER INTERRUPT
 	if( fiq & (1<<TC0_ID) ){
		data_out = tc->Channel_0.SR; 
		aic->ICCR = (1<<TC0_ID);

		data_in = PIOA_ODSR;  // Read LED Status - Which LED is on
		
		if (p1_scored) {
			p1_scored--;
			pioa->SODR = data_in ^ 0x04;      // toggle rightmost led;	
			if (p1_scored == 0) {
				pioa->CODR = data_in & 0x0000;         // turn off all leds;
				pioa->SODR = data_in & 0x04;           // turn on rightmost led;
				DIRECTION = LEFT;
				if (sc1 == 3){
					Channel_0_CCR = 0x02;
					GAME_RUNNING = 0;
				}
			}
			return;
		} 
		if (p2_scored) {
			p2_scored--;
			pioa->SODR = data_in ^ 0x800;      // toggle rightmost led;	
			if (p2_scored == 0) {
				pioa->CODR = data_in & 0x000000000000; // turn off all leds;
				pioa->SODR = data_in & 0x800;           // turn on rightmost led;
				DIRECTION = RIGHT;
				if (sc2 == 3){
					Channel_0_CCR = 0x02;
					GAME_RUNNING = 0;
				}
			}
			return;
		} 
    
		if (NITRO == 0) {
			interrupts++;
			if (interrupts > 5) {
				interrupts = 0;
				STEP = 1;
			}
		}
		if (NITRO || STEP) {
			STEP = 0;
			if ( (data_in & 0x04) && (DIRECTION == RIGHT) ) {
				sc2++;
				p2_scored = 6;
			} else if ( (data_in & 0x800) && (DIRECTION == LEFT) ) {
				sc1++;
				p1_scored = 6;
			} else {
				if (DIRECTION == RIGHT) {
					pioa->CODR = data_in & 0x0000;
					pioa->SODR = ( data_in >> 1 );
				} else {
					pioa->CODR = data_in & 0x0000;
					pioa->SODR = ( data_in << 1 );
				}
			}
		}
	}
}
