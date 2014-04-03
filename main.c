/*Important Information:
Assuming that the information found at http://vorlon.case.edu/~vxl11/NetBots/AmishiJoshi.pdf
is correct, the period for the PWM signal is 20 ms.  Full CCW rotation is at
0.9 ms, neutral position is 1.5 ms, and full CW rotation is 2.1 ms; assuming
an ACLK of 32768*/
#include "io430x14x.h"
#include "intrinsics.h"



void init(void);
void parseCmd(void);
void handleFailState(void);
int  calcSrvo(unsigned char);

//The maximum value for CCRs, resulting in the smallest pulse width
int minPulse = 20400;

char cmdArr[14];
char index = 0;
char parse = 0;
char syncd = 1;
char sncsw1= 0;
char sncsw2= 0;
char sncsw3= 0;
char fail  = 0;

void main( void )
{
  // Stop watchdog timer to prevent time out reset
  WDTCTL = WDTPW + WDTHOLD;
  
  //Call the initialization routine
  init();
  
  //Enable interupts here (instead of in the init routine) to ensure that init
  //has completed.
  __enable_interrupt();
  
  //Main loop
  while(1)
  {
    if(parse) parseCmd();
    if(fail) handleFailState();
  }
}

void init(void)
{
  /*Setting up UART0, and enabling only the RX interrupt; also, set P3.4 and
  *P3.5 to aux mode to turn on the UART pins*/
  U0CTL = CHAR;
  U0TCTL = SSEL0;
  U0BR0 = 0x03;
  U0BR1 = 0x00;
  U0MCTL = 0x4A;
  ME1 |= UTXE0 + URXE0;
  IE1 |= URXIE0;
  
  P3SEL |= 0x30;
    
  /*Setting up Timer B for PWM generation.  TB is suited for this application 
  *because of its unique double-latched operation; the PWM compare register will
  *not update until the clock register counts to 0, which avoids glitching in
  *the controlled servos.*/
  
  /*Item explanation: TBCTL (Timer B overall controll register)
  *TBCLGRP_0 = Each TB compare register latches in new values automatically
  *CNTL_0    = Set TB register to maximum length (16 bits, max = 0xFFFF)
  *TBSSEL_2   = Set TB source to SMCLK (1,048,576 Hz)
  *Note that the input divider is left at its default and that the timer is
  *left in stop mode until all other TB settings have been configured.*/
  TBCTL = TBCLGRP_0+CNTL_0+TBSSEL_2;
  
  /*Item explanation: TBCCTL0 (Cap/Compare register 0 control), TBCCR0
  *SCS    = Sync capture source, to lock the timer into the system clock
  *CLLD_0 = CL0 loads on write to the buffer CCR0 (this is done because it will
  *         only happen once, to set the PWM period to 20mSec)
  *All other values left at default*/
  TBCCTL0 = SCS+CLLD_0;
  TBCCR0 = 20971;
  
  /*Item explanation:TBCCTL1-6, TBCCR1-6
  *SCS      = Sync capture source, to lock the timer into the system clock
  *CLLD_1   = CLx loads from buffer CCRx only once TBR counts to 0
  *OUTBOD_3 = Outmode set to SET/RESET: pin goes high when TBR = CLx, drops
  *           low when TBR = CL0.  Output pins for the PWM that results range
  *           from P4.1 to P4.6.  To calculate the values here, use formula:
  *           TBCCR0 - (Thigh(Sec)*TBSSEL(Max)).  Because we will be calculating
  *           the setpoint as an offset from the minimum width, we'll set the 
  *           TBCCRx to the highest value: 20028 (the widest pulse occurs at 
  *           a value of 18769, a range of 1259 - divide by 8 for a possible 
  *           157 positions... almost exactly the dead band width of the servo
  *           at 8uSec).*/
  TBCCTL1 = SCS+CLLD_1+OUTMOD_3;
  TBCCR1 = minPulse;
  TBCCTL2 = SCS+CLLD_1+OUTMOD_3;
  TBCCR2 = minPulse;
  TBCCTL3 = SCS+CLLD_1+OUTMOD_3;
  TBCCR3 = minPulse;
  TBCCTL4 = SCS+CLLD_1+OUTMOD_3;
  TBCCR4 = minPulse;
  TBCCTL5 = SCS+CLLD_1+OUTMOD_3;
  TBCCR5 = minPulse;
  TBCCTL6 = SCS+CLLD_1+OUTMOD_3;
  TBCCR6 = minPulse;
  
  /*P4.0 and P4.7 are not used for PWM output, so we'll use them as Douts for
    the switches, and initialize them low.*/  
  P4SEL = 0x7E;
  P4DIR = 0xFF;
  P4OUT = 0x00;
  
  /*Set Timer B to count up to TBCL0, and reset (achieving a 20mSec period)*/
  TBCTL |= MC_1;
  
  /*Set up timer A to handle timeout failsafe*/
  /*TACTL = TASSEL_1;
  TACCTL0 = CCIE;
  TACCR0 = 32768;
  TACTL |= MC_1; */ 
}

void parseCmd(void)
{
  //Reset index variable
  index = 0;
  //Reset parse flag
  parse = 0;
  //Reset timout timer
  TAR = 0;
  //Check for initial sync
  if(cmdArr[0] == 'C' && syncd)
  {
    //Check the checksum
    if((cmdArr[1] ^ cmdArr[2] ^ cmdArr[3] ^ cmdArr[4] ^ cmdArr[5] ^ cmdArr[6] ^ cmdArr[7] ^ cmdArr[8]) == cmdArr[9])
    {
      U0TXBUF = 'G';
      
      //Set the PWM for the servo controls; note that since a smaller value in the
      //CLx register results in a wider pulse, we subtract the command result from
      //the minPulse value.
      TBCCR1 = minPulse - calcSrvo(cmdArr[1]);
      TBCCR2 = minPulse - calcSrvo(cmdArr[2]);
      TBCCR3 = minPulse - calcSrvo(cmdArr[3]);
      TBCCR4 = minPulse - calcSrvo(cmdArr[4]);
      TBCCR5 = minPulse - calcSrvo(cmdArr[5]);
      TBCCR6 = minPulse - calcSrvo(cmdArr[6]);
      
      //Handle the switch command
      if(cmdArr[7]&0x40) P4OUT |= 0x01;
      else P4OUT &= 0xFE;
      if(cmdArr[7]&0x80) P1OUT |= 0x80;
      else P4OUT &= 0x7F;
      
      //Handle the mode command
      //(TBD)
    }
    else
    {
      U0TXBUF = 'k';
    }
    //Make sure that the sync switches are reset
    sncsw1 = 0;
    sncsw2 = 0;
    sncsw3 = 0;
  }
  
  else
  {
    U0TXBUF = 's';
    syncd = 0;
  }
}

int calcSrvo(unsigned char pos)
{
  //check for range
  if(pos < 157) return pos*8;
  else return 157*8;
}

void handleFailState(void)
{
  TBCCR1 = minPulse;
  TBCCR2 = minPulse;
  TBCCR3 = minPulse;
  TBCCR4 = minPulse;
  TBCCR5 = minPulse;
  TBCCR6 = minPulse;
  while(fail)
  {
    syncd = 0;
    sncsw1 = 0;
    sncsw2 = 0;
    sncsw3 = 0;
  };
}

#pragma vector = USART0RX_VECTOR
__interrupt void u0RX(void)
{
  char cmd = U0RXBUF;
  U0TXBUF = cmd;
  if(syncd)
  {
    cmdArr[index] = cmd;
    index++;
    if(index >= 14) parse = 1;
  }
  else if(cmd == 'E' || sncsw1)
  {
    sncsw1 = 1;
    if(cmd == 'N' || sncsw2)
    {
      sncsw2 = 1;
      if(cmd == 'D' || sncsw3)
      {
        sncsw3 = 1;
        if(cmd == 255)
        {
          index = 0;
          fail = 0;
          TBR = 0;
          syncd = 1;
        }
      }
    }
  }
}


#pragma vector = TIMERA0_VECTOR
__interrupt void TAFailOut(void)
{
  fail = 1;
  TACCTL0 &= 0xFFFE;
}