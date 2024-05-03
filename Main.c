/*
 * File:   Main.c
 * Author: Timothy Marchant
 *
 * Created on April 25, 2024, 12:13 AM
 */
//TRISA register bits.
#define RCLK_TRI TRISC5 
#define SER_TRI TRISC4 
#define SRCLK_TRI TRISC3
//LATA register bits.
#define RCLK_LATA LATC5 
#define SER_LATA LATC4 
#define SRCLK_LATA LATC3

#include "CONFIG.h"
//32 MHz is the clock rate.
#define _XTAL_FREQ 32000000
#define HIGH 1
#define LOW 0
//the last bit is there so that we can bitwrite to it.
#define RTC_Clock_Address 0b11011110
void updateAlarm(void);
void updateMMSS(void);
void Shift_Out(unsigned char);
void displayDigit(unsigned char, unsigned char);
void DisplayTMR(void);
void ChangeTimedigit(_Bool, unsigned char, unsigned char[], _Bool);
_Bool ConfigureTime(_Bool);
void TimerButtonMenu(void);
void TimerMode(void);
void buttonmenu(void);
void DisplayTime(void);
void main(void);
void I2C_Init(void);
void I2C_ACK(void);
void I2C_NACK(void);
unsigned char I2C_Read(void);
void I2C_Wait(void);
unsigned char I2C_Write(unsigned char);
void I2C_Restart(void);
void I2C_Stop(void);
void I2C_Start(void);
void I2C_Transmission(unsigned char);
void I2C_ReadHHMMRegister(void);
void I2C_WriteHHMM(void);
unsigned char I2C_ReadClockRegister(unsigned char);

//7-segment binary representation.

enum SegmentValues {
    /*the datasheet has segments that have a corresponding letter associated with it.
    For this project you can read the binary numbers like 0b A B C D E F G DP where DP=decimal point.
    if you see a 1 that means that segment is lit up, otherwise it's not used for that number.
    the last bit is the decimal point in the digit spot.  For the current application, it is unnecessary to light up.*/
    Zero = 0b11111100,
    One = 0b01100000,
    Two = 0b11011010,
    Three = 0b11110010,
    Four = 0b01100110,
    Five = 0b10110110,
    Six = 0b10111110,
    Seven = 0b11100000,
    Eight = 0b11111110,
    Nine = 0b11100110,
};
const unsigned char numbers[10] = {Zero, One, Two, Three, Four, Five, Six, Seven, Eight, Nine};
const unsigned char digits[4] = {0b10000000, 0b01000000, 0b00100000, 0b00010000};
//24 hour time format.  Read Hour, hour : Minute Minute
//we store the time this way since the RTC clock stores each digit seperately.
unsigned char hhmm[4] = {0};
//For timer mode. Read Minute, minute : Second, Second
unsigned char mmss[4] = {0};
const unsigned char tenshourtable[4] = {0, 1, 2, 0};
const unsigned char GenericTimetable[11] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
const unsigned char Tens_Min_Sec_Table[7] = {0, 1, 2, 3, 4, 5, 0};
const unsigned char OnesHourSpecialTable[5] = {0, 1, 2, 3, 0};
__bit Pressed = 0;
__bit RanOutOfTime = 0;
//this is for determing if a button is pressed or being released. 0 implies it's being pressed
//while 1 implies it's being released.
__bit TMR1Debounce = 0;
unsigned int TMR2count = 0;
//data will be changed based on interrupts caused by rising edge/falling edge.
unsigned char data = 0;

void updateMMSS(void) {
    //update the timer array.
    for (signed char i = 3; i >= 0; i--) {
        //if the current entry is zero move to the next entry.
        if (mmss[i] != 0) {
            mmss[i] -= 1;

            //if we're updating the tens second posisition we need to update the ones second digit accordingly.
            if (i == 2) {
                mmss[3] = 9;
            }//special case, since the tens of the second posistion must be less than 6.
            else if (i == 1) {
                mmss[2] = 5;
                mmss[3] = 9;
            }                //if we're updating the tens minute digit everything to the right must be updated accordingly.
            else if (i == 0) {
                mmss[1] = 9;
                mmss[2] = 5;
                mmss[3] = 9;
            }
            //End
            return;
        }
    }
    //The only way for this to be set is if and only if each digit is zero.
    RanOutOfTime = 1;
}

void updateAlarm(void) {
    //read the minutes register.
    unsigned char minutes[2] = {0, 0};
    unsigned char temp = I2C_ReadClockRegister(0x01);
    //get correct time
    minutes[0] = (temp & 0b01110000) >> 4;
    minutes[1] = temp & 0b00001111;
    //logic for updating minutes accordingly. updates the tens when the ones place is a 9.
    if (minutes[1] == 9) {
        if (minutes[0] == 5) {
            minutes[0] = 0;
        } else {
            minutes[0]++;
        }
    }
    //this adjusts accordingly when the ones place of minutes is 9.
    minutes[1] = GenericTimetable[minutes[1] + 1];
    temp = minutes[1] | (minutes[0] << 4);
    //update alarm time.
    I2C_Start();
    //minutes register
    I2C_Transmission(0x0B);
    //update minutes
    I2C_Write(temp);
    //write to hours to get to the alarm settings register
    I2C_Write(0);
    //clear alarm interrupt bit while preserving settings
    I2C_Write(0b00010000);
    I2C_Stop();
}

void __interrupt() ISR(void) {
    if (TMR2IF) {
        TMR2count++;
        TMR2 = 131;
        if (TMR2count == 1000) {
            TMR2count = 0;
            updateMMSS();
            //RanOutOfTime=1;
        }
        TMR2IF = 0;
    }
    //poll for inputs every 50 milliseconds.
    if (TMR1IF) {
        data = PORTA & 0b00000111;
        data = data | ((PORTC & 0b00000111) << 3);
        if (Pressed && data == 0) {
            Pressed = 0;
        } else if (!Pressed && data > 0) {
            Pressed = 1;
        }            //user is holding down button.
        else {
            data = 0;
        }
        TMR1 = 15536;
        TMR1IF = 0;
    }
    if (IOCAF3) {
        updateAlarm();
        //Read updated time
        I2C_ReadHHMMRegister();
        IOCAF3 = 0;
    }
}
//this method shifts out data to a 74hc595 shift register.

void Shift_Out(unsigned char Sdata) {

    for (signed char i = 7; i >= 0; i--) {
        unsigned char temp = Sdata;
        //most significant to least significant bit.
        //Shift i bits to the left, then we need to shift the left most digit to the rightmost digit.
        temp = temp << i;
        temp = temp >> 7;
        if (temp == 1) {
            SER_LATA = HIGH;
        } else {
            SER_LATA = LOW;
        }
        SRCLK_LATA = HIGH;
        SRCLK_LATA = LOW;
    }
}

void displayDigit(unsigned char number, unsigned char digit) {
    RCLK_LATA = 0;
    Shift_Out(number);
    Shift_Out(digit);
    RCLK_LATA = 1;
    //wait until there is new data.
    data = 0;
    while (data == 0 && !RanOutOfTime);
}
//increment or decrement the currently selected number.

void ChangeTimedigit(_Bool isClock, unsigned char digit, unsigned char number[], _Bool isIncrement) {
    signed char index;
    if (isIncrement) {
        index = number[digit] + 1;
    } else {
        index = ((signed char) number[digit]) - 1;
        if (index < 0) {
            return;
        }
    }
    if (digit == 0 && isClock) {
        number[digit] = tenshourtable[index];
        if (number[digit] == 2 && number[1] > 3) {
            number[1] = 3;
        }
    } else if (isClock && digit == 1 && number[0] == 2) {
        number[digit] = OnesHourSpecialTable[index];
    } else if (digit == 2) {
        number[digit] = Tens_Min_Sec_Table[index];
    } else {
        number[digit] = GenericTimetable[index];
    }
}
//isClock means, if it referring to the actual clock mode, if it's one then it's in clock mode. 
//if it's a timer it's zero.

_Bool ConfigureTime(_Bool isClock) {
    unsigned char number[4] = {0};
    signed char currentdigit = 0;
    while (data != 0b00100000) {
        //display the currently selected digit.
        displayDigit(numbers[number[currentdigit]], digits[currentdigit]);
        switch (data) {
                //increment
            case 0b00000001:
                ChangeTimedigit(isClock, currentdigit, number, 1);
                break;
                //decrement
            case 0b00000010:
                ChangeTimedigit(isClock, currentdigit, number, 0);
                break;
                //Move to the right
            case 0b00000100:
                currentdigit = (currentdigit + 1) % 4;
                break;
                //Move to the Left
            case 0b00001000:
                //modulo in c is not the same as it is in mathematics.  using % on negative numbers can have unintended consequences.
                currentdigit = (currentdigit - 1);
                if (currentdigit == -1) {
                    currentdigit = 3;
                }
                break;
                //exit cancel what we're doing.
            case 0b00010000:
                return 0;
                //we don't have a case for 0b00000001 since we need to do some more stuff outside of the switch statement.
        }
    }
    for (unsigned char i = 0; i < 4; i++) {
        if (isClock) {
            hhmm[i] = number[i];

        } else {
            mmss[i] = number[i];
        }
    }
    //update RTC
    if (isClock) {
        I2C_WriteHHMM();
    }
    return 1;
}

void TimerButtonMenu(void) {
    //pause by
    if (data == 0b00000001) {
        TMR2ON = !TMR2ON;
    }//configure the current time
    else if (data == 0b00000010) {
        ConfigureTime(0);
    }//exit timer mode.  Just set the timer to 0.
    else if (data == 0b00010000) {
        RanOutOfTime = 1;
        mmss[0] = 0;
        mmss[1] = 0;
        mmss[2] = 0;
        mmss[3] = 0;
    }
    if (data > 0) {
        data = 0;
    }
}
//timer display.
//max time is 99 minutes and 59 seconds.  Or 99:59

void DisplayTMR(void) {
    for (unsigned char i = 0; i < 4; i++) {
        RCLK_LATA = LOW;
        Shift_Out(numbers[mmss[i]]);
        Shift_Out(digits[i]);
        RCLK_LATA = HIGH;
    }
}

void TimerMode(void) {

    //setup start time
    //if not 1, end the method we're exiting.

    if (!ConfigureTime(0)) {
        return;
    }
    //Start Timer 2.
    TMR2ON = 1;
    data = 0;
    while (RanOutOfTime == 0) {
        DisplayTMR();
        TimerButtonMenu();
    }
    //Turn off timer 2.
    TMR2ON = 0;
    unsigned int i = 0;
    //this is to indicate that the timer has ended and is waiting for the user to acknowledge that it has ended.
    while (data != 0b00100000) {
        if (i < 1000) {
            DisplayTMR();
        } else {
            displayDigit(0, 0);
            if (i == 2000) {
                i = 0;
            }
        }
        i++;
    }
    RanOutOfTime = 0;
}
//do something based on which button is pressed.  This method allows the user to set the time and use timer mode.

void buttonmenu(void) {
    //Set time button
    if (data == 0b00000001) {
        //Setup Clock.  If they exit do nothing, otherwise reset the timer register and reset TMR1count.
        //the one in the method call is so the method knows we're changing the clock's time.
        ConfigureTime(1);
    }        //Set Timer Mode use seperate timer module so that we can keep the original time constantly updated.
    else if (data == 0b00000010) {
        //preload TMR2 with 131, that way we can guarantee that we get a 0.001 second delay.
        TMR2 = 131;
        TimerMode();
    }
}

void DisplayTime(void) {
    //this loop runs for times, we start at the most significant digit and go to the right.
    //Since this method ends as soon as the loop ends we don't have to reset the digit variable.
    for (unsigned char i = 0; i < 4; i++) {
        RCLK_LATA = LOW;
        //shift out the bits in the shift register.  these arrays hold all necessary information.
        Shift_Out(numbers[hhmm[i]]);
        Shift_Out(digits[i]);
        RCLK_LATA = HIGH;

    }
}

void I2C_Init(void) {
    TRISA5 = 1;
    TRISA4 = 1;
    //turn on master mode and clear other SSP1CON registers.
    SSP1CON1 = 0b00101000;
    SSP1CON2 = 0;
    //SSP1CON3 = 0;
    SSP1STAT = 0;
    //Fclock=100kHz with this BRG value.
    SSP1ADD = 0x4F;
    //set RA5 and RA4 as SDA and SCL pins respecively.
    RA5PPS = 0b00011001;
    RA4PPS = 0b00011000;
    SSP1DATPPS = 0b00000101;
    SSP1CLKPPS = 0b00000100;
    SSP1IE = 0;
    SSP1IF = 0;
}

void I2C_Wait(void) {
    /*We need to wait for the start, stop, etc to finish.
    //the datasheet says to bitwise or the third bit of SSP1CON1 with the start, stop, etc bits.
    //when this is zero, we know that said conditions are finished.*/
    while (((SSP1STAT & 0b00000100) | (SSP1CON2 & 0b00011111)));
}

unsigned char I2C_Write(unsigned char sdata) {
    //Wait then write
    I2C_Wait();
    //Write
    SSP1BUF = sdata;
    I2C_Wait();
    return ACKSTAT;
}
//general transmission to a specific register must write data after calling.

void I2C_Transmission(unsigned char sdata) {
    I2C_Write(RTC_Clock_Address);
    I2C_Write(sdata);
}
//call this upon updating the current time.

void I2C_WriteHHMM(void) {
    //disable interrupts that way I2C is not interrupted mid-transfer (very unlikely, but best left alone).
    TMR1ON = 0;
    IOCAN3 = 0;
    //we're doing a sequential memory write but we need two temp variables
    //hours how it is currently set up it will be in a 24 hour time format.
    unsigned char temp1 = (0b00000000 | ((hhmm[0] << 4) | hhmm[1]));
    //minutes
    unsigned char temp2 = (0b00000000 | (hhmm[2] << 4) | hhmm[3]);
    I2C_Start();
    I2C_Transmission(0x00);
    //disable the clock while we're changing register values also set seconds value to zero.
    I2C_Write(0);
    I2C_Write(temp2);
    I2C_Write(temp1);
    I2C_Stop();
    //Need to go back to the first posistion in memory.
    I2C_Start();
    I2C_Transmission(0x00);
    //restart the clock.
    I2C_Write(0b10000000);
    I2C_Stop();
    //set alarm to go off next minute.
    updateAlarm();
    I2C_ReadHHMMRegister();
    TMR1ON = 1;
    IOCAN3 = 1;
}
//general read method.  Need to write slave address first though.

unsigned char I2C_Read(void) {
    I2C_Wait();
    //set RCEN bit high, then wait for it to clear then read SSP1BUF register.
    RCEN = 1;
    I2C_Wait();
    return SSP1BUF;
}
//This is for reading a specific register.  Plan on doing something with the alarm functionality of the RTC.

unsigned char I2C_ReadClockRegister(unsigned char registeraddress) {
    //disable interrupts that way I2C is not interrupted mid-transfer (very unlikely, but best left alone).
    TMR1ON = 0;
    I2C_Start();
    I2C_Transmission(registeraddress);
    I2C_Restart();
    //indicate we're reading by changing the last bit to a one.
    I2C_Write(RTC_Clock_Address | 0b00000001);
    unsigned char temp = I2C_Read();
    I2C_NACK();
    I2C_Stop();
    //disable interrupts that way I2C is not interrupted mid-transfer (very unlikely, but best left alone).
    TMR1ON = 1;
    return temp;
}
//Read the minutes and hour register of the RTC

void I2C_ReadHHMMRegister(void) {
    unsigned char temp1 = 0;
    unsigned char temp2 = 0;
    //disable interrupts that way I2C is not interrupted mid-transfer (very unlikely, but best left alone).
    TMR1ON = 0;
    I2C_Start();
    //0x01 is the minutes register.
    I2C_Transmission(0x01);
    I2C_Restart();
    //indicate we're reading by changing the last bit to a one.
    I2C_Write(RTC_Clock_Address | 0b00000001);
    temp1 = I2C_Read();
    I2C_ACK();
    //This is sequential memory read, so it goes to the next register which is the hours register.
    temp2 = I2C_Read();
    //send NACK to indicate end of reading.
    I2C_NACK();
    I2C_Stop();
    hhmm[3] = (temp1 & 0b00001111);
    hhmm[2] = (temp1 & 0b01110000) >> 4;
    hhmm[1] = (temp2 & 0b00001111);
    hhmm[0] = (temp2 & 0b00110000) >> 4;
    TMR1ON = 1;
}

void I2C_Start(void) {
    I2C_Wait();
    SEN = 1;
}

void I2C_Restart(void) {
    I2C_Wait();
    RSEN = 1;
}

void I2C_Stop(void) {
    I2C_Wait();
    PEN = 1;
}

void I2C_ACK(void) {
    I2C_Wait();
    ACKDT = 0;
    ACKEN = 1;
}

void I2C_NACK(void) {
    I2C_Wait();
    ACKDT = 1;
    ACKEN = 1;
}
//setup on startup

void setupAlarm(void) {
    I2C_Start();
    I2C_Transmission(0x0B);
    //setup first alarm to go off one minute later (start at 00:00 alarm at 00:01)
    I2C_Write(0b00000001);
    //set hours register to zero that way we can get to the setup address.
    I2C_Write(0);
    //setup Alarm set to go off on matching minutes.  Outputs logic LOW. read on RA3.
    I2C_Write(0b00010000);
    I2C_Stop();
    I2C_Start();
    //RTC control register
    I2C_Transmission(0x07);
    //Enable alarm0
    I2C_Write(0b00010000);
    I2C_Stop();
}

void main(void) {
    //nothing is analog clear analog registers
    ANSELA = 0;
    ANSELC = 0;
    //Set TRIS registers. RA2-RA0 and RC2-RC0 are inputs.
    //RA4-5 are set to inputs for i2c communication as indicated by the datasheet.
    TRISA = 0xff;
    //clear WPUA3 the internal pull up resistor on RA3.
    WPUA3=0;
    TRISC = 0b00000111;
    //clear LATx registers.
    LATA = 0;
    LATC = 0;
    //put the device in the idle state when SLEEP() is called.  Sleep will only be called when editing time.
    //IDLEN = 1;
    //Setup timer 2.  But don't enable the module.
    //we only need to set the prescaler, and it's 64 in this case.
    T2CONbits.T2CKPS = 0b11;
    TMR2IF = 0;
    TMR2IE = 1;
    //An interrupt is caused when TMR2==PR2 according to the datasheet.
    //Set PR2 to 255.
    PR2 = 0xff;
    //Setup Timer1 module for polling inputs
    //clock source is FOSC/4 and prescaler is 8.
    T1CON = 0b00110000;
    //we don't want this register to be used.  We don't need the timer gate functionality.
    T1GCON = 0b00000000;
    TMR1 = 15536;
    TMR1IF = 0;
    TMR1IE = 1;
    //enable interrupts.
    GIE = 1;
    PEIE = 1;
    //setup RTC clock and I2C.  Always set it to 00:00.  The RTC does not have a backup battery (the chip doesn't allow it).
    I2C_Init();
    setupAlarm();
    I2C_WriteHHMM();
    //this is for reading the alarm.
    IOCAN3 = 1;
    IOCAF3=0;
    TMR1ON=1;
    while (1) {
        DisplayTime();
        buttonmenu();
    }
}
