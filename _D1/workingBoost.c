/*   Notes: 
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
 
#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>
#include <math.h>
#include <avr/interrupt.h>

#define DELAY_MS      100
#define BDRATE_BAUD  9600 //speed of the code

#define ADCREF_V     3.3 //reference voltage
#define ADCMAXREAD   1023   /* 10 bit ADC */

/* Find out what value gives the maximum
   output voltage for your circuit:
*/
#define PWM_DUTY_MAX 240    /* 94% duty cycle */

volatile double error = 0.0;

volatile double errorPrev = 0.0;

volatile double errorInt = 0.0;

volatile double errorDiff = 0.0;

volatile double dutyCycle = 0.6;

volatile double targetVoltage = 10.0; 

void init_stdio2uart0(void);
int uputchar0(char c, FILE *stream);
int ugetchar0(FILE *stream);
		
void init_adc(void);
double v_load(void);

void init_pwm(void);
void pwm_duty(double dutyCycle);

void init_counter(void);

int pwmGlobal = 0;

ISR(INT1_vect){

	if((v_load()/0.176)>0){
		targetVoltage = targetVoltage - 0.5;	
	}	else {
		targetVoltage = 5;
	}
}

ISR(INT0_vect){
	
	if((v_load()/0.176)<15){
		targetVoltage = targetVoltage + 0.5;
	}	else {
		targetVoltage = 5;
	}
}

ISR(TIMER1_COMPA_vect){
	error = (v_load()/0.176) - targetVoltage;
    errorInt = (error + errorInt)*0.01;
    errorDiff = (error - errorPrev)/0.01;
    double kP=0.002;
    double kI=0.02;
    double kD=0.0001;
    dutyCycle = dutyCycle - ((error*kP) + (errorInt*kI) + (errorDiff*kD));
	if(dutyCycle>0.95 || dutyCycle<0.1){
		dutyCycle = 0.5;
	}
    errorPrev = error;
	pwm_duty(dutyCycle);
}


int main(void)
{
	uint16_t cnt =0;
        	
	init_stdio2uart0();
	init_pwm(); 
	init_adc();
	init_counter();

	EIMSK |= _BV(INT0);
	EIMSK |= _BV(INT1);
	
	sei();

	for(;;) {	    
	    printf( "%04d:  ", cnt );
	    
		double voltage = v_load()/0.176;

	    printf( " PWM = %4u -->  %5.3f V --> Boosted Voltage %5.3f --> Target voltage %5.3f      error%5.2f     errorInt%5.2f    errorDiff%5.2f\r\n", pwmGlobal ,v_load(),voltage, targetVoltage, error, errorInt, errorDiff);
	    //_delay_ms(DELAY_MS);
	    cnt++;
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
    ADMUX = 0x01;
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
	TCCR1B = _BV(WGM12);
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
void pwm_duty(double dutyCycle) 
{
    uint8_t x = (uint8_t)(PWM_DUTY_MAX*dutyCycle);
    
    //printf("PWM=%4u  ==>  ", x);  

    OCR2A = x;
	
	pwmGlobal = x;
}
