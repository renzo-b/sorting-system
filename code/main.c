//##################                      ##################
//################## LIBRARY DECLARATIONS ##################
//##################                      ##################

#include <stdlib.h> 							// the header of the general purpose standard library of C programming language
#include <avr/io.h>								// the header of i/o port
#include <avr/interrupt.h>
#include "LinkedQueue.h" 



//##################                       ##################
//################## FUNCTION DECLARATIONS ##################
//##################                       ##################

void mTimer(int count);
void RampdownTimer();
void InitializeStepper();
void MoveStepperRamp();
void InitializeStepper();
void TurnStepper();
void PauseButton();
void RampDownButton();
void ClassifyPart();



//##################                  ##################
//################## GLOBAL VARIABLES ##################
//##################                  ##################

// MOTOR CONTROL
char Motor_array[4] = {0b00000000,0b00000100,0b00001000,0b00001111};	// Array of status of DC motor: Brake high, CW, CCW, Cut power
char MOTORSPEED = 65;													// Speed of motor


// STEPPER CONTROL
char Position_array[4] = {0b11011000,0b10111000,0b10110100,0b11010100};	// Array of the possible stepper positions
char Delay_array[15] = {16,16,15,14,13,12,11,10,9,8,8,7,7,6,6};		// Delay in miliseconds for ramp
volatile int Last_position = 0;
volatile unsigned char StepperInitialized = 0;
volatile unsigned char StepperCurrentPosition = 0;
volatile unsigned char StepperDesiredPosition = 0;


// ADC
volatile unsigned int ADC_result_flag;
volatile unsigned int Lowest_value_low = 255;
volatile unsigned int Lowest_value_high = 255;
volatile unsigned int Lowest_value = 1023;


// LOGIC
volatile char STATE = 4;
volatile unsigned char Part = 0;
volatile unsigned char PeekedPart = 0;
volatile unsigned char SamePart = 0;
volatile unsigned char PartPastSensor = 1;
volatile unsigned char PartPastExit = 1;
volatile signed int StepperTempMaterial = 0;
volatile signed int Dequeue_counter = 0;
volatile signed int Enqueue_counter = 0;
volatile signed int Exit_counter = 0;
volatile char pause = 0;
volatile char RampDown = 0;
volatile char direction = 3;
volatile char Desired_direction = 4;
volatile unsigned int steps = 0;
volatile char timing = 75;
volatile char newsteps = 0;
char Sorted_Part_array[4] = {0,0,0,0}; 	// Black, steel, white, aluminum
volatile unsigned int partCode;
char quarterturn = 80;
char halfturn = 80;

								
// LINKED LIST
link *head;			 // The ptr to the head of the queue 
link *tail;			 // The ptr to the tail of the queue 
link *newLink;		 // A ptr to a link aggregate data type (struct) 
link *rtnLink;		 // same as the above 



//##################              ##################
//################## MAIN ROUTINE ##################
//##################              ##################

int main(int argc, char *argv[])
{
	// System Clock
	CLKPR = 0b10000000; 				// Enables system clock prescaler modification
	CLKPR = 0b00000000; 				// within 4 clock cycles, enables no prescaler of system clock (8MHz)
	
	// Output Ports
	DDRA = 0b11111111; 					// Signal to Stepper
	DDRB = 0b10101111; 					// PWM Output A for Timer/Counter0 or PWM Output B for Timer/Counter1 (bit 7)
	DDRC = 0b11111111; 					// LEDs display

	// Input Ports
	DDRD = 0b11110000; 					// External Interrupts 0 to 3 (bits 0 to 3)
	DDRE = 0b00000000; 					// External Interrupts 7 to 4 (bits 4 to 7)
	DDRF = 0b00000000; 					// ADC Input Channel 1 (bit 1)

	// Timers
	TCCR0B |=_BV(CS01) |_BV(CS00); 		// Timer/Counter Control Register B for Timer 0 (PWM), Chooses prescaler 1/64.
	TCCR0A |=_BV(WGM00) | _BV(WGM01); 	// Timer/Counter Control0 Register A for Timer 0 
	TCCR3B |=_BV(CS31); 				// Timer/Counter Control Register B for Timer 3, Chooses prescaler 1/8.
	

	// Linked List Variables
	setup(&head, &tail); 				// Initializes head and tail

	// DC Motor Setup
	TCCR0A |=_BV(COM0A1);				// Set compare output mode for fast PWM to clear OC0A on compare match, set OC0A at TOP
	PORTB |= 0b00000100;
	OCR0A = MOTORSPEED;

	// ADC Setup
	ADCSRA |= _BV(ADEN); 				// Enable ADC
	ADCSRA |= _BV(ADIE); 				// Converson complete enable interrupt of ADC
	ADMUX |= _BV(REFS0) | _BV(MUX0);
	ADCSRA |= _BV(ADPS2) | _BV(ADPS1);	// Chooses 1/64 ADC prescaler ~125kHz clock for 10 bit resolution

	// Interrupt Setup
	cli();															// Disables all interrupts
	EICRA |= _BV(ISC01) | _BV(ISC10) | _BV(ISC21) | _BV(ISC30);  	// | _BV(ISC31); This gives PORTD0-3 a falling/rising edge	
	EICRB |= _BV(ISC41) | _BV(ISC51);

	//EIMSK |= _BV(INT0); 	// Interrupt Enable PIN D0
	EIMSK |= _BV(INT1); 	// Interrupt Enable PIN D1
	EIMSK |= _BV(INT2); 	// Interrupt Enable PIN D2
	EIMSK |= _BV(INT3); 	// Interrupt Enable PIN D3
	EIMSK |= _BV(INT4); 	// Interrupt Enable PIN E4 Pause
	EIMSK |= _BV(INT5); 	// Interrupt Enable PIN E5 Ramp-down

	sei();					// Note this sets the Global Enable for all interrupts
	
	


	while (1)
	{
		switch(STATE)
		{
			case (0) :
				TurnStepper();
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

	return (0);
}



//##################            ##################
//################## INTERRUPTS ##################
//##################            ##################


// EX
ISR(INT1_vect)
{ 
	if (PartPastExit == 1)															// if the part arrives at exit sensor but the stepper is not in position, stop belt
	{
		if(StepperDesiredPosition != StepperCurrentPosition)
		{
			PORTB = 0b00000000;
		}
		PartPastExit = 0;
	}

	else
	{
		
		Exit_counter++;
		if (  (isEmpty(&head)==0)  && (Dequeue_counter == Exit_counter) )			// are there any other parts in the linked-list? if so, dequeue
		{
			dequeue(&head, &tail, &rtnLink);										// dequeue and classify the next part
			Dequeue_counter++;
		
			int temp = rtnLink->e.itemCode;
			partCode = temp;
			ClassifyPart();
			StepperDesiredPosition = Part;											

			
		
			if(isEmpty(&head)==0)													// are there any other parts in the linked list? if so, peek
			{
				element Peek = firstValue(&head);									// peek and classify the second next part
				int Temp_code = Peek.itemCode;
				partCode = Temp_code;
				ClassifyPart();
				StepperTempMaterial = Part;											
				
										
					signed int subtract = StepperTempMaterial-StepperDesiredPosition; // Based on the next and second next part, calculate direction and extra steps required
					if(subtract==3) // Black to aluminum = 3
					{	
						Desired_direction = 0;
						newsteps = 50;
						timing = quarterturn;
					}
					else if (subtract==-1) // aluminum to white, white to steel, steel to black 
					{
						Desired_direction = 0;
						newsteps = 50;
						timing = quarterturn;
					}
					else if(subtract==-2) // 180
					{
						Desired_direction = direction;
						newsteps = 100;
						timing = halfturn;
					}
					else if(subtract==2) // 180�
					{
						Desired_direction = direction;
						newsteps = 100;
						timing = halfturn;
					}					
					else if(subtract==-3) // aluminum to black
					{
						Desired_direction = 1;
						newsteps = 50;
						timing = quarterturn;
					}
					else if (subtract==1) // black to steel, steel to white, white to aluminum
					{
						Desired_direction = 1;
						newsteps = 50;
						timing = quarterturn;
					}
					else if (subtract ==0)
					{
						newsteps =0;					
					}
	
			}// end if 
		
			else
			{
				Desired_direction = 3;
				newsteps = 0;
			}


		}// end if 
		
		if ((Dequeue_counter - Exit_counter) >1)
		{
			PORTC = 0b10101010;
		}

		PartPastExit = 1;		
		Sorted_Part_array[StepperCurrentPosition]++;

	}// end else
}


// Hall Effect
ISR(INT2_vect)											// if hall effect triggers, stepper is in the initial position
{ 
	if(StepperInitialized ==0)
	{
		StepperCurrentPosition = 0;
		STATE = 0;										// After Stepper is initialized, goes to  State 0 which is the main code
	}
	StepperInitialized = 1;		
}


// OR 
ISR(INT3_vect)
{ 
	if(PartPastSensor == 1)								// When a part hits the OR, start a conversion
	{
		PartPastSensor = 0;
		Lowest_value = 1023;
		ADCSRA |= _BV(ADSC); 							// Starts Conversion				
	}//end if

	else
	{
		PartPastSensor = 1;								// Indicates that the part has passed the OR
	}//end else
}


// Pause
ISR(INT4_vect) 
{
	
	mTimer(20);
	while((PINE & 0b00010000) == 0b0001000);
	mTimer(20);

	if(pause == 0)
	{
		pause = 1;
	}
	else
	{
		pause =0;
	}

	OCR0A = 0;
	PORTB = 0b00000000;

	STATE = 1;

	/*
	TCCR2B =0; 	
	OCR2A=255; 						// Top value of timer
	TCNT2=0x00; 					// Initial Value of the Timer to 0
	TIFR2 |= _BV(OCF2A); 			// Output Compare A Match Flag
	TIMSK2=TIMSK2 | 0b00000010; 	// Sets OCIE3A bit to 1 which is a timer output compare A match				
	TCCR2B |=_BV(WGM22); 			// Timer/Counter1 Control Register B set bit WGM12 to 1 which is CTC. CTC is Clear Timer on Compare
	TCCR2B |= _BV(CS21) | _BV(CS22) | _BV(CS20);
	*/
}


// RampDown
ISR(INT5_vect) 
{ 
	STATE = 2;
}


// ADC
ISR(ADC_vect)
{
	if ( ADC < Lowest_value) 												// Saves the ADC value if its lower than the other conversions
	{
		Lowest_value = ADC;	
		Lowest_value_low = ADCL;
		Lowest_value_high = ADCH;				
	}//end if	
	

	if (PartPastSensor == 0)												// If the part hasn't passed the OR, do another conversion
	{
		ADCSRA |= _BV(ADSC);		
	}//end if
		
	else
	{
		PORTC = Lowest_value_low; 											// If the part has passed the OR, enqueue the lowest value of the conversions and display it in the LEDs
		PORTD = Lowest_value_high<<5;

		initLink(&newLink);							
		newLink->e.itemCode = Lowest_value;
		enqueue(&head, &tail, &newLink);
		Enqueue_counter++;
	}//end else		
}


// Rampdown timer
ISR(TIMER1_COMPA_vect)
{
	PORTB = 0b00000000;
	PORTC = 0b00000000;
	TIMSK1 = 0b00000000;

	EIMSK = 0b00000000;

	cli();
}

// Pause timer
ISR(TIMER2_COMPA_vect)
{
	TCCR2B =0;
	TIFR2 =0; 		
	TIMSK2= 0;

	if(pause == 0)
	{
		pause = 1;
		PORTC = 0b01100110;
						
	}//end if

	else
	{
		pause = 0;
		PORTC = 0b11000011;
	}//end else

	
	

	STATE = 1;

}

// Bad ISR
ISR(BADISR_vect)
{
	while(1)
	{
		PORTC = 0b11111111;
		PORTD = 0b11110000;	
	}
}



//##################           ##################
//################## FUNCTIONS ##################
//##################           ##################

// DESC: Turns the stepper to the necessary position
void TurnStepper()
{
	if (Exit_counter > Dequeue_counter)
	{
		Exit_counter = Dequeue_counter;
		StepperDesiredPosition = StepperCurrentPosition;
		PORTD = 0b11110000;
	}


	if ((Dequeue_counter == Exit_counter) && (isEmpty(&head)==0) )  // will go into the if, if there are parts in the linked list
	{	
		dequeue(&head, &tail, &rtnLink);
		Dequeue_counter++;		
		int temp = rtnLink->e.itemCode;
		partCode = temp;
		ClassifyPart();
		StepperDesiredPosition = Part;
	}//end if


	if ((Dequeue_counter - Exit_counter) == 1)
	{
		if ((StepperDesiredPosition-StepperCurrentPosition == 1) | (StepperDesiredPosition-StepperCurrentPosition == -3))
		{		
				direction = 1;
				steps = 50;
				timing = quarterturn;
				MoveStepperRamp();
		}//end if

		else if ((StepperDesiredPosition-StepperCurrentPosition == 2) | (StepperDesiredPosition-StepperCurrentPosition == -2))
		{
				direction = 1;
				steps = 100;
				timing = halfturn;
				MoveStepperRamp();
		}//end else if

		else if ((StepperDesiredPosition-StepperCurrentPosition == 3) | (StepperDesiredPosition-StepperCurrentPosition == -1))	
		{
				direction = 0;
				steps = 50;
				timing = quarterturn;
				MoveStepperRamp();
		}//end else if
	}// end if 

}	


void ClassifyPart()
{

	if (partCode > 974) 
	{
		Part = 0; // black
	}//end if

	else if ((partCode <= 974) && (partCode > 850)) 
	{
		Part = 2;		// white		
	}//end else if

	else if ((partCode <= 850) && (partCode > 380))
	{
		Part = 1;		// steel
	}//end else if

	else if ((partCode <= 380) && (partCode > 0))
	{
		Part = 3;		// aluminum
	}//end else if
	else if (partCode ==0)
	{
		Part = 3;		
	}//end else if
}


void MoveStepperRamp(){

	int i = Last_position;
	int delay = 5;

	// Clockwise
	if (direction == 0 ) 
	{ 
		for (int j=0;j<15;j++)												// Ramp-up Section
		{
			i++;
			if(i>3) i=0;
		
			PORTA = Position_array[i];
			mTimer(Delay_array[j]); 

			if((steps == 50) && (j == 2)) 									
			{
				StepperCurrentPosition = StepperDesiredPosition;
				PORTB = 0b00000100;

				if(Desired_direction == direction)							// Is the next move going to be in the same direction? if so, add the steps necesssary	
				{
					steps = steps + newsteps;		
					newsteps = 0;
					
				}		
				
			}//end if

		}// end for
	
	

		for (int j=0;j<(steps-30);j++)										// Constant velocity Section
		{
			i++;
			if(i>3) i=0;
	
			PORTA = Position_array[i];
			mTimer(delay); 


			if((j == (steps - timing) ) && (steps>50))
			{	
				StepperCurrentPosition = StepperDesiredPosition;
				
				if(Desired_direction == direction)							// Is the next move going to be in the same direction? if so, add the steps necesssary
				{
					steps = steps + newsteps;
					newsteps = 0;
				
				}						

				PORTB = 0b00000100;
				
			}
		}// end for



		for (int j=0;j<15;j++)												// Ramp-down section										
		{
			i++;
			if(i>3) i=0;
		
			PORTA = Position_array[i];
			mTimer(Delay_array[14-j]); 
		}// end for

	} // end if


	// Counter clockwise
	// Goes through the step positions in the array in reverse order
	else 
	{		
		for (int j=0;j<15;j++)												// Ramp-up Section
		{
			i--;
			if(i<0) i=3;
			
			PORTA = Position_array[i];
			mTimer(Delay_array[j]);

			if((steps == 50) && (j == 2)) 
			{
				StepperCurrentPosition = StepperDesiredPosition;
				PORTB = 0b00000100;

				if(Desired_direction == direction)							// Is the next move going to be in the same direction? if so, add the steps necesssary	
				{
					steps = steps + newsteps;		
					newsteps = 0;
					
				}			
			}//end if			 

		}// end for
	


		for (int j=0;j<(steps-30);j++)										// Constant velocity Section
		{
			i--;
			if(i<0) i=3;
			
			PORTA = Position_array[i];
			mTimer(delay); 
			

			if((j == (steps - timing) ) && (steps>50))
			{
				StepperCurrentPosition = StepperDesiredPosition;
				
				if(Desired_direction == direction)							// Is the next move going to be in the same direction? if so, add the steps necesssary	
				{
					steps = steps + newsteps;
					newsteps = 0;
				
				}		
				
				PORTB = 0b00000100;
			}
			
		}// end for
	
		for (int j=0;j<15;j++)												// Ramp-down section
		{
			i--;
			if(i<0) i=3;
			
			PORTA = Position_array[i];
			mTimer(Delay_array[14-j]); 
		}// end for


	} // end else
	steps = 0;
	Last_position = i;


}


void RampdownTimer()
{
	TCCR1B |=_BV(WGM12); 			// Timer/Counter1 Control Register B set bit WGM12 to 1 which is CTC. CTC is Clear Timer on Compare
	OCR1A=0x8000; 					// Top value of timer
	TCNT1=0x0000; 					// Initial Value of the Timer to 0
	TIFR1 |= _BV(OCF1A); 			// Output Compare A Match Flag
	TIMSK1=TIMSK1 | 0b00000010; 	// Sets OCIE3A bit to 1 which is a timer output compare A match	
	
	TCCR1B |=_BV(CS10) |_BV(CS12);	//Prescaler by 1024 and start the timer
}

void mTimer(int count)
{
	int i;
	i=0;

	TCCR3B |=_BV(WGM32); 			// Timer/Counter1 Control Register B set bit WGM12 to 1 which is CTC. CTC is Clear Timer on Compare
	OCR3A=0x03e8; 					// Output Compare Register is set to 1000 decimal at 1MHz => 1 ms 
	TCNT3=0x0000; 					// Initial Value of the Timer to 0
	//TIMSK3=TIMSK3 | 0b00000010; // Sets OCIE3A bit to 1 which is a timer output compare A match
	TIFR3 |= _BV(OCF3A);

	while(i<count)
	{
		if ((TIFR3 & 0x02) == 0x02)
		{
			TIFR3 |=_BV(OCF3A);
			i++;
		}//end if
	}//end while
}


void InitializeStepper()
{
	int j=20;
	int i=0;	// Assumed current step is 0

	while(StepperInitialized == 0 ) 
	{
		i++;
		// Goes through the step positions in the array in increasing order
		if(i>3) i=0;
		PORTA = Position_array[i];
		mTimer(j);
		Last_position = i;

	}//end while 
}


void PauseButton()
{
	
	mTimer(100);


	/*
	TCCR2B =0;
	TIFR2 =0; 		
	TIMSK2= 0;
	*/
	while(pause == 1)
	{
		PORTB = 0b00000000;
	
		PORTC = 0b00010000;
		PORTC |= Sorted_Part_array[0];
		mTimer(2000);

		PORTC = 0b00100000;
		PORTC |= Sorted_Part_array[1];
		mTimer(2000);

		PORTC = 0b01000000;
		PORTC |= Sorted_Part_array[2];
		mTimer(2000);

		PORTC = 0b10000000;
		PORTC |= Sorted_Part_array[3];
		mTimer(2000);
	
		PORTC = 0b11110000;
		PORTC |= (Enqueue_counter - Exit_counter);
		mTimer(2000);
	}

	pause = 0;

	
	OCR0A = MOTORSPEED;
	PORTB = 0b00000100;
	STATE = 0;



}
void RampDownButton()
{
	PORTC = 0b11110000;
	RampdownTimer();
	STATE = 0;

}

//##################                       ##################
//################## LINKED LIST FUNCTIONS ##################
//##################                       ##################

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

char isEmpty(link **h){
	return(*h == NULL);
}/*isEmpty*/
