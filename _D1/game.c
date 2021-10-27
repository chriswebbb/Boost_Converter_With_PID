#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <string.h>
#include "lcd.h"

void writeText(int x, int y, char *str);
int singlePlayGame();
int twoPlayGame();
int endGame();
void printTime();

volatile int timer = 0;//keeps time

volatile int u = 0;//TBD

rectangle squ2 = {50,100, 310, 313};//Global decleration of bat

int score; //keeps score

//moves the bat to the left when button is pressed
ISR(INT1_vect){
	fill_rectangle(squ2, BLACK);
	if(squ2.left >= 6){
		squ2.left = squ2.left - 5;
		squ2.right = squ2.right - 5;
	}
	fill_rectangle(squ2, WHITE);
}

//moves bat to the right when button is pressed.
ISR(INT0_vect){
	fill_rectangle(squ2, BLACK);
	if(squ2.right <= 235){
		squ2.left = squ2.left + 5;
		squ2.right = squ2.right + 5;
	}
	fill_rectangle(squ2, WHITE);
}

ISR(TIMER1_COMPA_vect){
	timer++;
	score = timer;
	printTime();
}

int main()
{
	
	singlePlayGame();

}

void printTime(){
	
	int x = 0;//location of the string x-axis
	int y = 0;//location of the string y-axis
	int minutes;	//holds minutes
	int seconds;	//holds seconds
	char minutesCH[99]; //holds the ASCII of minutes
	char secondsCH[99];	//holds the ASCII of seconds
	
	//changes where string is placed
	display.x = x;
	display.y = y;
	
	display_string("Time:");
	minutes = timer/60;	//gets the value of minutes
	seconds = timer%60;	//gets the value of seconds
	sprintf(minutesCH, "%d", minutes);//takes out the number of minutes
	sprintf(secondsCH, "%d", seconds);//takes out the number of seconds
	display_string(minutesCH);
	display_string(":");
	display_string(secondsCH);
	display_string("   ");
	
}

void writeText(int x, int y, char *str){
	
	display.x = x;
	display.y = y;
	display_string(str);
	
}

/* int menuSel(){
	
	int s = 0;
	
	char firstOp[] = ("1 player");
	char secondOp[] = ("2 player");
	
	int le = sizeof(firstOp);
	int x = (display.width+1)/2 -((le*5)/2);
	int y = 50;
	
	rectangle inner1 = (95, 145, 49, 56);
	rectangle outer1 = (92, 148, 46, 59);
	
	writeText(x, y, fistOp);
	writeText(x, y+25, secondOp);
	
	while(1){
		if(PINC&_BV(PD1)){
			if(s == 0){
				singleGame();
				break;
			}
		
			if(s == 1){
				twoPlayGame();
				break;
			}
		}
		
		if(PINC&_BV(PD0)){
			if(s!=1){
				s++;
			}else{
				s=0;
			} 
		}
		
		if(s=0){
			fill_rectangle(outer1, white);
			fill_rectangle(inner1, black);
			writeText(x, y, fistOp);
			
		}
		
	}
} */

int singlePlayGame(){

	int i = 0;	//for the incremental movement along horizontal
	int j = 0;	//for the incremental movement along vertical
	
	int xD = 0; // This is the direction of the x value changing, 1 is left and 0 is right
	int yD = 0; // This is the direction of the y value changing, 1 is down and 0 is up
	
	int incrementI = 2;	//increments or decrements the cube in the x-axis
	int incrementJ = 2;	//increments or decrements the cube in the y-axis
	
	int square_l = 157; //this is the left value of rectangle
    int square_r = 162; //this is the right value of rectangle
	int square_t = 137; //this is the top value of rectangle
	int square_b = 142; //this is the bottom value of rectangle
	int cubeHi = square_b - square_t;	//determines rectangles width
	int cubeWi = square_r - square_l;	//determines rectangles length
	
	int rect2L = 0; //this is the left value of rectangle
	int rect2R = 240;//this is the right value of rectangle
	int rect2T = 10;//this is the top value of rectangle
	int rect2B = 10;//this is the bottom value of rectangle
	
	init_lcd();//initilises the lcd 
	
	clear_screen();//clears screen
	
	set_orientation(North);//sets orientation to north
	
	rectangle squ = {square_l,square_r,square_t,square_b}; //this is the ball
		
	rectangle squ3 = {rect2L,rect2R,rect2T,rect2B};//this is a second bat
	
	fill_rectangle(squ, PURPLE);//displays the pong ball on screen in purple
	fill_rectangle(squ2, WHITE);//displays the bat
	fill_rectangle(squ3, WHITE);
	
	//Initialize the counter IN FAST PWM mode
	TCCR1A = 0;
	TCCR1B = _BV(WGM12);
	TCCR1B |= _BV(CS12)|_BV(CS10);
	
	//sets the top of the counter to be a certain value so that when it reaches the top 1 second has passed, uses the 1024 prescaler	
	OCR1A = 11718;
	
	//sets the external interrupt to be detected on falling edge
	EICRA |= _BV(ISC01);
	EICRA |= _BV(ISC11);
	
	//initilise interrupts
	EIMSK |= _BV(INT0);
	EIMSK |= _BV(INT1);
	//timer interrupt
	TIMSK1 |= _BV(OCIE1A) ; 
	sei();
	
	
	
	for(;;){
		
		_delay_ms(20);
		//fake animation
		fill_rectangle(squ, BLACK);
		
		//collision detection between the square and bat via the x-axis
		if((squ.right>=(squ2.left-(incrementI)))&&(squ.left<=(squ2.right+incrementI))&&(squ.top>=(squ2.top-cubeHi))&&(squ.bottom<=(squ2.bottom+cubeHi))){
			xD = !xD;
		}
		
		//collision detection between the square and bat via the y-axis
		if((squ.top<=(squ2.bottom+incrementJ))&&(squ.bottom>=(squ2.top-incrementJ))&&(squ.right<=(squ2.right+cubeWi))&&(squ.left>=(squ2.left-cubeWi))){
			yD = !yD;
		}
		
		/* if((squ.right>=(squ2.left-(incrementI)))&&(squ.left<=(squ2.right+incrementI))&&(squ.top>=(squ2.top-cubeHi))&&(squ.bottom<=(squ2.bottom+cubeHi))&&(squ.right>=(squ2.left-(incrementI)))&&(squ.left<=(squ2.right+incrementI))&&(squ.top>=(squ2.top-cubeHi))&&(squ.bottom<=(squ2.bottom+cubeHi))){
			yD = !yD;
			xD = !xD;
		} */
		
		//This will check if the cube is at the border
		if((display.width-incrementI) <= (squ.right)){xD = 1;}
		if(incrementI >= (squ.left)){xD = 0;}
		if((display.height-incrementJ) <= (squ.bottom))
		{
			//endGame();
			//break;
			yD = 1;
		}
		if((incrementJ+10) >= (squ.top)){yD = 0;}
		
		//increments or decrements the balls position based on the direction x-axis
		if(xD){
			i = i-incrementI;
		}else{
			i = i+incrementI;
		}
		
		//increments or decrements the balls position based on the direction y-axis
		if(yD){
			j = j-incrementJ;
		}else{
			j = j+incrementJ;
		}
		
		//updates the position of the pong ball
		squ.right = square_r + i;
		squ.left = square_l + i;
		squ.bottom = square_b + j;
		squ.top = square_t + j;
		
		//displays the new ball
		fill_rectangle(squ, PURPLE);
	}
	
}

int twoPlayGame(){
	
}

int endGame(){
	
	char str[] = {"Game Over"};
	int le = sizeof(str);
	cli();
	set_orientation(North);
	display.background = PURPLE;
	clear_screen();
	//sets the string to be in centre based on its length
	display.x = (display.width+1)/2-((le*5)/2);
	display.y = (display.height/2);
	display_string(str);
	//dispalys the score
	display.x = ((display.width+1)/2)-25;
	display.y = display.height/2 + 10;
	display_string("Score: ");
	char scoreL[999];
	sprintf(scoreL, "%d", score);
	display_string(scoreL);
	//sets timer to zero for restart
	timer = 0;
	while(1);
	
}
