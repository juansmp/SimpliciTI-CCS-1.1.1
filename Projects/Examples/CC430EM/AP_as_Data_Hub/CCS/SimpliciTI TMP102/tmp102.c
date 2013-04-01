#define ID_TEM_MSB 0x35
#define ID_TEM_LSB 0x9D
#define BASE_TIEMPO ID_3 //ID_1 = 4 SEGUNDOS, ID_2 = 8 SEGUNDOS, ID_3 = 16 SEGUNDOS
#define PERIODOS 20       //Periodo de la aplicación = BASE_TIEMPO*PERIODOS
/*----------------------------------------------------------------------------
 *  Demo Application for SimpliciTI
 *
 *  L. Friedman
 *  Texas Instruments, Inc.
 *---------------------------------------------------------------------------- */

/**********************************************************************************************
  Copyright 2007-2009 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights granted under
  the terms of a software license agreement between the user who downloaded the software,
  his/her employer (which must be your employer) and Texas Instruments Incorporated (the
  "License"). You may not use this Software unless you agree to abide by the terms of the
  License. The License limits your use, and you acknowledge, that the Software may not be
  modified, copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio frequency
  transceiver, which is integrated into your product. Other than for the foregoing purpose,
  you may not use, reproduce, copy, prepare derivative works of, modify, distribute,
  perform, display or sell this Software and/or its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE PROVIDED “AS IS”
  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY
  WARRANTY OF MERCHANTABILITY, TITLE, NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE.
  IN NO EVENT SHALL TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER LEGAL EQUITABLE
  THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES INCLUDING BUT NOT LIMITED TO ANY
  INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST
  DATA, COST OF PROCUREMENT OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY
  THIRD PARTIES (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com.
**************************************************************************************************/

#include "bsp.h"
#include "mrfi.h"
#include "nwk_types.h"
#include "nwk_api.h"

//#include "app_remap_led.h"

//#ifndef APP_AUTO_ACK
//#error ERROR: Must define the macro APP_AUTO_ACK for this application.
//#endif


unsigned char TXData;
unsigned char TXByteCtr;
unsigned char cnt_rst=0;


volatile float temperatura=0;


unsigned char *PTxData;                     // Pointer to TX data
unsigned char TXByteCtr;
int tempint;
volatile unsigned char RxBufferI2C[5]={0,0,0,0,0};
volatile unsigned char TxBufferI2C[5]={1,225,32,0,0};


unsigned char *PRxData;                     // Pointer to RX data
unsigned char RXByteCtr;
volatile unsigned char RxBuffer[128];       // Allocate 128 byte of RAM

void InitI2C(char address);
void I2CRead(char bytes);
void I2CWrite(char bytes);

unsigned char TI_USCI_I2C_slave_present(unsigned char slave_address);


unsigned char  retardo_16s=0, intentos=0;




static void linkTo(void);

static int  sTid = 0;
static linkID_t sLinkID1 = 0;

#define SPIN_ABOUT_A_SECOND   NWK_DELAY(1000)
#define SPIN_ABOUT_A_QUARTER_SECOND   NWK_DELAY(250)

/* How many times to try a Tx and miss an acknowledge before doing a scan */
#define MISSES_IN_A_ROW 11

void main (void)
{
	SYSRSTIV;//clear bor
	PJDIR |= 0xFF;
	    P1DIR |= 0xFF;
		P2DIR |= 0xFF;
		P3DIR |= 0xFF;
		P5DIR |= 0xFF;
		PADIR |= 0xFF;
		PBDIR |= 0xFF;
		PCDIR |= 0xFF;

	BSP_Init();
  InitI2C(0x48);
//  hih6131_Init();
  /* If an on-the-fly device address is generated it must be done before the
   * call to SMPL_Init(). If the address is set here the ROM value will not
   * be used. If SMPL_Init() runs before this IOCTL is used the IOCTL call
   * will not take effect. One shot only. The IOCTL call below is conformal.
   */
#ifdef I_WANT_TO_CHANGE_DEFAULT_ROM_DEVICE_ADDRESS_PSEUDO_CODE
  {
    addr_t lAddr;

    createRandomAddress(&lAddr);
    SMPL_Ioctl(IOCTL_OBJ_ADDR, IOCTL_ACT_SET, &lAddr);
  }
#endif /* I_WANT_TO_CHANGE_DEFAULT_ROM_DEVICE_ADDRESS_PSEUDO_CODE */

  /* Keep trying to join (a side effect of successful initialization) until
   * successful. Toggle LEDS to indicate that joining has not occurred.
   */
  while (SMPL_SUCCESS != SMPL_Init(0))
  {
	  if(intentos<10) {

	  		  TA1CTL = TASSEL_1 + MC_2 + TACLR + TAIE; //+ ID_3;  // ACLK, contmode, clear TAR
	  																   // enable interrupt
	  						 __bis_SR_register(LPM3_bits + GIE);       // Enter LPM3, enable interrupts
	  						 __no_operation();                         // For debugger
	  						 intentos++;
	  	  }else{
	  		  for(retardo_16s=0;retardo_16s<20;retardo_16s++){
	  					  TA1CTL = TASSEL_1 + MC_2 + TACLR + TAIE+ ID_3;  // ACLK, contmode, clear TAR
	  															   // enable interrupt
	  					 __bis_SR_register(LPM3_bits + GIE);       // Enter LPM3, enable interrupts
	  					 __no_operation();                         // For debugger
	  				   }
	  		  intentos=0;
	  	  }
  }



  /* Unconditional link to AP which is listening due to successful join. */
  linkTo();

  while (1) ;
}

static void linkTo()
{
  uint8_t     msg[7];
  uint8_t     button, misses, done;


  /* Keep trying to link... */
  while (SMPL_SUCCESS != SMPL_Link(&sLinkID1))
  {
	  TA1CTL = TASSEL_1 + MC_2 + TACLR + TAIE; //+ ID_3;  // ACLK, contmode, clear TAR
	                                                       // enable interrupt
	             __bis_SR_register(LPM3_bits + GIE);       // Enter LPM3, enable interrupts
	             __no_operation();                         // For debugger
  }

  cnt_rst=0;

  /* sleep radio... */
	      SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SLEEP, 0);
	      I2CWrite(3);
  while (1)
  {
		if(TI_USCI_I2C_slave_present(0x48)){
			InitI2C(0x48);
			/////////////////////////////////////
			//////////Driver temp////////////////
			TxBufferI2C[0]=1;//Escribir registro de configuración
			TxBufferI2C[1]=225;//Escribir registro de configuración - One Shot
			I2CWrite(3);//Petición de medición (escritura en el registro de configuración)

			//////////////////////////////Retardo//////////////////////////////////////
			for(retardo_16s=0;retardo_16s<1;retardo_16s++){
				TA1CTL = TASSEL_1 + MC_2 + TACLR + TAIE;  // ACLK, contmode, clear TAR
															// enable interrupt
				__bis_SR_register(LPM3_bits + GIE);       // Enter LPM3, enable interrupts
				__no_operation();                         // For debugger
			}
			///////////////////////////////////////////////////////////////////////////
			
			TxBufferI2C[0]=0;//Registro de temperatura
			I2CWrite(1);     //Cambiar registro por el de temperatura
			
			I2CRead(2);     //Leer registro de temperatura
			
			
			temperatura=(float)((int)RxBufferI2C[0]<<8|(int)RxBufferI2C[1]);
			temperatura=temperatura/256*10;
			tempint=(int)temperatura;
			
		
			msg[0]='T';
			msg[3] =ID_TEM_MSB;
		    msg[4] =ID_TEM_LSB;
			msg[5]=(char)(tempint>>8); //Bit más significativo de la temperatura
			msg[6]=(char)(tempint&0xFF);//Bit menos significativo de la temperatura

			TxBufferI2C[0]=1;//Escribir registro de configuración
			TxBufferI2C[1]=97;//Escribir registro de configuración - Shut Down
			I2CWrite(3);//Petición de medición (escritura en el registro de configuración)

		}else{
			
			msg[0]='F';
			msg[3] =ID_TEM_MSB;
		    msg[4] =ID_TEM_LSB;
			
		}
    {
      uint8_t      noAck;
      smplStatus_t rc;

      /* get radio ready...awakens in idle state */
      SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_AWAKE, 0);

      /* Set TID  */
      ++sTid;

      msg[1]=(char)(sTid>>8);
      msg[2] = (char)(sTid&0xFF);


      done = 0;

      while (!done)
            {
              for (misses=0; misses < MISSES_IN_A_ROW; ++misses)
              {
                if (SMPL_SUCCESS == SMPL_Send(sLinkID1, msg, ((msg[0]=='F')?5:sizeof(msg))))
                {

                  /* Not supporting Frequency agility. Just break out since there
                   * will be no ack.
                   */
                  break;

                }
              }

                done = 1;

            }


      /* radio back to sleep */
      SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SLEEP, 0);

     				  ////////////////////////////////////////////////////////////////////////////
     				  ///////Wait for next sample time////////////////////////////////////////////
     				  ////////////////////////////////////////////////////////////////////////////
     				  for(retardo_16s=0;retardo_16s<PERIODOS;retardo_16s++){
     					  TA1CTL = TASSEL_1 + MC_2 + TACLR + TAIE + BASE_TIEMPO;  // ACLK, contmode, clear TAR
     															  // enable interrupt
     					__bis_SR_register(LPM3_bits + GIE);       // Enter LPM3, enable interrupts
     					__no_operation();                         // For debugger
     				  }
     				   ///////////////////////////////////////////////////////////////////////////
     				   ///////////////////////////////////////////////////////////////////////////
     				   ///////////////////////////////////////////////////////////////////////////


    }
  }
}




// Timer_A3 Interrupt Vector (TAIV) handler
#pragma vector=TIMER1_A1_VECTOR
__interrupt void TIMER1_A1_ISR(void)
{
  switch(__even_in_range(TA1IV,14))
  {
    case  0: break;                          // No interrupt
    case  2: break;                          // CCR1 not used
    case  4: break;                          // CCR2 not used
    case  6: break;                          // reserved
    case  8: break;                          // reserved
    case 10: break;                          // reserved
    case 12: break;                          // reserved
    case 14:
    		if(cnt_rst<112){
    			cnt_rst++;
    		}else{
				cnt_rst=0;
    			PMMCTL0=PMMPW+PMMSWBOR;
    		}
    	__bic_SR_register_on_exit(LPM3_bits); // Exit LPM0
             break;
    default: break;
  }
}




void InitI2C(char address){
#if cloudmaster
  //uartSend("I2C not initiallized", 20);
#else
  WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT
  PMAPPWD = 0x02D52;                        // Get write-access to port mapping regs
  P3MAP5 = PM_UCB0SDA;                      // Map UCB0SDA output to P3.5
  P3MAP4 = PM_UCB0SCL;                      // Map UCB0SCL output to P3.4
  PMAPPWD = 0;                              // Lock port mapping registers

  P3SEL |= BIT5 + BIT4;                     // Select P3.4 & P3.5 to I2C function

  UCB0CTL1 |= UCSWRST;                      // Enable SW reset
  UCB0CTL0 = UCMST + UCMODE_3 + UCSYNC;     // I2C Master, synchronous mode
  UCB0CTL1 = UCSSEL_2 + UCSWRST;            // Use SMCLK, keep SW reset
  UCB0BR0 = 60;                             // fSCL = SMCLK/12 = ~100kHz
  UCB0BR1 = 0;
  UCB0I2CSA = address;                         // Slave Address
  UCB0CTL1 &= ~UCSWRST;                     // Clear SW reset, resume operation

  //uartSend("I2C initiallized", 16);
#endif
}

void I2CRead(char bytes){
//uartSend("I2C Read");
    UCB0IE &= ~UCTXIE;
    UCB0IE |= UCRXIE;							// Enable RX interrupt
    PRxData = (unsigned char *)RxBufferI2C;    // Start of RX buffer
    RXByteCtr = bytes;                         // Load RX byte counter
    while (UCB0CTL1 & UCTXSTP);                // Ensure stop condition got sent
    UCB0CTL1 |= UCTXSTT;                       // I2C start condition
    __bis_SR_register( LPM0_bits + GIE );
    __no_operation();
//uartSend("after interrupt");
}

void I2CWrite(char bytes){
    UCB0IE &= ~UCRXIE;
    UCB0IE |= UCTXIE;                         // Enable TX interrupt
    PTxData = (unsigned char *)TxBufferI2C;      // TX array start address
                                            // Place breakpoint here to see each
                                            // transmit operation.
    TXByteCtr = bytes;                      // Load TX byte counter

    while (UCB0CTL1 & UCTXSTP);             // Ensure stop condition got sent
	UCB0CTL1 |= UCTR + UCTXSTT;             // I2C TX, start condition

	__bis_SR_register(LPM0_bits + GIE);     // Enter LPM0 w/ interrupts
	__no_operation();                       // Remain in LPM0 until all data
	                                        // is TX'd
	UCB0IE &= ~UCTXIE;                         // Disable TX interrupt
	UCB0CTL1 &= ~UCTR;

}

#pragma vector = USCI_B0_VECTOR
__interrupt void USCI_B0_ISR(void)
{
  switch(__even_in_range(UCB0IV,12))
  {
  case  0: break;                           // Vector  0: No interrupts
  case  2: break;                           // Vector  2: ALIFG
  case  4: break;                           // Vector  4: NACKIFG
  case  6: break;                           // Vector  6: STTIFG
  case  8: break;                           // Vector  8: STPIFG
  case 10:                                  // Vector 10: RXIFG
    RXByteCtr--;                            // Decrement RX byte counter
    if (RXByteCtr)
    {
      *PRxData++ = UCB0RXBUF;               // Move RX data to address PRxData
       if (RXByteCtr == 1)                  // Only one byte left?
        UCB0CTL1 |= UCTXSTP;                // Generate I2C stop condition

    }
    else
    {
      *PRxData = UCB0RXBUF;                 // Move final RX data to PRxData
      __bic_SR_register_on_exit(LPM0_bits); // Exit active CPU
    }
    break;
  case 12:                                  // Vector 12: TXIFG
	    if (TXByteCtr)                          // Check TX byte counter
	    {
	      UCB0TXBUF = *PTxData++;               // Load TX buffer
	      TXByteCtr--;                          // Decrement TX byte counter
	    }
	    else
	    {
	      UCB0CTL1 |= UCTXSTP;                  // I2C stop condition
	      UCB0IFG &= ~UCTXIFG;                  // Clear USCI_B0 TX int flag
	      __bic_SR_register_on_exit(LPM0_bits); // Exit LPM0
	    }
  default: break;
  }
}

//------------------------------------------------------------------------------
// unsigned char TI_USCI_I2C_slave_present(unsigned char slave_address)
//
// This function is used to look for a slave address on the I2C bus.
//
// IN:   unsigned char slave_address  =>  Slave Address
// OUT:  unsigned char                =>  0: address was not found,
//                                        1: address found
//------------------------------------------------------------------------------
unsigned char TI_USCI_I2C_slave_present(unsigned char slave_address){
  unsigned char UCB0IE_bak, slaveadr_bak;
  volatile unsigned char returnValue;
  UCB0IE_bak = UCB0IE;                      // restore old UCB0I2CIE
  slaveadr_bak = UCB0I2CSA;                   // store old slave address
  UCB0IE &= ~ UCNACKIE;                    // no NACK interrupt
  UCB0I2CSA = slave_address;                  // set slave address
  UCB0IE &= ~(UCTXIE + UCRXIE);              // no RX or TX interrupts
  __disable_interrupt();
  UCB0CTL1 |= UCTR + UCTXSTT + UCTXSTP;       // I2C TX, start condition
  while (UCB0CTL1 & UCTXSTP);                 // wait for STOP condition

  returnValue = !(UCB0IFG & UCNACKIFG);
  __enable_interrupt();
  UCB0I2CSA = slaveadr_bak;                   // restore old slave address
  UCB0IE = UCB0IE_bak;                      // restore old UCB0CTL1
  return returnValue;                         // return whether or not
                                              // a NACK occured

}

