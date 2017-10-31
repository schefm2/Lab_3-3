/* Modified sample code for Lab 3-1, with additional functions from sample code for Lab 3-2.*/
#include <c8051_SDCC.h>
#include <stdio.h>
#include <stdlib.h>
#include <i2c.h>		//Must be included last in header file block
//-----------------------------------------------------------------------------
// 8051 Initialization Functions
//-----------------------------------------------------------------------------
void Port_Init(void);
void PCA_Init (void);
void XBR0_Init();
void SMB_Init(void);
void PCA_ISR (void) __interrupt 9;
//-----------------------------------------------------------------------------
//High level Functions
//-----------------------------------------------------------------------------
void Read_Heading(void);
void Set_PW(void);
//-----------------------------------------------------------------------------
//Low level functions
//-----------------------------------------------------------------------------
unsigned int Read_Compass(void);
//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------
__sbit __at 0xB7 SS;			//Slide switch configured for P3.7 (Pin 32 on EVB)

unsigned int PW_CENTER = 2825;	//Estimated center pulse width, used when SS is off
unsigned int PW_LEFT = 2395;	//Maximum left pulse width
unsigned int PW_RIGHT = 3185;	//Maximum right pulse width
unsigned int SERVO_PW = 2825;	//PW that is allowed to change based on compass reading

//Variables initialized to 0 unless specified otherwise
unsigned char h_count = 0;			//Keeps track of how many PCA interrupts occurred (resets at value of 2 or greater)
unsigned char read_counter = 0;		//Keeps track of how many compass reads have occurred
unsigned char new_heading = 0;		//Flag used to keep 40 ms between compass readings
unsigned int desired_heading = 0;	//Arbitrary starting heading, will change depending on program
unsigned int heading = 0;			//Stores the value of 0 to 3599 returned by the electronic compass
unsigned char Data[2];				//Array for reading data from slave registers
signed int error;					//Difference between desired and actual heading
//-----------------------------------------------------------------------------
// Main Function
//-----------------------------------------------------------------------------
void main(void)
{
	//Initialize board
	Sys_Init();
	putchar(' ');	//The quotes in this line may not format correctly
	Port_Init();
	XBR0_Init();
	PCA_Init();
	SMB_Init();
	
	//Print beginning message
	printf("\r\nLab 3-3: Steering Servo Task.\r\n");
	

	//Displays version number of the electronic compass's software
	i2c_read_data(0xC0, 0, Data, 1);
	printf("The compass's version number is %d\r\n", Data[0]);

	while(1)
	{
		Read_Heading();
		Set_PW();
	}
}
//-----------------------------------------------------------------------------
// Port_Init
//-----------------------------------------------------------------------------
//
// Set up ports for input and output
//
void Port_Init()
{
	P1MDOUT |= 0x01;	//set output pin for CEX0 in push-pull mode (P1.0)
	P3MDOUT &= ~0x80;	//Sets P3.7 to open drain
	P3 |= 0x80;			//Sets impedence on P3.7 high
}
//-----------------------------------------------------------------------------
// XBR0_Init
//-----------------------------------------------------------------------------
//
// Set up the crossbar
//
void XBR0_Init()
{
	XBR0 = 0x27;	//configure crossbar with UART, SPI, SMBus, and CEX channels as
					//in worksheet
}
//-----------------------------------------------------------------------------
// SMB_Init
//-----------------------------------------------------------------------------
//
// Set up the I2C Bus
//
void SMB_Init()
{
	SMB0CR = 0x93;	//Sets SCL to 100 kHz (actually ~94594 Hz)
	ENSMB = 1;		//Enables SMB
}
//-----------------------------------------------------------------------------
// PCA_Init
//-----------------------------------------------------------------------------
//
// Set up Programmable Counter Array
//
void PCA_Init(void)
{
	PCA0MD = 0x81;		//Enable CF Interrupt, uses SYSCLK/12, 
	PCA0CPM0 = 0xC2;	//CCM0 in 16-bit compare mode,enables PWM
	PCA0CN |= 0x40;		//Enables PCA counter
	EIE1 |= 0x08;		//Enable PCA interrupt
	EA = 1;				//Enable global interrupt	
}
//-----------------------------------------------------------------------------
// PCA_ISR
//-----------------------------------------------------------------------------
//
// Interrupt Service Routine for Programmable Counter Array Overflow Interrupt;
// compass reads only occur every 40 ms because the compass recalculates heading
// every 33.3 ms, so to be safe and keep the code simple, this is increased to
// 40 ms per read to prevent duplicate reads
//
void PCA_ISR(void) __interrupt 9
{
	if (CF)
	{
		CF = 0;					//Reset CF interrupt flag
		PCA0 = 28672;			//Starting value for a 20 ms pulse when using SYSCLK/12 and 16-bit timer
		h_count++;
		if(h_count >= 2)
		{
			new_heading = 1;	// 2 overflows is about 40 ms
			h_count = 0;
		}
	}
	
	PCA0CN &= 0x40;		//Handle other PCA interrupt sources
}

//-----------------------------------------------------------------------------
// Read_Heading
//-----------------------------------------------------------------------------
// 
// Reads the compass heading and stores it as a variable. Also prints all necessary
// data for determining if the control algorithm functions properly
//
void Read_Heading()
{
	while (!new_heading);		//Waits until enough overflows for a new heading
	heading = Read_Compass();	//Read compass heading, store as variable heading
	new_heading = 0;			//Lowers flag that must be raised before compass is read
	if (read_counter >= 10)		//Only prints heading every tenth compass read
	{
		read_counter = 0;		//Resets read counter
		printf("Compass heading is: %d\r\n", heading);
		printf("Desired heading is: %d\r\n", desired_heading);
		printf("The error value is: %d\r\n", error);
		printf("The servo pulse width is: %d\r\n", SERVO_PW);
	}

}
//-----------------------------------------------------------------------------
// Set_PW
//-----------------------------------------------------------------------------
// 
// Uses the heading value set by Read_Heading to change the Servo PW to attempt
// to center the car's direction on the desired_heading
//
void Set_PW()
{
	if (SS)
	{
		//Sets CCM value that switches CEX to high; here it is set to a PW for
		//steering to be centered
		PCA0CPL0 = 0xFFFF - PW_CENTER;
		PCA0CPH0 = (0xFFFF - PW_CENTER) >> 8;
		
		//Waits until the slide switch is turned back off
		while(SS);
	}
	error = (signed int)desired_heading - heading;	//Should allow error values between 3599 and -3599

	//If the error is greater abs(180) degrees, then error is set to explementary angle of original error
	if (error > 1800)
		error = error - 3599;
	if (error < -1800)
		error = 3599 + error;

	SERVO_PW = .416666*(error) + PW_CENTER;		//Limits the change from PW_CENTER to 750

	//Additional precaution: if SERVO_PW somehow exceeds the limits set in Lab 3-1,
	//then SERVO_PW is set to corresponding endpoint of PW range [PW_LEFT, PW_RIGHT]
	if (SERVO_PW > PW_RIGHT)
		SERVO_PW = PW_RIGHT;
	if (SERVO_PW < PW_LEFT)
		SERVO_PW = PW_LEFT;

	//Sets CCM value that switches CEX to high; here it is set to a PW determined
	//by the control algorithm
	PCA0CPL0 = 0xFFFF - SERVO_PW;
	PCA0CPH0 = (0xFFFF - SERVO_PW) >> 8;
	
	
}
//-----------------------------------------------------------------------------
// Read_Compass
//-----------------------------------------------------------------------------
// 
// Takes reading from electronic compass and returns a value between 0 and 3599
//
unsigned int Read_Compass()
{
	unsigned char addr = 0xC0; 			//The address of the sensor, 0xC0 for the compass
	i2c_read_data(addr, 2, Data, 2);	//Read two byte, starting at reg 2
	heading =(((unsigned int)Data[0] << 8) | Data[1]); //combine the two values
	//heading has units of 1/10 of a degree
	read_counter++;		//Increments read_counter used for printing reads
	return heading; 	//The heading returned in degrees between 0 and 3599
}