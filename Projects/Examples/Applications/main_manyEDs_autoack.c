/*----------------------------------------------------------------------------
 *  Aplicación para sensores inalámbricos de temperatura y humedad
 *
 *  J. S. Morant
 *  Netux S.A.S.
 *---------------------------------------------------------------------------- */
///////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////Parámetros de configuración del nodo///////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
#define ID_HUM_MSB 0x38		//Byte más significativo de la variable de Ubidots de la humedad
#define ID_HUM_LSB 0xA2		//Byte menos significativo de la variable de Ubidots de la humedad
#define ID_TEM_MSB 0x38		//Byte más significativo de la variable de Ubidots de la temperatura
#define ID_TEM_LSB 0xA1		//Byte menos significativo de la variable de Ubidots de la temperatura
#define BASE_TIEMPO ID_3 	//ID_1 = 4 SEGUNDOS, ID_2 = 8 SEGUNDOS, ID_3 = 16 SEGUNDOS Periodo de la interrupción del Timer
#define PERIODOS 20      	//Periodo de la transmisión = BASE_TIEMPO*PERIODOS
//#define TEMP   1			//Comentar cuando la aplicación no use un tmp102
#define TEMP_HUM  1		//Comentar cuando la aplicación no use un HIH6131
//#define PwrIdx IOCTL_LEVEL_1; //IOCTL_LEVEL_0:-10dB ,IOCTL_LEVEL_1:0dB, IOCTL_LEVEL_2:10dB pwr
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

#include "bsp.h"
#include "mrfi.h"
#include "nwk_types.h"
#include "nwk_api.h"



unsigned char TXByteCtr;
unsigned char cnt_rst=0;
unsigned char  retardo_16s=0, intentos=0;
static int  sTid = 0;
static linkID_t sLinkID1 = 0;

/* How many times to try a Tx and miss an acknowledge before doing a scan */
#define MISSES_IN_A_ROW  11

//int dBm_10=IOCTL_LEVEL_0; PWR!!

unsigned char TI_USCI_I2C_slave_present(unsigned char slave_address); //Función para detectar desconexión de sondas
static void linkTo(void); //Loop infinito donde se ejecuta la tarea del nodo

#ifdef TEMP_HUM
unsigned char TXData;
float temperatura, humedad;
unsigned char *PRxData;                     // Pointer to RX data
unsigned char RXByteCtr;
volatile unsigned char RxBuffer[128];       // Allocate 128 byte of RAM
void hih6131_Init(void);
void hih6131_MR(void);
void hih6131_DF(float * humedad, float * temperatura);
#endif //TEMP_HUM

#ifdef TEMP
volatile float temperatura=0;
unsigned char *PTxData;                     // Pointer to TX data
int tempint;
volatile unsigned char RxBufferI2C[5]={0,0,0,0,0};
volatile unsigned char TxBufferI2C[5]={1,225,32,0,0};
unsigned char *PRxData;                     // Pointer to RX data
unsigned char RXByteCtr;
volatile unsigned char RxBuffer[128];       // Allocate 128 byte of RAM
void InitI2C(char address);
void I2CRead(char bytes);
void I2CWrite(char bytes);
#endif //TEMP

void main (void)
{

	SYSRSTIV;//clear bor

	////Pines como salida para ahorrar energía////////
	//////////////////////////////////////////////////
	PJDIR |= 0xFF;
	P1DIR |= 0xFF;
	P2DIR |= 0xFF;
	P3DIR |= 0xFF;
	P5DIR |= 0xFF;
	PADIR |= 0xFF;
	PBDIR |= 0xFF;
	PCDIR |= 0xFF;
    /////////////////////////////////////////////////

	BSP_Init();
  
  #ifdef TEMP_HUM
	hih6131_Init();	//Inicializo el puerto I2C
  #endif //TEMP_HUM
 
  #ifdef TEMP
   InitI2C(0x48);  	//Inicializo el puerto I2C
  #endif //TEMP

  // int pwridxint=IOCTL_LEVEL_2;pwr
  // SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SETPWR, &dBm_10);pwr
  // SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SETPWR, &pwridxint);pwr

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
   * successful.
   */
  while (SMPL_SUCCESS != SMPL_Init(0))
  {
	  if(intentos<10) {

		  TA1CTL = TASSEL_1 + MC_2 + TACLR + TAIE; //+ ID_3;  	   // ACLK, contmode, clear TAR
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
  uint8_t     msg[11];
  uint8_t     misses, done;


  /* Keep trying to link... */
  while (SMPL_SUCCESS != SMPL_Link(&sLinkID1))
  {
	  TA1CTL = TASSEL_1 + MC_2 + TACLR + TAIE; //+ ID_3;  // ACLK, contmode, clear TAR
	                                                       // enable interrupt
	             __bis_SR_register(LPM3_bits + GIE);       // Enter LPM3, enable interrupts
	             __no_operation();                         // For debugger
  }



  /* sleep radio... */
	      SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SLEEP, 0);

  while (1)
  {
	  #ifdef TEMP_HUM
	  if(TI_USCI_I2C_slave_present(0x27)){

		  hih6131_Init();

	      /////////////////////////////////////
    	  //////////Driver hum////////////////
          hih6131_MR();
    	  hih6131_DF(&humedad,&temperatura);
    	  msg[0]='H';
    	  msg[3] =ID_HUM_MSB;
    	  msg[4] =ID_HUM_LSB;//humedad
    	  msg[7] =ID_TEM_MSB;
    	  msg[8] =ID_TEM_LSB;//temperatura
          msg[5]=(char)((int)(humedad*10)>>8);
          msg[6]=(char)(((int)(humedad*10))&0xFF);
          msg[9]=(char)((int)(temperatura*10)>>8);
          msg[10]=(char)(((int)(temperatura*10))&0xFF);
    	  /////////////////////////////////////
    	  /////////////////////////////////////


	  }else{
		  	  	  	msg[0]='F';
		  			msg[3] =ID_HUM_MSB;
		  		    msg[4] =ID_HUM_LSB;
	  }
      #endif //TEMP_HUM

	   #ifdef TEMP
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
	   #endif //TEMP
	  
    {
      /* get radio ready...awakens in idle state */
      SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_AWAKE, 0);


      //SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_SETPWR, &dBm_10);pwr

      /* Set TID  */
      ++sTid;

      msg[1]=(char)(sTid>>8);
      msg[2] = (char)(sTid&0xFF);


      done = 0;

      while (!done)
            {
              for (misses=0; misses < MISSES_IN_A_ROW; ++misses)
              {
                if (SMPL_SUCCESS == SMPL_Send(sLinkID1, msg, ((msg[0]=='F')?5:sizeof(msg)))) //Envío de la información al maestro
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



#ifdef TEMP_HUM
void hih6131_Init(void){//Inicialización de la interfaz I2C

	  WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT

	  PMAPPWD = 0x02D52;                        // Get write-access to port mapping regs
	  P3MAP5 = PM_UCB0SDA;                      // Map UCB0SDA output to P3.5
	  P3MAP4 = PM_UCB0SCL;                      // Map UCB0SCL output to P3.4
	  PMAPPWD = 0;                              // Lock port mapping registers

	  P3SEL |= BIT5 + BIT4;                     // Select P3.4 & P3.5 to I2C function


	  UCB0CTL1 |= UCSWRST;                      // Enable SW reset
	  UCB0CTL0 = UCMST + UCMODE_3 + UCSYNC;     // I2C Master, synchronous mode
	  UCB0CTL1 = UCSSEL_2 + UCSWRST;            // Use SMCLK, keep SW reset
	  UCB0BR0 = 60;                             // fSCL = SMCLK/12 = ~100kHz    debe estar entre 100 KHz y 400KHz
	  UCB0BR1 = 0;
	  UCB0I2CSA = 0x27;                         // Slave Address is 027h
	  UCB0CTL1 &= ~UCSWRST;                     // Clear SW reset, resume operation
}

void hih6131_MR(void){//Measurement request para el sensor HIH6131
	         UCB0IE |= UCTXIE;                         // Enable TX interrupt
		     //TXData = 0x01;                            // Holds TX data
		     TXByteCtr = 0;                          // Load TX byte counter

		      while (UCB0CTL1 & UCTXSTP);             // Ensure stop condition got sent
		      UCB0CTL1 |= UCTR + UCTXSTT;             // I2C TX, start condition

		      __bis_SR_register(LPM0_bits + GIE);     // Enter LPM0 w/ interrupts
		      __no_operation();                       // Remain in LPM0 until all data
		                                              // is TX'd
		      UCB0IE &= ~UCTXIE;                         // Disable TX interrupt
		      UCB0CTL1 &= ~UCTR;

		      __delay_cycles(1000000);// >36 ms
}
void hih6131_DF(float * humedad, float * temperatura){//Data fetch para el HIH6131
	          PRxData = (unsigned char *)RxBuffer;    // Start of RX buffer
		      RXByteCtr = 4;                          // Load RX byte counter


		      UCB0IE |= UCRXIE;                         // Disable TX interrupt
		      UCB0CTL1 |= UCTXSTT;             // I2C TX, start condition

		      __bis_SR_register(LPM0_bits + GIE);     // Enter LPM0 w/ interrupts
		      __no_operation();                       // Remain in LPM0 until all data


		  *humedad=(float)((int)(RxBuffer[0]&0x3F)<<8|(int)(RxBuffer[1]))/163.83;
		  *temperatura=(float)((int)RxBuffer[2]<<6|(int)(RxBuffer[3]>>2))/99.29-40;
}

//------------------------------------------------------------------------------
// The USCIAB0_ISR is structured such that it can be used to transmit any
// number of bytes by pre-loading TXByteCtr with the byte count.
//------------------------------------------------------------------------------
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
  case 10:
	      RXByteCtr--;                            // Decrement RX byte counter
	      if (RXByteCtr)
	      {
	        *PRxData++ = UCB0RXBUF;               // Move RX data to address PRxData
	        if (RXByteCtr == 1){                   // Only one byte left?

	        	//generate NACK
	        	//UCB0CTL1 |=UCTXNACK;
	        	UCB0CTL1 |= UCTXSTP;                // Generate I2C stop condition
	        }
	      }
	      else
	      {
	        *PRxData = UCB0RXBUF;                 // Move final RX data to PRxData
	        __bic_SR_register_on_exit(LPM0_bits); // Exit active CPU
	      }


	  break;                           // Vector 10: RXIFG
  case 12:                                  // Vector 12: TXIFG
    if (TXByteCtr)                          // Check TX byte counter
    {
      UCB0TXBUF = TXData;                   // Load TX buffer
      TXByteCtr--;                          // Decrement TX byte counter
    }
    else
    {
      UCB0CTL1 |= UCTXSTP;                  // I2C stop condition
      UCB0IFG &= ~UCTXIFG;                  // Clear USCI_B0 TX int flag
      __bic_SR_register_on_exit(LPM0_bits); // Exit LPM0
    }
    break;
  default: break;
  }
}

#endif //TEMP_HUM

#ifdef TEMP

void InitI2C(char address){//Inicialización de la interfaz I2C
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

void I2CRead(char bytes){//Lectura de los datos I2C
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

void I2CWrite(char bytes){//Escritura de los datos I2C
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

#endif//TEMP


// Interrupción periódica para controlar posibles problemas de comunicación por caida del maestro
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

