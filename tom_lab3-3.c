/* Sample code for Lab 3.3. This code controls the motor using the ultrasonic ranger. */
#include <c8051_SDCC.h>
#include <stdio.h>
#include <stdlib.h>
#include <i2c.h>
//-----------------------------------------------------------------------------
// Function Prototypes
//-----------------------------------------------------------------------------
void Port_Init(void);
void PCA_Init (void);
void XBR0_Init();
void SMB_Init(void);
void PCA_ISR ( void ) __interrupt 9;

int setCarSpeed(unsigned int range);
unsigned int readUSRanger(void);
unsigned int readCmRange(unsigned char * Data, unsigned int range);
void startPing(unsigned char * Data);


//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------
unsigned int NEUTRAL_PW = 2765; //1.5 ms
unsigned int BACKWARD_PW_MAX = 2027; //1.1 ms
unsigned int FORWARD_PW_MAX = 3502; //1.9 ms
int PW = 0;

unsigned char init_time = 50; //50 * 20 ms about = 1 s
unsigned char init_ticks = 0;
//Period: 20 ms. PW: init 1.5 ms.

unsigned char r_ct = 0;
unsigned int _ranger_addr = 0xE0;
unsigned int _ping_cm = 0x51;

unsigned int range = 0;

__sbit __at 0xB6 SS;


//-----------------------------------------------------------------------------
// Main Function
//-----------------------------------------------------------------------------
void main(void)
{
    // initialize board
    Sys_Init();
    putchar(' '); //the quotes in this line may not format correctly
    Port_Init();
    PCA_Init();
    XBR0_Init();
	SMB_Init();

    //print beginning message
    printf("Embedded Control Speed\r\n");
    // set the PCA output to a neutral setting
	
	NEUTRAL_PW = 2765; //1.5 ms
	BACKWARD_PW_MAX = 2027; //1.1 ms
	FORWARD_PW_MAX = 3502; //1.9 ms

    PW = NEUTRAL_PW;
	PCA0CP2 = 0xFFFF - PW;

    printf("Initializing...\r\n");
	init_ticks = 0;
    while ( init_ticks <= init_time ) { ; } //wait for speed controller to initialize
    printf("Ready!\r\n");
    while(1)
    {
        if (SS)
            //slide switch on, do the thing
        {
            if (r_ct >= 4)
            {
                range = readUSRanger();
                PW  = setCarSpeed(range);
            }
        }
        else
            //motor off
        {
            range = 45;
            PW  = setCarSpeed(range);
        }

        //PW  = setCarSpeed(range);
        //PCA0CP2 = 0xFFFF - PW;
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
    //P1MDOUT = ________;  //set output pin for CEX0 or CEX2 in push-pull mode
    P1MDOUT |= 0x03; //CEX2 in push-pull for ultrasonic ranger
    P3MDOUT &= ~0x20;
    P3MDOUT |= 0x20; //pin 6 is open drain for slide switch
}

//-----------------------------------------------------------------------------
// XBR0_Init
//-----------------------------------------------------------------------------
//
// Set up the crossbar
//
void XBR0_Init()
{
    XBR0 = 0X27;
    //As written in lab. Has UART, SPI, SMBus, and CEX channels as in worksheet.
}

//-----------------------------------------------------------------------------
// SMB_Init
//-----------------------------------------------------------------------------
//
// Set up the SMBus
//
void SMB_Init()
{
    SMB0CR = 0x93;
    ENSMB = 1;
}

//-----------------------------------------------------------------------------
// PCA_Init
//-----------------------------------------------------------------------------
//
// Set up Programmable Counter Array
//
void PCA_Init(void)
{
    // reference to the sample code in Example 4.5 -Pulse Width Modulation 
    // implemented using the PCA (Programmable Counter Array), p. 50 in Lab Manual.

    //Period: 20ms
    //PW min: 1.1 ms
    //PW max: 1.9 ms
    //PW ini: 1.5 ms

    PCA0MD = 0x81; //suspends PCA when system is idle (bit 7 = 1),
    // uses SYSCLK/12 (bits 1-3 = 0), and enables inerrupt (bit 0 = 1)
    PCA0CPM2 = 0xC2;
    //16bit (bit 7 = 1), compare mode (bit 6 = 1), enables PWM (bit 1 = 1)
    EA = 1; //enable interrupts
    EIE1 |= 0x08; //enable PCA0 interrupt

    //PCA_start = 28671; //start point so period is 20 ms
    PCA0CN = 0x40; //enable timer

	PCA0CP2 = 0xFFFF - NEUTRAL_PW; //init
}

//-----------------------------------------------------------------------------
// PCA_ISR
//-----------------------------------------------------------------------------
//
// Interrupt Service Routine for Programmable Counter Array Overflow Interrupt
//
void PCA_ISR ( void ) __interrupt 9
{
    // reference to the sample code in Example 4.5 -Pulse Width Modulation 
    // implemented using the PCA (Programmable Counter Array), p. 50 in Lab Manual.

    if (CF)
    {
        CF = 0; //reset interrupt flag
        /*
        PCA0H = 0x6F;
        PCA0L = 0xFF;
        */
        PCA0 = 28671;
        //28671 split in high and low bits - makes 20 ms period.

        ++r_ct; //20 ms passed; increment ranger count
        ++init_ticks; //speed controller needs to get ready.
    }
    PCA0CN &= 0x40; //clear CF bit, clear CCF bits 0-4.
    //PCA0CN |= 0x40; //enable timer
}

//-----------------------------------------------------------------------------
// readUSRanger
//-----------------------------------------------------------------------------
//
// Read the ultrasound ranger, send next ping, print range, reset flag
//
unsigned int readUSRanger ( void )
{
    unsigned char Data[2];

    range = readCmRange(Data, range); //read range
    printf("Distance: %u cm \r\n", range); //print range

    startPing(Data);
    //write 0x81 to ranger (location: 0xE0) to get ranging mode in cm
    r_ct = 0; //set r_ct = 0 (80 ms flag)

    return range;
}

//-----------------------------------------------------------------------------
// readCmRange
//-----------------------------------------------------------------------------
//
// Read the ultrasound ranger distance
//
unsigned int readCmRange ( unsigned char * Data, unsigned int range )
{
    i2c_read_data ( _ranger_addr, 2, Data, 2 );
    //read from addr, register 2, put in Data, 2 bytes
    range = (unsigned int) Data[0]; //clears prev data
    range <<= 8;
    range += (unsigned int) Data[1];
    return range;
}

//-----------------------------------------------------------------------------
// startPing
//-----------------------------------------------------------------------------
//
// Start a new ping for the ultrasonic ranger
//
void startPing ( unsigned char * Data )
{
    Data[0] = _ping_cm;
    i2c_write_data ( _ranger_addr, 0, Data, 1 );
    //write to addr, register 0, put in Data, 1 bytes
}

//-----------------------------------------------------------------------------
// setCarSpeed
//-----------------------------------------------------------------------------
//
// Set the car speed according to the distance read from the ranger.
//
int setCarSpeed ( unsigned int range )
{
    int scaled = 0;
    if ( range < 40 ) //under lower bound of neutral (40 cm)
    {
        if ( range <= 10 ) //minrange = 10 cm
            //max forward
        {
            //PW = FORWARD_PW_MAX;
            scaled = FORWARD_PW_MAX - NEUTRAL_PW;
        }
        else
            //between min and neutral; linear scaling
        {
            //PW = ((float)(range - 10)/(40-10))*FORWARD_PW_MAX;
            scaled = ((float)(40 - range)/(40-10))*(signed int)(FORWARD_PW_MAX - NEUTRAL_PW);
        }
    }
    else if ( range > 50 ) //above upper bound of neutral (50 cm)
    {
        if ( range >= 90 ) //maxrange = 90 cm
            //max backward
        {
            //PW = BACKWARD_PW_MAX;
            scaled = BACKWARD_PW_MAX - NEUTRAL_PW;
        }
        else
            //between min and neutral; linear scaling
        {
            //PW = ((float)(range - 50)/(90-50))*BACKWARD_PW_MAX;
            scaled = ((float)(range - 50)/(90-50))*(signed int)(BACKWARD_PW_MAX - NEUTRAL_PW);
        }
    }
    else
        //in neutral bounds
    {
        //PW = NEUTRAL_PW;
        scaled = 0;
    }

    PW = scaled + NEUTRAL_PW;

    printf("PW: %u\r\n", PW);
    PCA0CP2 = 0xFFFF - PW;

    return PW;

}
