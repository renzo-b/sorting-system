//################## LIBRARY DECLARATIONS ##################
#include <stdlib.h> // the header of the general purpose standard library of C programming language
#include <avr/io.h>// the header of i/o port
#include <avr/interrupt.h>


//################## FUNCTION DECLARATIONS ##################
void mTimer(int count);

//################## GLOBAL VARIABLES ##################
volatile unsigned char ADC_result_low;
volatile unsigned char ADC_result_high;
volatile unsigned int ADC_result_flag;

//################## MAIN ROUTINE ##################
int main(int argc, char *argv[]){

// Output Ports
DDRA = 0b11111111; // Signal to Stepper
DDRB = 0b10101111; // PWM Output A for Timer/Counter0 or PWM Output B for Timer/Counter1 (bit 7)
DDRC = 0b11111111; // LEDs display

// Input Ports
DDRD = 0b11110000; // External Interrupts 0 to 3 (bits 0 to 3)
DDRE = 0b00000000; // External Interrupts 7 to 4 (bits 4 to 7)
DDRF = 0b00000000; // ADC Input Channel 0 (bit 0)

// Timers
TCCR1B |=_BV(CS10); // Timer/Counter Control Register B for Timer 1, Chooses no prescaler, the frequency is 8MHz.
TCCR0B |=_BV(CS01); // Timer/Counter Control Register B for Timer 0 (PWM), Chooses no prescaler, the frequency is 1 MHz.
TCCR0A |=_BV(WGM00) | _BV(WGM01); // Timer/Counter Control0 Register A for Timer 0 



// ADC Setup
// by default, the ADC input (analog input is set to be ADC0 / PORTF0)
	ADCSRA |= _BV(ADEN); // enable ADC
	ADCSRA |= _BV(ADIE); // converson complete enable interrupt of ADC
//	ADMUX |= _BV(ADLAR); // ADLAR sets left justified, which is fine for 8 bits. The REFS0 sets voltage reference to AVcc with external capacitor AREF
	ADMUX |= _BV(REFS0);

// Interrupt Setup
cli();	// Disables all interrupts
EICRA |= _BV(ISC01) | _BV(ISC11) | _BV(ISC21) | _BV(ISC30) | _BV(ISC31); // This gives PORTD0-3 a falling edge interrupt sense

EIMSK |= _BV(INT0); // Interrupt Enable PIN D0
EIMSK |= _BV(INT1); // Interrupt Enable PIN D1
EIMSK |= _BV(INT2); // Interrupt Enable PIN D2
EIMSK |= _BV(INT3); // Interrupt Enable PIN D3

sei();	// Note this sets the Global Enable for all interrupts



while(1){


	ADCSRA |= _BV(ADSC);
	while (ADC_result_flag==0)	
	{
	}
	ADC_result_flag = 0x00;
	
	//mTimer(1000);
	PORTC = ADC_result_low;
	PORTD = 0b00000000;
	if(ADC_result_high & 0b00000001 == 0b00000001)
	{
		PORTD |= 0b11000000;
	}
	if(ADC_result_high & 0b00000010 == 0b00000010)
	{
		PORTD |= 0b00110000;
	}



}


return (0); //This line returns a 0 value to the calling program
// generally means no error was returned
}

//################## INTERRUPTS ##################


// the interrupt will be trigured if the ADC is done 
ISR(ADC_vect)
{
	ADC_result_low = ADCL;
	ADC_result_high = ADCH;
	ADC_result_flag = 1;
}


//################## FUNCTIONS ##################

// DESC: initializes a timer lasting COUNT milliseconds
void mTimer(int count)
{
	int i;
	i=0;

	TCCR1B |=_BV(WGM12); // Timer/Counter1 Control Register B set bit WGM12 to 1 which is CTC. CTC is Clear Timer on Compare
	OCR1A=0x03e8; // Output Compare Register is set to 1000 decimal at 1MHz => 1 ms 
	TCNT1=0x0000; // Initial Value of the Timer to 0
	//TIMSK1=TIMSK1 | 0b00000010; // Sets OCIE3A bit to 1 which is a timer output compare A match
	TIFR1 |= _BV(OCF1A);

	while(i<count){
		if ((TIFR1 & 0x02) == 0x02){
		TIFR1 |=_BV(OCF1A);
		i++;
		}
	}

}

