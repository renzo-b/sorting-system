//################## LIBRARY DECLARATIONS ##################
#include <stdlib.h> // the header of the general purpose standard library of C programming language
#include <avr/io.h>// the header of i/o port
#include <avr/interrupt.h>


//################## FUNCTION DECLARATIONS ##################
void mTimer(int count);
void CheckButtons();
void PauseButton();
void RampDownButton();

//################## GLOBAL VARIABLES ##################
volatile char STATE = 0;
volatile char Pause = 0;
volatile char RampDown = 0;


//################## MAIN ROUTINE ##################
int main(int argc, char *argv[])
{

	// Output Ports
	DDRA = 0b11111111; // Signal to Stepper
	DDRB = 0b10101111; // PWM Output A for Timer/Counter0 or PWM Output B for Timer/Counter1 (bit 7)
	DDRC = 0b11111111; // LEDs display

	// Input Ports
	DDRD = 0b11110000; // External Interrupts 0 to 3 (bits 0 to 3)
	DDRE = 0b00000000; // External Interrupts 7 to 4 (bits 4 to 7)
	DDRF = 0b00000000; // ADC Input Channel 1 (bit 1)

	// Timers
	TCCR1B |=_BV(CS10); // Timer/Counter Control Register B for Timer 1, Chooses no prescaler, the frequency is 8MHz.
	TCCR0B |=_BV(CS01); // Timer/Counter Control Register B for Timer 0 (PWM), Chooses no prescaler, the frequency is 1 MHz.
	TCCR0A |=_BV(WGM00) | _BV(WGM01); // Timer/Counter Control0 Register A for Timer 0 

	// Interrupt Setup
	cli();	// Disables all interrupts
	//EICRA |= _BV(ISC01) | _BV(ISC10) | _BV(ISC21) | _BV(ISC30); // | _BV(ISC31); This gives PORTD0-3 a falling/rising edge

	//EIMSK |= _BV(INT0); // Interrupt Enable PIN D0
	//EIMSK |= _BV(INT1); // Interrupt Enable PIN D1
	//EIMSK |= _BV(INT2); // Interrupt Enable PIN D2
	//EIMSK |= _BV(INT3); // Interrupt Enable PIN D3
	
	EIMSK |= _BV(INT4); // Interrupt Enable PIN E4
	EIMSK |= _BV(INT5); // Interrupt Enable PIN E5

	EICRA |= _BV(ISC41); // Falling edge
	EICRA |= _BV(ISC51); // Falling edge

	sei();	// Note this sets the Global Enable for all interrupts


	while (1)
	{
		switch(STATE)
		{
			case (0) :
				//TurnStepper();
				break;
			case (1) :
				PauseButton();
				break;		
			case (2) :
				RampDownButton();
				break;	
			case (4) :
				//InitializeStepper();
				break; 
			default :
				break;
		}//switch STATE	

		

	}

	return (0); 
}




//################## INTERRUPTS ##################

ISR(INT4_vect) // Pause
{
	STATE = 1;
	//PORTD = 0b11000000;
}

ISR(INT5_vect) // RampDown
{ 
	STATE = 2;
	//PORTD = 0b00110000;
}


ISR(BADISR_vect)
{
	PORTC = 0b11111111;
	PORTD = 0b11110000;	
	mTimer(50000);
}

//################## FUNCTIONS ##################

void PauseButton()
{
	mTimer(100);
	PORTD = 0b11000000;

	if(Pause == 0)
	{
		Pause = 1;
	}
	else
	{
		Pause = 0;
	}

	PORTC = Pause;
	mTimer(100);
	STATE = 0;

}

void RampDownButton()
{
	mTimer(100);
	PORTD = 0b00110000;

	if(RampDown == 0)
	{
		RampDown = 1;
	}
	else
	{
		RampDown = 0;
	}

	PORTC = RampDown<<4;
	mTimer(100);
	STATE = 0;

}
/*
void CheckButtons()
{
		//Add debounce
		if ((0b00010000 & PINE) == 0b00000000){
			mTimer(20);
			buttonPressed = 1;
		} //end if

		if (((0b00010000 & PINE) == 0b00010000) && buttonPressed == 1){
			mTimer(20);
			
			buttonPressed = 0;
			
			if(Pause == 0)
			{
				Pause = 1;
			}

			else
			{
				Pause = 0;
			}
		} //end if

		if ((0b00100000 & PINE) == 0b00000000){
			mTimer(20);
			buttonPressed = 1;
		} //end if

		if (((0b00100000 & PINE) == 0b00100000) && buttonPressed == 1){
			mTimer(20);
			
			buttonPressed = 0;
			
			if(RampDown == 0)
			{
				RampDown = 1;
			}

			else
			{
				RampDown = 0;
			}
		} //end if
}
*/

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

	while(i<count)
	{
		if ((TIFR1 & 0x02) == 0x02)
		{
			TIFR1 |=_BV(OCF1A);
			i++;
		}//end if
	}//end while

}


