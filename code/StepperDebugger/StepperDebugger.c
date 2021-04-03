//################## LIBRARY DECLARATIONS ##################
#include <stdlib.h> // the header of the general purpose standard library of C programming language
#include <avr/io.h>// the header of i/o port
#include <avr/interrupt.h>
#include "LinkedQueue.h" 

//################## FUNCTION DECLARATIONS ##################
void mTimer(int count);
void InitializeStepper();
void MoveStepper(int steps, int direction, int delay);
void InitializeStepper();
void MoveStepper90cw();
void MoveStepper90ccw();
void MoveStepper180cw();
void TurnStepper();
void PauseButton();
void RampDownButton();


//################## GLOBAL VARIABLES ##################
volatile char STATE = 4;
char Motor_array[4] = {0b00000000,0b00000100,0b00001000,0b00001111};	// Array of status of DC motor: Brake high, CW, CCW, Cut power
//char Position_array[4] = {0b11000000,0b00011000,0b10100000,0b00010100};	// Array of the possible stepper positions
char Position_array[4] = {0b11011000,0b10111000,0b10110100,0b11010100};	// Array of the possible stepper positions
volatile int Last_position = 0;
volatile unsigned int ADC_result_flag;
volatile unsigned char PartPastSensor = 0x01;
volatile unsigned int Lowest_value_low = 255;
volatile unsigned int Lowest_value_high = 255;
volatile unsigned int Lowest_value = 1023;
volatile unsigned char StepperInitialized = 0x00;
volatile unsigned char StepperCurrentPosition = 0x00;
volatile unsigned char StepperDesiredPosition = 0x00;
link *head;			 // The ptr to the head of the queue 
link *tail;			 // The ptr to the tail of the queue 
link *newLink;		 // A ptr to a link aggregate data type (struct) 
link *rtnLink;		 // same as the above 
volatile unsigned char SamePart = 0;
volatile signed int Dequeue_counter = 0;
volatile signed int Exit_counter = 0;
volatile unsigned char PartPastExit = 1;
char Delay_array[16] = {17,17,16,16,15,14,13,12,11,10,9,8,8,7,7,6};
volatile char DebugLED = 0;
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

	// Linked List Variables
	//link *head;			 // The ptr to the head of the queue 
	//link *tail;			 // The ptr to the tail of the queue 
	//link *newLink;		 // A ptr to a link aggregate data type (struct) 
	//link *rtnLink;		 // same as the above 
	setup(&head, &tail); // Initializes head and tail
	//initLink(&newLink);
	//newLink->e.itemCode = 0;
	//enqueue(&head, &tail, &newLink);

	// DC Motor Setup
	TCCR0A |=_BV(COM0A1);// Set compare output mode for fast PWM to clear OC0A on compare match, set OC0A at TOP
	PORTB |= 0b00000100;
	OCR0A = 60;

	// ADC Setup
	// by default, the ADC input (analog input is set to be ADC0 / PORTF0)
	ADCSRA |= _BV(ADEN); // enable ADC
	ADCSRA |= _BV(ADIE); // converson complete enable interrupt of ADC
	//ADMUX |= _BV(ADLAR) | _BV(REFS0); // ADLAR sets left justified, which is fine for 8 bits. The REFS0 sets voltage reference to AVcc with external capacitor AREF
	//ADCSRA |= _BV(ADPS0);	pre-scaler = 1/8
	//ADCSRA |= _BV(ADPS1); 
	ADMUX |= _BV(REFS0) | _BV(MUX0);

	// Interrupt Setup
	cli();	// Disables all interrupts
	EICRA |= _BV(ISC01) | _BV(ISC10) | _BV(ISC21) | _BV(ISC30); // | _BV(ISC31); This gives PORTD0-3 a falling/rising edge

	//EIMSK |= _BV(INT0); // Interrupt Enable PIN D0
	EIMSK |= _BV(INT1); // Interrupt Enable PIN D1
	EIMSK |= _BV(INT2); // Interrupt Enable PIN D2
	EIMSK |= _BV(INT3); // Interrupt Enable PIN D3

	sei();	// Note this sets the Global Enable for all interrupts



	while (1)
	{
	
		switch(STATE)
		{
			case (0) :
				//TurnStepper();
				mTimer(20);
				MoveStepperRamp(50,0);
				mTimer(200);
				MoveStepperRamp(50,1);
				mTimer(200);
				MoveStepperRamp(50,0);
				mTimer(200);
				MoveStepperRamp(50,1);
				mTimer(200);
				MoveStepperRamp(50,0);
				mTimer(200);

				break;	
			case (1) :
				PauseButton();
				break;
			case (2) :
				RampDownButton();
			break;
			case (4) :
				InitializeStepper();
				break; 
			default :
				break;
		}//switch STATE	

	}


	return (0); //This line returns a 0 value to the calling program
	// generally means no error was returned
}




//################## INTERRUPTS ##################

ISR(INT0_vect)// OI
{ 
	
	STATE = 0;
}

ISR(INT1_vect)// EX
{ 
	if (PartPastExit == 1)
	{

		if(StepperDesiredPosition != StepperCurrentPosition)
		{
			PORTB = 0b00000000;
		}
		PartPastExit =0;
	}
	else
	{
		PartPastExit = 1;
		SamePart =0;
		Exit_counter++;
	//	PORTC = Exit_counter;
	}
	//PORTC = PartPastExit;

}

ISR(INT2_vect)// Hall Effect
{ 
	StepperInitialized = 1;
	StepperCurrentPosition = 0;
	STATE = 0;
}

ISR(INT3_vect)// OR + RL
{ 

	if(PartPastSensor == 1)
	{
		PartPastSensor = 0;
		Lowest_value = 1023;
		ADCSRA |= _BV(ADSC); // Starts Conversion				
	}//end if
	else
	{
		PartPastSensor = 1;
	}//end else
}


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



// the interrupt will be trigured if the ADC is done 
ISR(ADC_vect)
{
	if ( ADC < Lowest_value) 
	{
	
		Lowest_value = ADC;	
		Lowest_value_low = ADCL;
		Lowest_value_high = ADCH;		
	}//end if	
	

	if (PartPastSensor == 0)
	{
		ADCSRA |= _BV(ADSC);
	}//end if	
	else
	{
		PORTC = Lowest_value_low; // Display ADC result on LEDs
		PORTD = Lowest_value_high<<5;
			
		initLink(&newLink);
		newLink->e.itemCode = Lowest_value;
		enqueue(&head, &tail, &newLink);
	}//end else		
}

ISR(BADISR_vect)
{
	PORTC = 0b11111111;
	PORTD = 0b11110000;	
	mTimer(50000);
}

//################## FUNCTIONS ##################

// DESC: Turns the stepper to the necessary position
void TurnStepper()
{
	if (Exit_counter > Dequeue_counter)
	{
		Exit_counter = Dequeue_counter;
		SamePart = 0;
		StepperDesiredPosition = StepperCurrentPosition;
		//PORTD = 0b11110000;
	}


	if ((StepperDesiredPosition == StepperCurrentPosition) && (SamePart == 0) && (Dequeue_counter == Exit_counter)) 
	{
		dequeue(&head, &tail, &rtnLink);
		
		
		
		int temp = rtnLink->e.itemCode;
	
		if (temp > 970) 
		{
			StepperDesiredPosition = 0;
			Dequeue_counter++;
		
			if ((StepperDesiredPosition == StepperCurrentPosition))
			{
				SamePart = 1;
			}//end if

		}//end if
		else if ((temp <= 970) && (temp > 850)) 
		{
			StepperDesiredPosition = 2;
			Dequeue_counter++;
		
			if ((StepperDesiredPosition == StepperCurrentPosition))
			{
				SamePart = 1;
			}//end if

		}//end else if
		else if ((temp <= 850) && (temp > 300))
		{
		
			StepperDesiredPosition = 1;
			Dequeue_counter++;
		
			if ((StepperDesiredPosition == StepperCurrentPosition))
			{
				SamePart = 1;
			}//end if

		}//end else if
		else if ((temp <= 300) && (temp > 10))
		{
		
			StepperDesiredPosition = 3;
			Dequeue_counter++;
		
			if ((StepperDesiredPosition == StepperCurrentPosition))
			{
				SamePart = 1;
			}//end if

		}//end else if

		

	}//end if

	

	if ((Dequeue_counter - Exit_counter) == 1)
	{
	
		if ((StepperDesiredPosition-StepperCurrentPosition == 1) | (StepperDesiredPosition-StepperCurrentPosition == -3))
		{		
			MoveStepper90cw();
			StepperCurrentPosition = StepperDesiredPosition;
			PORTB = 0b00000100;
		}//end if
		else if ((StepperDesiredPosition-StepperCurrentPosition == 2) | (StepperDesiredPosition-StepperCurrentPosition == -2))
		{
			MoveStepper180cw();
			StepperCurrentPosition = StepperDesiredPosition;
			PORTB = 0b00000100;
		}//end else if
		else if ((StepperDesiredPosition-StepperCurrentPosition == 3) | (StepperDesiredPosition-StepperCurrentPosition == -1))	
		{
			MoveStepper90ccw();
			StepperCurrentPosition = StepperDesiredPosition;
			PORTB = 0b00000100;
		}//end else if
	}// end if


}	


void MoveStepperRamp(int steps, int direction){

	int i = Last_position;
	int delay = 6;


	if (direction ==0 ) { // clockwise

		for (int j=0;j<16;j++){
				i++;
				// Goes through the step positions in the array in increasing order
				if(i>3) i=0;
			
				PORTA = Position_array[i];
				mTimer(Delay_array[j]); 

		}// end for
		
		
		for (int j=0;j<(steps-32);j++){
				i++;
				// Goes through the step positions in the array in increasing order
				if(i>3) i=0;
			
				PORTA = Position_array[i];
				mTimer(delay); 

		}// end for

		for (int j=0;j<16;j++){
				i++;
				// Goes through the step positions in the array in increasing order
				if(i>3) i=0;
			
				PORTA = Position_array[i];
				mTimer(Delay_array[15-j]); 

		}// end for
	} // end if

	
	else {	// counter clockwise
		
		// Goes through the step positions in the array in reverse order
		
		
		for (int j=0;j<16;j++){
			i--;
			// Goes through the step positions in the array in increasing order
			if(i<0) i=3;
			
			PORTA = Position_array[i];
			mTimer(Delay_array[j]); 

		}// end for

		for (int j=0;j<(steps-32);j++){
			i--;
			// Goes through the step positions in the array in increasing order
			if(i<0) i=3;
			
			PORTA = Position_array[i];
			mTimer(delay); 

		}// end for

		
		for (int j=0;j<16;j++){
			i--;
			// Goes through the step positions in the array in increasing order
			if(i<0) i=3;
			
			PORTA = Position_array[i];
			mTimer(Delay_array[15-j]); 

		}// end for
	} // end else

	Last_position = i;
}



// DESC: Rotates the stepper 90 degrees CW
void MoveStepper90cw()
{
	MoveStepperRamp(50, 1);
	mTimer(5); 
}

// DESC: Rotates the stepper 90 degrees CCW
void MoveStepper90ccw()
{
	MoveStepperRamp(50, 0);
	mTimer(5); 
}

// DESC: Rotates the stepper 180 degrees CW
void MoveStepper180cw()
{
	MoveStepperRamp(100, 1);
	mTimer(5); 
}

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


void InitializeStepper()
{
	int j=16;
	int i=0;	// Assumed current step is 0
	while(StepperInitialized == 0 ) 
	{
		i++;
		// Goes through the step positions in the array in increasing order
		if(i>3) i=0;
		PORTA = Position_array[i];
		mTimer(j);
		if (j>7)
		{
		j--;
		} 
		Last_position =i;
	}//end while 
	

}

void MoveStepper(int steps, int direction, int delay){

	int i = Last_position;

	if (direction ==0 ) { // clockwise
		
		for (int j=0;j<steps;j++){
			i++;
			// Goes through the step positions in the array in increasing order
			if(i>3) i=0;
			
			PORTA = Position_array[i];
			mTimer(delay); 
			//PORTC = Position_array[i];
		}// end for
	} // end if

	
	else {	// counter clockwise
		
		// Goes through the step positions in the array in reverse order
		for (int j=0;j<steps;j++){
			i--;
			// Goes through the step positions in the array in increasing order
			if(i<0) i=3;
			
			PORTA = Position_array[i];
			mTimer(delay); 
			//PORTC = Position_array[i];
		}//end for
	}//end else

	Last_position = i;


}


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

//################## LINKED LIST FUNCTIONS ##################

// DESC: initializes the linked queue to 'NULL' status
void setup(link **h,link **t){
	*h = NULL;		/* Point the head to NOTHING (NULL) */
	*t = NULL;		/* Point the tail to NOTHING (NULL) */
	return;
}




// DESC: This initializes a link and returns the pointer to the new link or NULL if error 
void initLink(link **newLink){
	//link *l;
	*newLink = malloc(sizeof(link));
	(*newLink)->next = NULL;
	return;
}


// DESC: Accepts as input a new link by reference, and assigns the head and tail of the queue accordingly				
void enqueue(link **h, link **t, link **nL){

	if (*t != NULL){
		/* Not an empty queue */
		(*t)->next = *nL;
		*t = *nL; //(*t)->next;
		
	}/*if*/
	else{
		/* It's an empty Queue */
		//(*h)->next = *nL;
		//should be this
		*h = *nL;
		*t = *nL;
	
	}/* else */
	return;
}



// DESC: Removes the link from the head of the list and assigns it to deQueuedLink
void dequeue(link **h, link **t, link **deQueuedLink){

	*deQueuedLink = *h;	// Will set to NULL if Head points to NULL
	/* Ensure it is not an empty queue */
	if (*h != NULL){
		*h = (*h)->next;
		if (*h == NULL) {
		(*t) = NULL; 
		}
	}/*if*/
	
	return;
}

// DESC: Peeks at the first element in the list
/* This simply allows you to peek at the head element of the queue and returns a NULL pointer if empty */
element firstValue(link **h){
	return((*h)->e);
}


// DESC: deallocates (frees) all the memory consumed by the Queue
void clearQueue(link **h, link **t){

	link *temp;

	while (*h != NULL){
		temp = *h;
		*h=(*h)->next;
		free(temp);
	}/*while*/
	
	/* Last but not least set the tail to NULL */
	*t = NULL;		

	return;
}

