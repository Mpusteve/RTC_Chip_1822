#include <xc.h>
#define PIC12F 1
#ifdef PIC12F
#include <pic12lf1822.h>
#else
#include <pic16lf1823.h>
#endif
//#include <SPI.c>

// PIC12LF1822 Version 1.0


//Set configuration fuses
#ifdef PIC12F
// PIC12LF1822 Configuration Bit Settings ( 8 Pin package))

// 'C' source line Config statements

// CONFIG1
// Pragma block to set up Configuration Bits
#pragma config FOSC = INTOSC    // Oscillator Selection (XT Oscillator, Crystal/resonator connected between OSC1 and OSC2 pins) - Set to Internal Oscillator
#pragma config WDTE = OFF       // Watchdog Timer Enable (WDT disabled)
#pragma config PWRTE = ON       // Power-up Timer Enable (PWRT disabled)
#pragma config MCLRE = OFF      // MCLR Pin Function Select MCLE is used (MCLR/VPP pin function is RA3)
#pragma config CP = OFF         // Flash Program Memory Code Protection (Program memory code protection is disabled)
#pragma config CPD = OFF        // Data Memory Code Protection (Data memory code protection is disabled)
#pragma config BOREN = OFF      // Brown-out Reset Enable (Brown-out Reset enabled)
#pragma config CLKOUTEN = OFF   // Clock Out Enable (CLKOUT function is disabled. I/O or oscillator function on the CLKOUT pin)
#pragma config IESO = OFF       // Internal/External Switchover (Internal/External Switchover mode is disabled)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor is enabled)

// CONFIG2
#pragma config WRT = OFF         // Flash Memory Self-Write Protection (Write protection off)
#pragma config PLLEN = OFF       // PLL Enable (4x PLL disabled)
#pragma config STVREN = OFF      // Stack Overflow/Underflow Reset Enable (Stack Overflow or Underflow will cause a Reset)
#pragma config BORV = HI         // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (Vbor), low trip point selected.)
#pragma config LVP = ON          // Low-Voltage Programming Enable (Low-voltage programming enabled)
// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.
#else
// PIC16LF1823 Configuration Bit Settings ( 14 Pin package))

// 'C' source line config statements

// CONFIG1
// Pragma block to set up Configuration Bits
#pragma config FOSC = INTOSC    // Oscillator Selection (XT Oscillator, Crystal/resonator connected between OSC1 and OSC2 pins) - Set to Internal Oscillator
#pragma config WDTE = OFF       // Watchdog Timer Enable (WDT disabled)
#pragma config PWRTE = OFF      // Power-up Timer Enable (PWRT disabled)
#pragma config MCLRE = ON       // MCLR Pin Function Select MCLE is not used (MCLR/VPP pin function is MCLR)
#pragma config CP = OFF         // Flash Program Memory Code Protection (Program memory code protection is disabled)
#pragma config CPD = OFF        // Data Memory Code Protection (Data memory code protection is disabled)
#pragma config BOREN = OFF      // Brown-out Reset Enable (Brown-out Reset enabled)
#pragma config CLKOUTEN = OFF   // Clock Out Enable (CLKOUT function is disabled. I/O or oscillator function on the CLKOUT pin)
#pragma config IESO = OFF       // Internal/External Switchover (Internal/External Switchover mode is disabled)
#pragma config FCMEN = OFF       // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor is enabled)

// CONFIG2
#pragma config WRT = OFF        // Flash Memory Self-Write Protection (Write protection off)
#pragma config PLLEN = OFF       // PLL Enable (4x PLL disabled)
#pragma config STVREN = OFF      // Stack Overflow/Underflow Reset Enable (Stack Overflow or Underflow will cause a Reset)
#pragma config BORV = HI        // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (Vbor), low trip point selected.)
#pragma config LVP = ON         // Low-Voltage Programming Enable (Low-voltage programming enabled)
// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.
#endif


#define SECONDS 0
#define MINUTES 1
#define HOURS	2
#define DAY		3
#define DATE	4
#define MONTH	5
#define YEAR	6
#define CONTROL 7

//#define BCD_MODE 1

unsigned char memory[40];				// This is the internal DS1307 time and control registers
										// plus 32 bytes of RAM

unsigned char SPI_flag;					// This flag tells the background that it has an SPI CS
unsigned char SPI_Read_Write;			// This flag tells the background that it had a read or write
#ifndef PIC12F
unsigned int Toggle;                    // Toggle only used for testing in 16LF1823
#endif

//Define all SPI Pins
#ifdef PIC12F
#define SPI_IN      RA0                 // Define SPI SDI signal to be PIC port RA0
#define SPI_CLK     RA1                 // Define SPI CLK signal to be PIC port RA1
#define SPI_CS      RA2                 // Define SPI CS  signal to be PIC port RA2
#define SPI_OUT     RA3                 // Define SPI SDO signal to be PIC port RA3
#else
#define SPI_OUT     RC1                 // Define SPI SDO signal to be PIC port RC1
#define SPI_CLK     RC2                 // Define SPI CLK signal to be PIC port RC2
#define SPI_IN      RC0                 // Define SPI SDI signal to be PIC port RC0
#define SPI_CS      RA2                 // Define SPI CS  signal to be PIC port RA2
#define TEST		RC3                 // Test output
#endif

/* ********************************** D-Tech RTC1307-HT *****************************************
	25/1/2019
 * This program implements an emulation of a DS1307 Real Time Clock chip.
	An Microchip PIC12LF1822-H is used to give an 8 pin 150C device.
	It provides the RTC values dd.mm.yy hh:mm:ss plus 32 bytes of RAM implemented in this version.
	Years are limited to 2000-2255 and Leap Years are supported.
 * The chip is internally clocked at 4MHz when active.
 * The external Low Power Oscillator runs at 32768Hz and updates Timer 1
 * When Timer 1 overflows (1 second) it wakes the processor from sleep and updates the clock.
 * The External Interrupt is used as the SPI_CS	and wakes up the processor from sleep to
 * input and output data.
 * Overall current consumption is under 100uA.
*************************************************************************************************/

// The Wait_For_Clock function waits for a low to high transition on the SPI Clock line
void Wait_For_Clock(void)
{
while(SPI_CLK)							// Wait for SPI Clock to go low
	asm("NOP");
while(SPI_CLK != 1)						// Wait for SPI Clock to go high
	asm("NOP");	
}
void Wait_For_Clock_Low(void)
{
while(1)
	{
	if(!SPI_CLK)				// Wait for SPI Clock to go low
		break;
	}
}
void Wait_For_Clock_High(void)
{
while(1)
	{
	if(SPI_CLK)							// Wait for SPI Clock to go high
		break;
	}
}

// This function generates 8 bit BCD from an 8 bit HEX number (0-99)
unsigned char hex2bcd(unsigned char x)
{
unsigned char y;
y = (x / 10) << 4;
y = y | (x % 10);
return(y);
}

// This function generates 8 bit HEX from an 8 bit BCD number (0-99)
unsigned char bcd2hex(unsigned char x)
{
unsigned char binary;
binary = ((x & 0xF0) >> 4 ) * 10 + (x & 0x0F);
return(binary);
}

// SPI_Read_Address reads the first (address) byte from the SPI input
// If it's a READ (MS bit high), SPI_Read_Write flag is set to 1
// If it's a WRITE (MS bit low), SPI_Read_Write flag is set to 0
// The seven bit address byte is read and returned
unsigned char SPI_Read_Address(void)
{
unsigned char i, address, temp;

Wait_For_Clock();						// Get the read/write clock edge
if(SPI_IN)
	SPI_Read_Write = 1;					// It's Read Mode
else
	SPI_Read_Write = 0;					// It's Write Mode
// Now get the address
address = 0;
for(i=0;i<7;i++)						// 7 bits
	{
	Wait_For_Clock();					// Get the next positive clock edge
	if(SPI_IN)
		address = address | 0x01;
	else
		address = address & 0xFE;
	if(i < 6)
		address = address << 1;
	}
return address;
}

// SPI_Read_Byte reads the addressed byte from the internal memory and sends it to the SPI
void SPI_Read_Byte(unsigned char address)
{
unsigned char i, data;
data = memory[address];					// Get the selected memory byte
#ifdef BCD_MODE
data = hex2bcd(data);					// Convert the HEX data to BCD
#endif
for(i = 8;i > 0;i--)					// 7 bits
	{
	Wait_For_Clock_Low();					// Get the next negative clock edge
	SPI_OUT = data >> (i-1);
	Wait_For_Clock_High();					// Get the next positive clock edge
	}
//	delay_10us(1);
}

// SPI_Write_Byte writes the received byte from the SPI into internal memory
void SPI_Write_Byte(unsigned char address)
{
unsigned char i, data;
data = 0;
for(i = 0;i < 8;i++)					// 7 bits
	{
	Wait_For_Clock();					// Get the next clock edge
	if(SPI_IN)
		data = data | 0x01;
	else
		data = data & 0xFE;
	if(i < 7)
		data = data << 1;
	}
#ifdef BCD_MODE
data = bcd2hex(data);					// Convert the BCD data to HEX
#endif
memory[address] = data;
}

// Interrupt service routine for 1 second clock update
void interrupt Timer1(void)
{
unsigned char temp;

#ifndef PIC12F
if(Toggle)
    {
    TEST = 1;
    Toggle = 0;
    }
else
    {
    TEST = 0;
    Toggle = 1;
    }
#endif

if(INTF)                            // check for SPI_CS high (INT - external interrupt)
		{
		SPI_flag = 1;					// set the flag for SPI_CS HIGH
		INTF = 0;                       // clear the interrupt flag
		return;							// external interrupt
		}
if (TMR1IE && TMR1IF)
    {
//    TEST = 1;


    TMR1ON = 0;                         // Make sure Timer1 is stopped
    TMR1H = 0x080;						// Set the Timer1 count to 32768
    TMR1L = 0;                          // (it will count to 65536) to give 1 second Interrupts

    TMR1IF = 0;                         // reset the timer1 Interrupt Flag
    TMR1ON = 1;                         // Re-start Timer1
	memory[SECONDS]++;
	if(memory[SECONDS] > 59)
		{
		memory[SECONDS] = 0;
		memory[MINUTES]++;
		}
	if(memory[MINUTES] > 59)
		{
		memory[MINUTES] = 0;
		memory[HOURS]++;
		}
	if(memory[HOURS] > 23)
		{
		memory[HOURS] = 0;
		memory[DATE]++;						// Increment the Date
		memory[DAY]++;						// Increment the Day of the week
		if(memory[DAY] > 7)
			memory[DAY] = 1;				// range is 1 to 7 - Mon to Sun
		}
	switch(memory[MONTH])
		{
		case 9:		// September
		case 4:		// April
		case 6:		// June
		case 11:	// November
			if(memory[DATE] > 30)
				{
				memory[DATE] = 1;
				memory[MONTH]++;
				}
			break;
		case 1:		// January
		case 3:		// March
		case 5:		// May
		case 7:		// July
		case 8:		// August
		case 10:	// October
		case 12:	// December
			if(memory[DATE] > 31)
				{
				memory[DATE] = 1;
				memory[MONTH]++;
				if(memory[MONTH] > 12)
					{
					memory[MONTH] = 1;
					memory[YEAR]++;
					}
				}
			break;
		case 2:								// February "Grrr"
			temp = memory[YEAR] / 4;
			if(memory[YEAR] - (4 * temp))	// Check for Leap Year
				{
				if(memory[DATE] > 28) 		// It's not / 4 so 28 days
					{
					memory[DATE] = 1;
					memory[MONTH]++;
					}
				}
			else
				{
				if(memory[DATE] > 29) 		// It's / 4 so 29 days
					{
					memory[DATE] = 1;
					memory[MONTH]++;
					}
				}
		}


//    TEST = 0;
    }
}


// main routine does a low current background loop
// NOP's are very low current!
main()
{
int Delay;
unsigned char Read_Write_Address;
OSCCON = 0xEA;                             // Set No 4XPLL, 4MHz, Internal Oscillator EA = 4MHz, CA = 0.5MHz
#ifdef PIC12F
// Set RA0=In SPI_OUT (to save current), RA1= In (SPI_CLK), RA2=In (SPI_CS), RA3=In (SPI_SDI), RA4 and RA5 to In (32k Oscillator pins)
TRISA = 0x3F;
#else
TRISC = TRISC | 1;    					// Enable SPI_IN input on RC0
TRISC = TRISC | 2;    					// Enable SPI_OUT input on RC1 (to save current))
TRISC = TRISC | 4;    					// Enable SPI_CLK input on RC2
TRISA = TRISA | 4;    					// Enable SPI_CS input on RA2
TRISC = TRISC & 0xF7;    				// Enable TEST Output on RC3
#endif

ANSELA = 0;							// Disable analogue inputs

TMR1ON = 0;                         // Make sure Timer1 is stopped
TMR1H = 0x080;                       // Set the Timer1 count to 32768 (it will count up to 65536))
TMR1L = 0;                          // to give a 1 second count
TMR1IE = 1;                         // Enable the Timer1 bit in the PIE1
TMR1GE = 0;                         // Disable the Timer1 bit in the GIE (Gate))
T1CON = T1CON | 0x4;                // Set the T1SYNC bit to not Synchronise
TMR1CS0 = 0;                        // Timer1 CS2 Disabled
TMR1CS1 = 1;                        // Timer1 CS1 Enabled
T1CKPS0 = 0;                        // Set the Timer1 Pre-scale to 1:1
T1CKPS1 = 0;
T1OSCEN = 1;                        // Timer1 External Oscillator Select = 1
for(Delay = 0;Delay < 10000;Delay++) // Delay for 32KHz oscillator to settle
    {
    }

TMR1IF = 0;                         // Clear any Timer1 Interrupts
INTEDG = 1;                         // Set SPI_CS Interrupt to positive edge
INTF = 0;                           // Clear the Ext Interrupt Flag
PEIE = 1;                           // Enable the all peripheral Interrupts

INTE = 1;                           // Enable INT external interrupt
GIE = 1;                            // Global Enable Interrupts
TMR1ON = 1;                         // Turn on timer1

memory[SECONDS] = 0;				// Set default time/date
memory[MINUTES] = 0;				// to midnight, 1st Jan 2012 (Sunday)
memory[HOURS] = 0;
memory[DAY] = 7;
memory[DATE] = 1;
memory[MONTH] = 1;
memory[YEAR] = 12;
int i,j;
while(1)
    {
        SLEEP();                                        // Go to Sleep until Timer1 or External Interrupt
 		asm("NOP");									// Wait for Sleep to go inactive
		asm("NOP");									// Wait for Sleep to go inactive
   
    if(SPI_flag == 1)									// If SPI_CS was set, go into SPI routines
		{
#ifdef PIC12F
		TRISA = 0x37;									// enable the SPI OUT (RA3) line
#else
    TRISC = TRISC & 0x3D;								// enable the SPI OUT (RC1) line
#endif
		SPI_OUT = 1;                                    // set SPI Output to 1
		SPI_flag = 0;									// reset the SPI_CS found flag
        asm("NOP");                                     // 10us Delay to allow SPI CS de-bounce
        asm("NOP");
        asm("NOP");
        asm("NOP");
        asm("NOP");
        asm("NOP");
        asm("NOP");
        asm("NOP");
        asm("NOP");
        asm("NOP");
        
        if(SPI_CS != 1)									// Check SPI CS is still there
			continue;									// Error SPI_CS not set - ignore
		Read_Write_Address = SPI_Read_Address();		// Look at the SPI Address
		if(SPI_Read_Write)
			SPI_Read_Byte(Read_Write_Address);			// If it's READ send the byte to the SPI	
		else
			SPI_Write_Byte(Read_Write_Address);			// If it's WRITE, write the byte to memory
		SPI_flag = 0;									// reset the SPI_CS found flag
		while(SPI_CS == 1)
			asm("NOP");									// Wait for SPI_CS 	to go inactive
#ifdef PIC12F
		TRISA = 0x3F;									// Disable the SPI_OUT line (RA3) to save current
#else
        TRISC = TRISC | 2;                              // Disable the SPI_OUT line (RC1) to save current)
#endif
        }    
    }
}

