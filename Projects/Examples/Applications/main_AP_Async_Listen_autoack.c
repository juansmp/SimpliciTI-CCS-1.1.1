/*----------------------------------------------------------------------------
 *  Aplicación para sensores inalámbricos de temperatura y humedad v
 *
 *  J. S. Morant
 *  Netux S.A.S.
 *---------------------------------------------------------------------------- */
///////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////Parámetros de configuración del maestro////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
#define ID_UBIDOTS 13726
#define BASE_TIEMPO ID_2 //ID_1 = 4 SEGUNDOS, ID_2 = 8 SEGUNDOS, ID_3 = 16 SEGUNDOS
#define PERIODOS 1       //Periodo de la aplicación = BASE_TIEMPO*PERIODOS
//#define rssitest 1       //COMENTAR PARA DESHABILITAR PRUEBAS DE RSSI
#define CLOUDTHINX 1	 //=1 para CLOUDTHINX, !=1 para el kit de desarrollo
#define FIRMWARE_VERSION "--v130408.1\n"
///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////


#include <string.h>
#include "bsp.h"
#include "mrfi.h"
#include "nwk_types.h"
#include "nwk_api.h"
#include "nwk_frame.h"
#include "nwk.h"
#include "stdio.h"

unsigned char cont=0;
char buffer[200]={'$','0','0','\n'};



void uartSend(char string[], char size);
void uartInit(void);
unsigned char bufferSize=0;
int charReceived;

unsigned char tids[250]={0};

int myid=ID_UBIDOTS;


unsigned char cnt_rst=0;

/**************************** COMMENTS ON ASYNC LISTEN APPLICATION ***********************
Summary:
  This AP build includes implementation of an unknown number of end device peers in
  addition to AP functionality. In this scenario all End Devices establish a link to
  the AP and only to the AP. The AP acts as a data hub. All End Device peers are on
  the AP and not on other distinct ED platforms.

  There is still a limit to the number of peers supported on the AP that is defined
  by the macro NUM_CONNECTIONS. The AP will support NUM_CONNECTIONS or fewer peers
  but the exact number does not need to be known at build time.

  In this special but common scenario SimpliciTI restricts each End Device object to a
  single connection to the AP. If multiple logical connections are required these must
  be accommodated by supporting contexts in the application payload itself.

Solution overview:
  When a new peer connection is required the AP main loop must be notified. In essence
  the main loop polls a semaphore to know whether to begin listening for a peer Link
  request from a new End Device. There are two solutions: automatic notification and
  external notification. The only difference between the automatic notification
  solution and the external notification solution is how the listen semaphore is
  set. In the external notification solution the sempahore is set by the user when the
  AP is stimulated for example by a button press or a commend over a serial link. In the
  automatic scheme the notification is accomplished as a side effect of a new End Device
  joining.

  The Rx callback must be implemented. When the callback is invoked with a non-zero
  Link ID the handler could set a semaphore that alerts the main work loop that a
  SMPL_Receive() can be executed successfully on that Link ID.

  If the callback conveys an argument (LinkID) of 0 then a new device has joined the
  network. A SMPL_LinkListen() should be executed.

  Whether the joining device supports ED objects is indirectly inferred on the joining
  device from the setting of the NUM_CONNECTIONS macro. The value of this macro should
  be non-zero only if ED objects exist on the device. This macro is always non-zero
  for ED-only devices. But Range Extenders may or may not support ED objects. The macro
  should be be set to 0 for REs that do not also support ED objects. This prevents the
  Access Point from reserving resources for a joinng device that does not support any
  End Device Objects and it prevents the AP from executing a SMPL_LinkListen(). The
  Access Point will not ever see a Link frame if the joining device does not support
  any connections.

  Each joining device must execute a SMPL_Link() after receiving the join reply from the
  Access Point. The Access Point will be listening.

*************************** END COMMENTS ON ASYNC LISTEN APPLICATION ********************/

/************  THIS SOURCE FILE REPRESENTS THE AUTOMATIC NOTIFICATION SOLUTION ************/

/* reserve space for the maximum possible peer Link IDs */
static linkID_t sLID[NUM_CONNECTIONS] = {0};

static struct mystruct
 {
     int UbiID;
     char sid;
 } paqueterep[NUM_CONNECTIONS]={0,0};

 static char PaqueteActual=0; //Contiene la posición actual de paqueterep
 static char RecorrerPaquetes=0;//Sirve para recorrer paquetes para identificar repetidos
 static char rep=0; //bandera para identificar si el paquete está repetido o no (rep=0 paquete NO repetido, rep=1 paquete repetido)


//static linkID_t paqueterep[NUM_CONNECTIONS] = {0};
static uint8_t  sNumCurrentPeers = 0;

/* callback handler */
static uint8_t sCB(linkID_t);

/* received message handler */
static void processMessage(linkID_t, uint8_t *, uint8_t);

/* work loop semaphores */
static volatile uint8_t sPeerFrameSem = 0;
static volatile uint8_t sJoinSem = 0;

#ifdef FREQUENCY_AGILITY
/*************** BEGIN interference detection support */

#define INTERFERNCE_THRESHOLD_DBM (-70)
#define SSIZE    25
#define IN_A_ROW  3
static int8_t  sSample[SSIZE];
static uint8_t sChannel = 0;
#endif  /* FREQUENCY_AGILITY */

/*     ************** END interference detection support                       */


/* Variables para la comunicación serial*/
unsigned char time=0,data_length=0;

#define SPIN_ABOUT_A_QUARTER_SECOND   NWK_DELAY(250)



void main (void)
{
  bspIState_t intState;
#ifdef FREQUENCY_AGILITY
  memset(sSample, 0x0, sizeof(sSample));
#endif  /* FREQUENCY_AGILITY */
  BSP_Init();

  uartInit();//Inicialización de la interfaz serial de comunicaciones.

  TA1CTL = TASSEL_1 + MC_2 + TACLR + TAIE + BASE_TIEMPO;  // ACLK, contmode, clear TAR  // enable interrupt

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

  SMPL_Init(sCB);


  /* main work loop */
  while (1)
  {
    /* Wait for the Join semaphore to be set by the receipt of a Join frame from a
     * device that supports an End Device.
     *
     * An external method could be used as well. A button press could be connected
     * to an ISR and the ISR could set a semaphore that is checked by a function
     * call here, or a command shell running in support of a serial connection
     * could set a semaphore that is checked by a function call.
     */
    if (sJoinSem && (sNumCurrentPeers < NUM_CONNECTIONS))
    {
      /* listen for a new connection */
      while (1)
      {
        if (SMPL_SUCCESS == SMPL_LinkListen(&sLID[sNumCurrentPeers]))
        {
          break;
        }
        /* Implement fail-to-link policy here. otherwise, listen again. */
      }

      sNumCurrentPeers++;

      BSP_ENTER_CRITICAL_SECTION(intState);
      sJoinSem--;
      BSP_EXIT_CRITICAL_SECTION(intState);
    }

    /* Have we received a frame on one of the ED connections?
     * No critical section -- it doesn't really matter much if we miss a poll
     */
    if (sPeerFrameSem)
    {
      uint8_t     msg[MAX_APP_PAYLOAD], len, i;

      /* process all frames waiting */
      for (i=0; i<sNumCurrentPeers; ++i)
      {
        if (SMPL_SUCCESS == SMPL_Receive(sLID[i], msg, &len))
        {
          processMessage(sLID[i], msg, len);

          BSP_ENTER_CRITICAL_SECTION(intState);
          sPeerFrameSem--;
          BSP_EXIT_CRITICAL_SECTION(intState);
        }
      }
    }


  }

}


/* Runs in ISR context. Reading the frame should be done in the */
/* application thread not in the ISR thread. */
static uint8_t sCB(linkID_t lid)
{
  if (lid)
  {
    sPeerFrameSem++;

  }
  else
  {
    sJoinSem++;
  }

  /* leave frame to be read by application. */
  return 0;
}

static void processMessage(linkID_t lid, uint8_t *msg, uint8_t len)
{
	int tempid,humedadid,temperaturaent,humedadent;

	#ifdef rssitest //Imprimir el rssi de cada paquete que llega
		int rssiint=0;
		connInfo_t  *pCInforssi = nwk_getConnInfo(lid);
		rssiint=pCInforssi->sigInfo.rssi; // I added this one
		char bufferrssi[28]={'$','0','0','\n'};
		sprintf(bufferrssi,"rssi: %d, lid %d, sid %d\n",rssiint,lid,msg[2]);
		bufferrssi[27]='\n';
		uartSend(bufferrssi,28);
	#endif //rssitest

	rep=0;
	PaqueteActual=(PaqueteActual<(NUM_CONNECTIONS-1))?(PaqueteActual+1):0;

	paqueterep[PaqueteActual].UbiID=(int)((msg[3])*256+msg[4]);
	paqueterep[PaqueteActual].sid=msg[2];

	for(RecorrerPaquetes=((PaqueteActual==0)?(NUM_CONNECTIONS-1):(PaqueteActual-1));;){

			if((paqueterep[PaqueteActual].UbiID)==(paqueterep[RecorrerPaquetes].UbiID)){
				if((paqueterep[PaqueteActual].sid)==(paqueterep[RecorrerPaquetes].sid)){
					rep=1; //paquete repetido
					break;
				}else{
					rep=0; //paquete NO repetido
					break;
				}
			}else{
				RecorrerPaquetes=((RecorrerPaquetes==0)?(NUM_CONNECTIONS-1):(RecorrerPaquetes-1));//Decremento el contador
			}
			if(RecorrerPaquetes==PaqueteActual){
				rep=0; //paquete NO repetido
				break;
			}
	}


if(rep==0){
  if (len)
  {
	   if(cont<170)//para evitar violación de segmento por desborde del buffer de la uart
		{
		switch (msg[0]){
			  case 'T':
				  data_length+=1;
				  temperaturaent=(int)(msg[5])*256+msg[6];
				  tempid=(int)(msg[3])*256+msg[4];
				  if(buffer[1]!='X'){
					  sprintf(buffer,"$XX,%d,%d\n",tempid,temperaturaent); //Se pone XX en el tamaño de los datos mientras se define
					  for(cont=0;buffer[cont]!='\n';cont++);
				  }else{
					  sprintf(buffer+cont,",%d,%d\n",tempid,temperaturaent);
					  for(cont=0;buffer[cont]!='\n';cont++);
				  }
				  break;
			  case 'H':
				  data_length+=2;
				  temperaturaent=(int)(msg[9])*256+msg[10];
				  tempid=(int)(msg[7])*256+msg[8];
				  humedadent=(int)(msg[5])*256+msg[6];
				  humedadid=(int)(msg[3])*256+msg[4];
                  if(temperaturaent==-400){
					  data_length-=1;
					  if(buffer[1]!='X'){
								  sprintf(buffer,"$XX,%d,%d:\n",myid,tempid);
								  for(cont=0;buffer[cont]!='\n';cont++);
							   }else{
								  sprintf(buffer+cont,",%d,%d:\n",myid,tempid);
								  for(cont=0;buffer[cont]!='\n';cont++);
							  }
				  }else{
				    if(buffer[1]!='X'){
						  sprintf(buffer,"$XX,%d,%d,%d,%d\n",tempid,temperaturaent,humedadid,humedadent);
						  for(cont=0;buffer[cont]!='\n';cont++);
					  }else{
						  sprintf(buffer+cont,",%d,%d,%d,%d\n",tempid,temperaturaent,humedadid,humedadent);
						  for(cont=0;buffer[cont]!='\n';cont++);
					  }
					}

				  break;
			  case 'F':
				  data_length+=1;
				  tempid=(int)(msg[3])*256+msg[4];
				  if(buffer[1]!='X'){
					  sprintf(buffer,"$XX,%d,%d:\n",myid,tempid);
					  for(cont=0;buffer[cont]!='\n';cont++);
				   }else{
					  sprintf(buffer+cont,",%d,%d:\n",myid,tempid);
					  for(cont=0;buffer[cont]!='\n';cont++);
				  }
				  break;
			  default:
				  break;
			  }//switch (msg[0])
	  }//if(cont<170)
	  if(cont>170) //Está próximo a ocurrir un desborde del buffer, por lo que se envía la información por serial y se vacía
	  {
	  	  	  	  	  time=0;

		  	  	  	  buffer[1]=((data_length/10)%10)+48;
		              buffer[2]=(data_length%10)+48;
		              data_length=0;
		              for(cont=0;buffer[cont]!='\n';cont++);
		              uartSend(buffer,cont+1);
		              cont=0;
		              buffer[3]='\n';
	 }//if(cont>170)
  }//if (len)
}//paquete no repetido
  return;
}

//Función para enviar una cadena de caracteres por el puerto serial
void uartSend(char string[], char size)
{
  int i;
  int TA1CTL_temp=TA1CTL;
  
TA1CTL &= ~TAIE;//deshabilito interrupciones mientras envío info por serial   

for (i=0; i<size; i++)
  {
   while (!(UCA0IFG & UCTXIFG));
   UCA0TXBUF = string[i];
  }
TA1CTL=TA1CTL_temp;//Restablezco la información

}

//Inicialización de la interfaz serial
void uartInit(void)
{
unsigned int i;
WDTCTL = WDTPW+WDTHOLD;                   // Stop watchdog timer
P5SEL |= 0x03;                            // Enable XT1 pins
  do
  {
    UCSCTL7 &= ~(XT1LFOFFG + DCOFFG);
                                            // Clear XT2,XT1,DCO fault flags
    SFRIFG1 &= ~OFIFG;                      // Clear fault flags
    for(i=0;i<10000;i++);                  // Delay for Osc to stabilize
  }while (SFRIFG1&OFIFG);                   // Test oscillator fault flag
  //P1OUT = 0x000;                            // P1.0/1 setup for LED output
  //P1DIR |= BIT0+BIT1;                       //

#if CLOUDTHINX
	  PMAPPWD = 0x02D52;                        // Get write-access to port mapping regs
	  P3MAP4 = PM_UCA0RXD;                      // Map UCA0RXD output to P3.4
	  P3MAP5 = PM_UCA0TXD;                      // Map UCA0TXD output to P3.5
         // P2MAP4 = PM_MCLK;                         // Map MCLK output to P2.4
	  PMAPPWD = 0;                              // Lock port mapping registers

	  P3DIR |= BIT5;// + BIT4;                            // Set P3.5 as TX output
	  P3SEL |= BIT4 + BIT5;// + BIT4;                     // Select P3.4 & P3.5 to UART function
#else
      PMAPPWD = 0x02D52;                        // Get write-access to port mapping regs
	  P3MAP7 = PM_UCA0RXD;                      // Map UCA0RXD output to P3.7
	  P3MAP2 = PM_UCA0TXD;                      // Map UCA0TXD output to P3.2
         // P2MAP4 = PM_MCLK;                         // Map MCLK output to P2.4
	  PMAPPWD = 0;                              // Lock port mapping registers

	  P3DIR |= BIT2;// + BIT4;                            // Set P3.2 as TX output
	  P3SEL |= BIT2 + BIT7;// + BIT4;                     // Select P3.2 & P3.7 to UART function
#endif
	  UCA0CTL1 |= UCSWRST;                      // **Put state machine in reset**
	  UCA0CTL1 |= UCSSEL_1;                     // CLK = ACLK
	  UCA0BR0 = 0x03;                           // 32k/9600 - 3.41
	  UCA0BR1 = 0x00;                           //
	  UCA0MCTL = 0x06;                          // Modulation
	  UCA0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**
	  //UCA0IE |= UCTXIE;		            // Enable USCI_A0 TX interrupt
	  UCA0IE |= UCRXIE;                // Enable USCI_A0 RX interrupt

  //_BIS_SR(GIE);                         // Enable Interrupts
  __bis_SR_register(GIE);       // interrupts enabled
  uartSend(FIRMWARE_VERSION,12);
  uartSend("--Cloudthinx Ready ",19);
  sprintf(buffer,"%d\n",myid);
  uartSend(buffer,6);
  sprintf(buffer,"$00\n");

}


// Timer para enviar periódicamente la información recolectada de los nodos
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
    	if(time<PERIODOS){
    		time++;
    	}else{
    		buffer[1]=((data_length/10)%10)+48;
            buffer[2]=(data_length%10)+48;
            if((data_length%10)==0 && (data_length/10)==0){//
            	buffer[3]='\n';
            }
            data_length=0;
            for(cont=0;buffer[cont]!='\n';cont++);
            uartSend(buffer,cont+1);
            time=0;
            buffer[3]='\n';
            if(cont<6){//Cuando se envían 30 $00 seguidos reinicio el CLOUDTHINX
                if(cnt_rst<30){
						cnt_rst++;
						if(cnt_rst==15){
							uartSend("--3minutes idle\n",16);
						}
            	}else{
            			cnt_rst=0;
            			uartSend("--7 minutes idle reset\n",23);
            			SPIN_ABOUT_A_QUARTER_SECOND;
						PMMCTL0=PMMPW+PMMSWBOR;
				}
			}else{
				cnt_rst=0;
			}
    	}
             break;
    default: break;
  }
}

