//   Notes: 
//          - Use with a terminal program
// 
//          - F_CPU must be defined to match the clock frequency
//
//          - Compile with the options to enable floating point
//            numbers in printf(): 
//               -Wl,-u,vfprintf -lprintf_flt -lm
//
//          - Pin assignment: 
//            | Port | Pin | Use                         |
//            |------+-----+-----------------------------|
//            | A    | PA0 | Voltage at load             |
//            | D    | PD0 | Host connection TX (orange) |
//            | D    | PD1 | Host connection RX (yellow) |
//            | D    | PD7 | PWM out to drive MOSFET     |
//
/* avr-gcc -mmcu=atmega644p -DF_CPU=12000000 -Wall -Os -Wl,-u,vfprintf -lprintf_flt -lm boost.c -o boost.elf
 avr-objcopy -O ihex boost.elf boost.hex
 avrdude -c usbasp -p m644p -U flash:w:boost.hex */
 
 //avr-gcc -mmcu=atmega644p -L ./ -o text.elf text.o -llcd
 
 //avr-gcc -u vfprintf -lprintf_flt -lm -mmcu=atmega644p -DF_CPU=12000000 -Wall -Os -c boost.c -o boost.o
 //avr-gcc -mmcu=atmega644p -u vfprintf -lprintf_flt -lm -L.\ -o boost.exe boost.o -llcd
 //avr-objcopy -O ihex boost.exe boost.hex
 //avrdude -c usbasp -p m644p -U flash:w:boost.hex
 
 /*
 avr-gcc -mmcu=atmega644p -DF_CPU=12000000 -Wall -u vprintf -lprintf_flt -lm -Os -c boost.c -o boost.o
 avr-gcc -mmcu=atmega644p -L .\ -u vprintf -lprintf_flt -lm -Os boost.o -llcd boost.elf
 avr-objcopy -o ihex boost.elf boost.hex
 avrdude -c usbasp -p m644p -U flash:w:boost.hex
 
 */
 
#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>
#include <math.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include "lcd.h"
#include <string.h>


#define DELAY_MS      500
#define BDRATE_BAUD  9600

#define ADCREF_V     3.3
#define ADCMAXREAD   1023   /* 10 bit ADC */

#define F_CPU 12000000

/* Find out what value gives the maximum
   output voltage for your circuit:
*/
#define PWM_DUTY_MAX 240    /* 94% duty cycle */

#define VOUTMAX 15
#define VOUTMIN 1.5
		
void init_stdio2uart0(void);
int uputchar0(char c, FILE *stream);
int ugetchar0(FILE *stream);
		
void init_adc(void);
double v_load(void);

void init_pwm(void);
void pwm_duty(double x);

void led_light(void);

void init_Interrupts(void);
void display_lcd(void);

volatile uint8_t Vout_target = 10; //Target Vout
volatile double error, error_int, error_dif, error_old = 0; //Initializes all errors for PID control

volatile double DutyCycle = 0.3; //Initializes DutyCycle
volatile double PWM;

volatile char buffer[1];
volatile char check[1];
volatile char input0[1];
volatile char input1[1];
volatile char inputP1[1];
volatile char inputP2[1];
volatile char inputD1[1];
volatile char inputD2[1];
volatile char inputI1[1];
volatile char inputI2[1];

volatile int checknr;

volatile double kP = 0.0015;
volatile double kI = 0.00025;
volatile double kD = 0.0005; 


ISR(INT0_vect){
	if(v_load()/0.176 > VOUTMIN){
		Vout_target--;
	}
	else{
		Vout_target = 10;
	}
}
ISR(INT1_vect, ISR_NOBLOCK){
	if(v_load()/0.176 < VOUTMAX){
		Vout_target++;
	}
	else{
		Vout_target = 10;
	}
}

ISR(USART0_RX_vect){
	scanf("%c", buffer); //Buffer to catch first input
	printf("\n\r To Update Target Voltage, press '1' key.");
	printf("\n\r To Update kP, press '2' key.");
	printf("\n\r To Update kD , press '3' key.");
	printf("\n\r To Update kI , press '4' key.");
	_delay_ms(100);
	fscanf(stdin, "%c", check);
	checknr = atoi(check);
	switch(checknr){
		case 1:
			printf("\n\rPlease enter your new voltage as two number keystrokes, they are added together.");
			fscanf(stdin, "%c", input0); //First number inputted(0-9)
			_delay_ms(200); 
			fscanf(stdin, "%c", input1); //Second number inputted(0-9)
			Vout_target = (atoi(input0)+atoi(input1));//Sum of first and second number assigned to Vout_target
			if (Vout_target > 15 || Vout_target < 2) Vout_target = 10; //Ensures Vout_target does not go too low or too high
			break;
		
		case 2:
			printf("\n\rPlease enter your new value for kP, the number gets added then multiplied by a constant of 1e-4");
			fscanf(stdin, "%c", input0); //First number inputted(0-9)
			_delay_ms(200); 
			fscanf(stdin, "%c", input1); //Second number inputted(0-9)
			kP = ((atoi(input0)+atoi(input1))*1e-4);//Sum of first and second number assigned to kP, then multipled by constant
			if(kP > 0.003 || kP < 0.0005) kP = 0.0015;
			break;
		case 3:
			printf("\n\rPlease enter your new value for kD, the number gets added then multiplied by a constant of 1e-4");
			fscanf(stdin, "%c", input0); //First number inputted(0-9)
			_delay_ms(200); 
			fscanf(stdin, "%c", input1); //Second number inputted(0-9)
			kD = ((atoi(input0)+atoi(input1))*1e-4);//Sum of first and second number assigned to kP, then multipled by constant
			if(kD > 0.002   || kP < 0.0001) kD = 0.0005;
			break;
		case 4:
			printf("\n\rPlease enter your new value for kI, the number gets added then multiplied by a constant of 1e-5");
			fscanf(stdin, "%c", input0); //First number inputted(0-9)
			_delay_ms(200); 
			fscanf(stdin, "%c", input1); //Second number inputted(0-9)
			kI = ((atoi(input0)+atoi(input1))*1e-5);//Sum of first and second number assigned to kP, then multipled by constant
			break;
			if(kI > 0.0008 || kI < 0.00005) kI = 0.00025;
		default:
			printf("Please enter a valid number \n\n\n");
	}
	_delay_ms(500);
}


ISR(TIMER1_COMPA_vect, ISR_NOBLOCK){
	error = ((v_load()/0.176) - Vout_target);
	error_dif = ((error - error_old)/0.01);
	error_int = ((error_int + error) * 0.01);
	DutyCycle = DutyCycle-(error*kP + error_int*kI + error_dif*kD);
	if (DutyCycle > 0.95 || DutyCycle < 0.1){
		DutyCycle = 0.3;
	}
	pwm_duty(DutyCycle);   /* Limited by PWM_DUTY_MAX */  
	error_old = error;
}

int main(void)
{
	uint16_t cnt =0;
    DDRA |= _BV(PA2);
	DDRA |= _BV(PA3);
	init_stdio2uart0();
	init_pwm(); 
	init_adc();
	init_Interrupts();
	sei(); //Enables all interrupts
	
	init_lcd();
	set_orientation(North);
	

	for(;;) {
		led_light();
		display_lcd();
		
	}
}

void display_lcd(){
	char vout_s[20];
	sprintf(vout_s, "%lf", (v_load()/0.176));
	display.x = 10;
	display.y = 10;
	display_string("Vout = ");
	display_string(vout_s);
	display_string("V");
	
	char vout_target_s[20];
	sprintf(vout_target_s, "%d", Vout_target);
	display.x = 10;
	display.y = 20;
	display_string("Vout_target = ");
	display_string(vout_target_s);
	display_string("V");

	char kP_s[20];
	sprintf(kP_s, "%lf", kP);
	display.x = 10;
	display.y = 30;
	display_string("kP = ");
	display_string(kP_s);
	
	char kD_s[20];
	sprintf(kD_s, "%lf", kD);
	display.x = 10;
	display.y = 40;
	display_string("kD = ");
	display_string(kD_s);
	
	char kI_s[20];
	sprintf(kI_s, "%lf", kI);
	display.x = 10;
	display.y = 50;
	display_string("kI = ");
	display_string(kI_s);
	
	char error_string[20];
	sprintf(error_string, "%lf", error);
	display.x = 120;
	display.y = 10;
	display_string("error = ");
	display_string(error_string);
	
	char PWM_string[20];
	sprintf(PWM_string, "%d", ((int16_t)(DutyCycle*PWM_DUTY_MAX)));
	display.x = 120;
	display.y = 30;
	display_string("PWM = ");
	display_string(PWM_string);
}

void led_light(void){
	if(error < 0.5 && error > -0.5){
	PORTA |= _BV(PA2);
	PORTA &= _BV(PA3);//Turns Red LED off
	}
	else if (error > 0.5 || error < -0.5){
	PORTA &= _BV(PA3);	//Turns Red LED on
	PORTA |= _BV(PA2);
	}
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

void init_Interrupts(void){  //Idea for timer interrupt given to me by Christian Webb, cw8g19, majority of code taken from interrrupt lab
	//Configures registers for TIMER1
	TCCR1A = 0;				
	TCCR1B |= _BV(WGM12);//sets mode 4
	TCCR1B |= _BV(CS12) | _BV(CS10);//sets prescaler of 1
	OCR1A = 59; 		 //Triggers at rate of 0.01 seconds
	TIMSK1 |= _BV(OCIE1A);//Enables interrupt for TIMER1 
	
	
	/*Configures interrupt to trigger on falling edge*/
	EICRA |= _BV(ISC01);
	EICRA |= _BV(ISC11);
	
	EIMSK |= _BV(INT0);
	EIMSK |= _BV(INT1);
	
	
	
	UCSR0B |= _BV(RXCIE0); // Enables UART interrupt on receiving data
}



/* Adjust PWM duty cycle
   Keep in mind this is not monotonic
   a 100% duty cycle has no switching
   and consequently will not boost.  
*/
void pwm_duty(double Duty) 
{
    int16_t x = (int16_t)(Duty*PWM_DUTY_MAX);
	
	OCR2A = x;
}
