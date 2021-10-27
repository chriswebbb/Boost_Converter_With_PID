/* embedded_boost.c 
 *
 *  Author: Steve Gunn & Klaus-Peter Zauner 
 * Licence: This work is licensed under the Creative Commons Attribution License. 
 *          View this license at http://creativecommons.org/about/licenses/
 *   Notes: 
 *          - Use with a terminal program
 * 
 *          - F_CPU must be defined to match the clock frequency
 *
 *          - Compile with the options to enable floating point
 *            numbers in printf(): 
 *               -Wl,-u,vfprintf -lprintf_flt -lm
 *
 *          - Pin assignment: 
 *            | Port | Pin | Use                         |
 *            |------+-----+-----------------------------|
 *            | A    | PA0 | Voltage at load             |
 *            | D    | PD0 | Host connection TX (orange) |
 *            | D    | PD1 | Host connection RX (yellow) |
 *            | D    | PD7 | PWM out to drive MOSFET     |
 */

 // avr-gcc -u vfprintf -lprintf_flt -lm -mmcu=atmega644p -DF_CPU=12000000 -Wall -Os embedded_boost.c -o boost.elf
 //avr-objcopy -O ihex boost.elf boost.hex
 //avrdude -c usbasp -p m644p -U flash:w:boost.hex
 
 //avr-gcc -u vfprintf -lprintf_flt -lm -mmcu=atmega644p -DF_CPU=12000000 -Wall -Os -c embedded_boost.c -o boost.o
 //avr-gcc -mmcu=atmega644p -u vfprintf -lprintf_flt -lm -L.\ -o boost.exe boost.o -llcd
 //avr-objcopy -O ihex boost.exe boost.hex
 //avrdude -c usbasp -p m644p -U flash:w:boost.hex
 
#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>
#include <math.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdlib.h>

#include "lcd.h"

#define DELAY_MS      100
#define BDRATE_BAUD  9600 //speed of the code

#define ADCREF_V     3.3 //reference voltage
#define ADCMAXREAD   1023   /* 10 bit ADC */

/* Find out what value gives the maximum
   output voltage for your circuit:
*/
#define PWM_DUTY_MAX 240    /* 94% duty cycle */

#define MAXV 13

#define MINV 2

volatile int delay =0;

volatile double error = 0.0;

volatile double errorPrev = 0.0;

volatile double errorInt = 0.0;

volatile double errorDiff = 0.0;

volatile double dutyCycle = 0.6;

volatile uint8_t targetVoltage = 10.0; 

volatile char buffer[0];

volatile char inp0[0]; 

volatile char inp1[0];

volatile char temp2[0]; 

volatile int checknr;

void init_stdio2uart0(void);
int uputchar0(char c, FILE *stream);
int ugetchar0(FILE *stream);
		
void init_adc(void);
double v_load(void);

void init_pwm(void);

void pwmDuty(double dutyCycle);

void writeText(int x, int y, char *str);
void init_counter(void);
void grid(void);

double pwmGlobal = 0;

volatile double kP=0.0017;
volatile double kI=0.02;
volatile double kD=0.0001;

ISR(INT1_vect, ISR_NOBLOCK){

	if((v_load()/0.176)>2){
		targetVoltage = targetVoltage - 0.5;	
	}	else {
		targetVoltage = 5;
	}
}

ISR(INT0_vect, ISR_NOBLOCK){
	
	if((v_load()/0.176)<15){
		targetVoltage = targetVoltage + 0.5;
	}	else {
		targetVoltage = 5;
	}
}

ISR(TIMER1_COMPA_vect, ISR_NOBLOCK){
	error = ((v_load()/0.176) - targetVoltage);
    errorInt = ((error + errorInt)*0.01);
    errorDiff = ((error - errorPrev)/0.01);
    dutyCycle = dutyCycle - (error*kP + errorInt*kI + errorDiff*kD);
	if(dutyCycle>0.95 || dutyCycle<0.1){
		dutyCycle = 0.5;
	}
	pwmDuty(dutyCycle);
    errorPrev = error;
	if(v_load()>13){
		pwmDuty(0);
	}
}

//The UART was completed in association with Christian Esterhuise

ISR(USART0_RX_vect){
	scanf("%c", buffer);
	printf("\n\n\r To alter desired voltage, it's key 1.");
	printf("\n\r To alter P weighting, it's key 2.");
	printf("\n\r To alter I weighting, it's key 3.");
	printf("\n\r To alter D weighting, it's key 4.");
	printf("\n\r To alter delay it's key 5");
	_delay_ms(100);
	fscanf(stdin, "%c", temp2);
	checknr = atoi(temp2);
	printf("\n %i \n", checknr);
	switch(checknr){
		case 1:
			printf("\n\rPlease enter your new voltage as two number keystrokes, they are added together.");
			fscanf(stdin, "%c", inp1);
			_delay_ms(200); 
			fscanf(stdin, "%c", inp0);
			//printf("%d",inp0);
			targetVoltage = (atoi(inp0)+atoi(inp1));
			if (targetVoltage > 14 || targetVoltage < 2) targetVoltage = 10;
			printf("\nCompleted\n");
			break;
		
		case 2:
			printf("\n\rPlease enter your new value for kP, the number gets added then multiplied by a constant of 1e-4");
			fscanf(stdin, "%c", inp0);
			_delay_ms(200); 
			fscanf(stdin, "%c", inp1);
			kP = ((atoi(inp0)+atoi(inp1))*1e-4);
			if(kP > 0.003 || kP < 0.0005) kP = 0.002;
			printf("\nCompleted\n");
			break;
		case 3:
			printf("\n\rPlease enter your new value for kD, the number gets added then multiplied by a constant of 1e-4");
			fscanf(stdin, "%c", inp0);
			_delay_ms(200); 
			fscanf(stdin, "%c", inp1);
			kD = ((atoi(inp0)+atoi(inp1))*1e-4);
			if(kD > 0.002   || kP < 0.0001) kD = 0.02;
			printf("\nCompleted\n");
			break;
		case 4:
			printf("\n\rPlease enter your new value for kI, the number gets added then multiplied by a constant of 1e-5");
			fscanf(stdin, "%c", inp0);
			_delay_ms(200); 
			fscanf(stdin, "%c", inp1);
			kI = ((atoi(inp0)+atoi(inp1))*1e-5);
			printf("\nCompleted\n");
			break;
			if(kI > 0.0008 || kI < 0.00005) kI = 0.0001;
		case 5:
			printf("\n\rPlease enter your new delay value, this is multiplied by 100");
			fscanf(stdin, "%c", inp0);
			_delay_ms(200); 
			delay = ((atoi(inp0)+atoi(inp1))*10);
			printf("\nCompleted\n");
			break;
		default:
			printf("\n Please enter a valid number\n");
	}
	_delay_ms(300);
}

int main(void)
{
	uint16_t cnt =0;
    
	_delay_ms(delay);
	
	init_stdio2uart0();
	init_pwm(); 
	init_adc();
	init_counter();
	init_lcd();//initilises the lcd 
	clear_screen();//clears screen
	
	set_orientation(East);
	
	EIMSK |= _BV(INT0);
	EIMSK |= _BV(INT1);
	
	UCSR0B |= _BV(RXCIE0);
	
	//DDRC |= _BV(0);
	//DDRC |= _BV(1);
	char voltageS0[10];
	char voltageS1[10];
	char errorW[10];
	char errorWI[10];
	char errorWD[10];
	uint16_t timeX = 0;
	uint16_t timeX2 =0;
	uint8_t offset = 20;
	uint8_t height = height = LCDWIDTH - offset;
	uint8_t voltageY = 10;
	//int xPos = 0;
	rectangle clearer = {0,0,(voltageY+offset),offset};
	rectangle point = {timeX, timeX, (voltageY+offset), (voltageY+offset)};
	rectangle upBar = {0,display.width, 10, 11};
	rectangle downBar = {0, display.width, 227, 228};
	fill_rectangle(upBar, WHITE);
	fill_rectangle(downBar, WHITE);
	clearer.top = height;
	clearer.bottom = offset;
	sei();
	grid();

	for(;;) {	    
	
	    point.right = timeX;
		point.left = timeX;
		point.top = voltageY;
		point.bottom = voltageY;
		
		//if(timeX>LCDHEIGHT)timeX=-1;
		if(timeX>display.width){timeX2=0;}
		clearer.right = timeX2;
		clearer.left = timeX2;
		
		fill_rectangle(clearer, BLACK);
		//_delay_ms(10);
		fill_rectangle(point, PURPLE);
		
		double voltage = v_load()/0.176;
		
		/* printf( "%04d:  ", cnt );
	    printf( " PWM = %4.3f -->  %5.3f V --> Boosted Voltage %5.3f --> Target voltage %5u     error%5.2f     errorInt%5.2f    errorDiff%5.2f  timeX = %5u\r\n", pwmGlobal ,v_load(),voltage, targetVoltage, error, errorInt, errorDiff, timeX); */
	    //_delay_ms(DELAY_MS);
		sprintf(voltageS0, "%f", voltage);
		sprintf(voltageS1, "%i", targetVoltage);
		sprintf(errorW, "%.5f", kP);
		sprintf(errorWI, "%.5f", kI);
		sprintf(errorWD, "%.5f", kD);
		
		writeText(0, 0 , "Boosted Voltage:");
		writeText(100, 0, voltageS0);
		writeText(200, 0 ,"Desired Voltage: ");
		writeText(300, 0, voltageS1);
		display_string(" ");
		//writeText(200, 0 ,"Desired Voltage:        ");
		
		writeText(0, 230 ,"P = ");
	    writeText(20, 230, errorW);
		
		writeText(70,230, "I = ");
		writeText(90,230, errorWI);
		
		writeText(140,230, "D = ");
		writeText(160,230, errorWD);
		
		voltageY = (uint8_t)(display.height-((voltage/MAXV)*height));
		timeX = (uint16_t)(display.width-(cnt % display.width));
		timeX2 = timeX;
		cnt++;
		
	if((cnt%display.width) == 0){
		clear_screen();
		fill_rectangle(upBar, WHITE);
		fill_rectangle(downBar, WHITE);
		grid();
		};
		
	}
}

void grid(void){
	rectangle z ={1,1,11, 228};
	rectangle a ={32,32,11, 228};
	rectangle b ={64,64,11, 228};
	rectangle c ={96,96,11, 228};
	rectangle d ={128,128,11, 228};
	rectangle e ={160,160,11, 228};
	rectangle f ={192,192,11, 228};
	rectangle g ={224,224,11, 228};
	rectangle h ={256,256,11, 228};
	rectangle i ={288,288,11, 228};
	rectangle j ={319,319,11, 228};
	fill_rectangle(z, WHITE);
	fill_rectangle(a, WHITE);
	fill_rectangle(b, WHITE);
	fill_rectangle(c, WHITE);
	fill_rectangle(d, WHITE);
	fill_rectangle(e, WHITE);
	fill_rectangle(f, WHITE);
	fill_rectangle(g, WHITE);
	fill_rectangle(h, WHITE);
	fill_rectangle(i, WHITE);
	fill_rectangle(j, WHITE);
	
}

void writeText(int x, int y, char *str){
	
	display.x = x;
	display.y = y;
	display_string(str);
	
}

int uputchar0(char c, FILE *stream)
{
	if (c == '\n') uputchar0('\r', stream);
	while (!(UCSR0A & _BV(UDRE0)));
	UDR0 = c;
	return c;
}

int ugetchar0(FILE *stream)
{
	while(!(UCSR0A & _BV(RXC0)));
	return UDR0;
}

void init_stdio2uart0(void)
{
	/* Configure UART0 baud rate, one start bit, 8-bit, no parity and one stop bit */
	UBRR0H = (F_CPU/(BDRATE_BAUD*16L)-1) >> 8;
	UBRR0L = (F_CPU/(BDRATE_BAUD*16L)-1);
	UCSR0B = _BV(RXEN0) | _BV(TXEN0);
	UCSR0C = _BV(UCSZ00) | _BV(UCSZ01);

	/* Setup new streams for input and output */
	static FILE uout = FDEV_SETUP_STREAM(uputchar0, NULL, _FDEV_SETUP_WRITE);
	static FILE uin = FDEV_SETUP_STREAM(NULL, ugetchar0, _FDEV_SETUP_READ);

	/* Redirect all standard streams to UART0 */
	stdout = &uout;
	stderr = &uout;
	stdin = &uin;
	
	// Turns on interrupt receive flag
}

void init_adc (void)
{
    /* REFSx = 0 : Select AREF as reference
     * ADLAR = 0 : Right shift result
     *  MUXx = 0 : Default to channel 0
     */
    ADMUX = 0x00;
    /*  ADEN = 1 : Enable the ADC
     * ADPS2 = 1 : Configure ADC prescaler
     * ADPS1 = 1 : F_ADC = F_CPU / 64
     * ADPS0 = 0 :       = 187.5 kHz
     */
    ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1);
}

void init_counter(void){
	
	//initilise the counter in fast pwm mode
	TCCR1A = 0;
	TCCR1B |= _BV(WGM12);
	TCCR1B |= _BV(CS12)|_BV(CS10);
	//TOP value set to count to the appropriate number
	OCR1A = 59; //counts to 0.01
	//sets the interrup to be detected on the falling edge
	EICRA |= _BV(ISC01);
	EICRA |= _BV(ISC11);
	//activates the interrupt
	TIMSK1 |= _BV(OCIE1A);

}

double v_load(void)
{
     uint16_t adcread;
         
     /* Start single conversion */
     ADCSRA |= _BV ( ADSC );
     /* Wait for conversion to complete */
     while ( ADCSRA & _BV ( ADSC ) );
     adcread = ADC;
    
     //printf("ADC=%4d", adcread);  
 
     return (double) (adcread * ADCREF_V/ADCMAXREAD);
}

void init_pwm(void)
{
    /* TIMER 2 */
    DDRD |= _BV(PD6); /* PWM out */
    DDRD |= _BV(PD7); /* inv. PWM out */
    

    TCCR2A = _BV(WGM20) | /* fast PWM/MAX */
	     _BV(WGM21) | /* fast PWM/MAX */
	     _BV(COM2A1); /* A output */
    TCCR2B = _BV(CS20);   /* no prescaling */   
}


/* Adjust PWM duty cycle
   Keep in mind this is not monotonic
   a 100% duty cycle has no switching
   and consequently will not boost.  
*/

void pwmDuty(double dutyCycle) 
{
    uint8_t x = (uint8_t)(PWM_DUTY_MAX*dutyCycle);
    
    //printf("PWM=%4u  ==>  ", x);  

    OCR2A = x;
	
	pwmGlobal = x;
}