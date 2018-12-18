//#include <system.h>
#define PIC12F 1
#ifdef PIC12F
#include <pic12lf1822.h>
#else
#include <PIC16LF1823.h>
#endif
//#include <SPI.c>

// Version 1.0

// Set clock frequency to 4.194304MHz
//Set configuration fuse
#ifdef PIC12F
#pragma DATA _CONFIG, _XT_OSC & _WDT_OFF & _CP_OFF & _MCLRE_OFF & _BOR_OFF & _PWRTE_ON
//#pragma CLOCK_FREQ 1048576
//#pragma CLOCK_FREQ 4194304
#else
#pragma DATA _CONFIG, _XT_OSC & _WDT_OFF & _CP_OFF //& _BOR_OFF
#pragma CLOCK_FREQ 4194304
#endif
// Interrupt service routine for 1 second clock update

#define MHZ_4				// This defines a 4 or 1MHz crystal

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
unsigned char Toggle;					// This is the Toggle byte only used to divide the interrupts time by 2

//Define all SPI Pins
#ifdef PIC12F
#define SPI_OUT     RA0                 // Define SPI SDO signal to be PIC port GP0
#define SPI_CLK     RA1                 // Define SPI CLK signal to be PIC port GP1
#define SPI_CS      RA2                 // Define SPI CS  signal to be PIC port GP2
#define SPI_IN      RA3                 // Define SPI SDI signal to be PIC port GP3
#else
#define SPI_OUT     RC1                 // Define SPI SDO signal to be PIC port RC1
#define SPI_CLK     RC2                 // Define SPI CLK signal to be PIC port RC2
#define SPI_IN      RC0                 // Define SPI SDI signal to be PIC port RC0
#define SPI_CS      RA2                 // Define SPI CS  signal to be PIC port RA2
#define TEST		RC3                 // Test output
#endif

/* ********************************** MECIRIA RTC1307-HT *****************************************
	This program implements an emulation of a DS1307 Real Time Clock chip.
	An Microchip PIC12F615-H is used to give an 8 pin 150C device.
	It provides the RTC values dd.mm.yy hh:mm:ss plus 32 bytes of RAM implemented in this version.
	Years are limited to 2000-2255 and Leap Years are supported.
	Care has been taken to minimise current consumption. At 4.194304Mhz current should be around
	400uA. At 1.048576MHz this drops to around 250uA. Lower frequency operation is possible down
	to around 0.1MHz with an XT crystal. Delays need to be re-calculated with each new speed.
*************************************************************************************************/
// The Wait_For_Clock function waits for a low to high transition on the SPI Clock line
void Wait_For_Clock(void)
{
while(SPI_CLK)							// Wait for SPI Clock to go low
	asm("nop");
while(SPI_CLK != 1)						// Wait for SPI Clock to go high
	asm("nop");	
}
void Wait_For_Clock_Low(void)
{
while(1)
	{
#ifdef PIC12F
	if(!RA1)                          // Wait for SPI Clock to go low
		break;
#else
//	if(!RC2)				// Wait for SPI Clock to go low
		break;
#endif
	}
}
void Wait_For_Clock_High(void)
{
while(1)
	{
#ifdef PIC12F
	if(RA1)							// Wait for SPI Clock to go high
		break;
#else
//	if(RC2)							// Wait for SPI Clock to go high
		break;
#endif
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
#ifdef PIC12F
if(RA3)
	SPI_Read_Write = 1;					// It's Read Mode
else
	SPI_Read_Write = 0;					// It's Write Mode
#else
if(RC0)
	SPI_Read_Write = 1;					// It's Read Mode
else
	SPI_Read_Write = 0;					// It's Write Mode
#endif
// Now get the address
address = 0;
for(i=0;i<7;i++)						// 7 bits
	{
	Wait_For_Clock();					// Get the next positive clock edge
#ifdef PIC12F
	if(RA3)
		address = address | 0x01;
	else
		address = address & 0xFE;
#else
	if(RC0)
		address = address | 0x01;
	else
		address = address & 0xFE;
#endif
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
#ifdef PIC12F
	if(RA3)
		data = data | 0x01;
	else
		data = data & 0xFE;
#else
	if(RC0)
		data = data | 0x01;
	else
		data = data & 0xFE;
#endif
	if(i < 7)
		data = data << 1;
	}
#ifdef BCD_MODE
data = bcd2hex(data);					// Convert the BCD data to HEX
#endif
memory[address] = data;
}

void __interrupt(high_priority) _Timer1(void)
{
unsigned char temp;

	if(INTF)                            // check for SPI_CS high (external interrupt)
		{
		SPI_flag = 1;					// set the flag for SPI_CS HIGH
		INTF = 0;                       // clear the interrupt flag
		return;							// external interrupt
		}
	TMR1H = 0;							// Clear the Timer1 count
	TMR1L = 0;
	TMR1IF = 0;                         // reset the timer1 Interrupt Flag
#ifdef MHZ_4							// If its 4MHz crystal we need a software divide by 2
	if(Toggle == 0)						// If first cycle of a Toggle - Skip
		{
		Toggle = 1;
		return;
		}
	Toggle = 0;							// reset the Toggle
#endif
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
}


// main routine does a low current background loop
// nop's are very low current!
main()
{
unsigned char Read_Write_Address;

#ifdef PIC12F
// Set GP0=In (to save current), GP1= In, GP2=In, GP3=Inclear
TRISA = 0x3F;
#else
TRISC.0 = 1;    					// Enable SPI_SI intput on RC0
TRISC.2 = 1;    					// Enable SPI_CLK input on RC2
//clear_bit(trisc,1);					// Enable SPI_OUT input on RC1
TRISA.2 = 1;    					// Enable SPI_CS input on RA2
TRISC.3 = 0;    					// Enable TEST on RC3
#endif

ANSELA = 0;							// Disable analog inputs
Toggle = 0;
#ifdef MHZ_4
T1CKPS0 = 1;                        // Set the Timer1 prescale to 1:8
#else
clear_bit(t1con,T1CKPS0);			// Set the Timer1 prescale to 1:4
#endif
T1CKPS1 = 1;                        // This bit is set fot both /4 and /8
TMR1H = 0;							// Clear the Timer1 count
TMR1L = 0;

INTEDG = 1;                         // Set SPI_CS Interrupt to positive edge
INTF = 0;                           // Clear the Ext Int Flag

TMR1IE = 1;                         // Set the Timer1 Interrupt enable bit
PEIE = 1;                           // Enable the Timer1 Interrupt
//NOT_T1SYNC = 1;                     // Don't use Sync
//TMR1CS = 0;                         // Select internal clock
T1OSCEN = 1;
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

while(1)
	{
//RA0 = 1;		// TEST TEST TEST
	asm("nop");												// nop is the lowest power instruction.
	asm("nop");												// By having lots of nop's here we can
	asm("nop");												// minimise power consumption.
	asm("nop");
	asm("nop");												// 32 nop's plus 1 test and jump
	asm("nop");												// gives lowest power and a delay
	asm("nop");												// of
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	asm("nop");
	if(SPI_flag == 1)									// If SPI_CS was set, go into SPI routines
		{
		TRISA = 0x3E;									// enable the SPI OUT (gpio0) line
		RA0 = 1;                                        // set SPI output to 1
		SPI_flag = 0;									// reset the SPI_CS found flag
	//	delay_10us(1);									// Delay to allow SPI CS debounce
		if(SPI_CS != 1)									// Check SPI CS is still there
			continue;									// Error SPI_CS not set - ignor
		Read_Write_Address = SPI_Read_Address();		// Look at the SPI Address
		if(SPI_Read_Write)
			SPI_Read_Byte(Read_Write_Address);			// If it's READ send the byte to the SPI	
		else
			SPI_Write_Byte(Read_Write_Address);			// If it's WRITE, write the byte to memory
		SPI_flag = 0;									// reset the SPI_CS found flag
		while(SPI_CS == 1)
			asm("nop");										// Wait for SPI_CS 	to go inactive
		TRISA = 0x3F;									// Disable the SPI OUT line to save current
		}
	}
}

