/***************************************************************
 *
 * OpenBeacon.org 2.4GHz proximity tag - main entry, CRC, behaviour
 *
 * Copyright (C) 2011 Milosch Meriac <meriac@openbeacon.de>
 * - Unified Proximity/Tracking-Mode into one Firmware
 * - Increased Tracking/Proximity rate to 12 packets per second
 * - Implemented Tag-ID assignment/management via RF interface to
 *   simplify production process
 *
 * Copyright (C) 2008 Istituto per l'Interscambio Scientifico I.S.I.
 * - extended by Ciro Cattuto <ciro.cattuto@gmail.com> by support for
 *   SocioPatterns.org platform
 *
 * Copyright (C) 2006 Milosch Meriac <meriac@openbeacon.de>
 * - Optimized en-/decryption routines, CRC's, EEPROM handling
 *
/***************************************************************

/*
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*/
 
//#include <htc.h> //PICC thing
#include <stdlib.h>
#include "config.h"
#include "timer.h"
#include "xxtea.h"
#include "nRF_CMD.h"
#include "nRF_HW.h"
#include "main.h"
#include  <msp430x22x4.h>
#include "TI_USCI_I2C_master.h"
//#include "mma8452.h"
//#include "MMA8452Q.h"

#define XXTEA_ROUNDS (6UL + 52UL / XXTEA_BLOCK_COUNT)
#define DELTA 0x9E3779B9UL
#define FlashAddress (char *)0x1001

// key is set in create_counted_firmware.php while patching firmware hex file
volatile const u_int32_t oid = 0xFFFFFFFF;
volatile const u_int32_t seed = 0xFFFFFFFF;
const u_int32_t xxtea_key[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };

static u_int32_t seq = 0;
static u_int16_t oid_ram = 0;
static u_int16_t code_block;
static u_int8_t global_flags = 0;

static u_int16_t oid_seen[PROX_MAX], oid_last_seen;
static u_int8_t oid_seen_count[PROX_MAX], oid_seen_pwr[PROX_MAX];
static u_int8_t seen_count = 0;

static u_int32_t z, y, sum;
static u_int8_t e;
static s_int8_t p;

static TBeaconEnvelope pkt;

unsigned char flashblock[8];

#define ntohs htons
static u_int16_t
htons (u_int16_t src)
{
  u_int16_t res;

  ((unsigned char *) &res)[0] = ((unsigned char *) &src)[1];
  ((unsigned char *) &res)[1] = ((unsigned char *) &src)[0];

  return res;
}

#define ntohl htonl
static u_int32_t
htonl (u_int32_t src)
{
  u_int32_t res;

  ((u_int8_t *) & res)[0] = ((u_int8_t *) & src)[3];
  ((u_int8_t *) & res)[1] = ((u_int8_t *) & src)[2];
  ((u_int8_t *) & res)[2] = ((u_int8_t *) & src)[1];
  ((u_int8_t *) & res)[3] = ((u_int8_t *) & src)[0];

  return res;
}

static void
store_incremented_codeblock (void)
{
  u_int16_t temp;
	
  code_block++;
	
	flashblock[0] = (char)(oid_ram>>12);
	flashblock[1] = (char)(oid_ram>>8);
	flashblock[2] = (char)(oid_ram>>4);
	flashblock[3] = (char)(oid_ram);
	flashblock[4] = (char)(code_block>>12);
	flashblock[5] = (char)(code_block>>8);
	flashblock[6] = (char)(code_block>>4);
	flashblock[7] = (char)(code_block);
	
	flash_store(flashblock);
}

static u_int16_t
crc16 (const unsigned char *buffer, unsigned char size)
{
  unsigned short crc = 0xFFFF;
  if (buffer)
    {
      while (size--)
	{
	  crc = (crc >> 8) | (crc << 8);
	  crc ^= *buffer++;
	  crc ^= ((unsigned char) crc) >> 4;
	  crc ^= crc << 12;
	  crc ^= (crc & 0xFF) << 5;
	}
    }
  return crc;
}

static void
protocol_fix_endianess (void)
{
  for (p = 0; p < XXTEA_BLOCK_COUNT; p++)
    pkt.block[p] = htonl (pkt.block[p]);
}

static u_int32_t
protocol_mx (void)
{
  return ((z >> 5 ^ y << 2) + (y >> 3 ^ z << 4) ^ (sum ^ y) +
	  (xxtea_key[p & 3 ^ e] ^ z));
}

static void
protocol_encode (void)
{
  unsigned char i;

  protocol_fix_endianess ();

  z = pkt.block[XXTEA_BLOCK_COUNT - 1];
  sum = 0;

  for (i = XXTEA_ROUNDS; i > 0; i--)
    {
      sum += DELTA;
      e = sum >> 2 & 3;

      for (p = 0; p < XXTEA_BLOCK_COUNT; p++)
	{
	  y = pkt.block[(p + 1) & (XXTEA_BLOCK_COUNT - 1)];
	  z = pkt.block[p] += protocol_mx ();
	}
    }

  protocol_fix_endianess ();
}

static void
protocol_decode (void)
{
  protocol_fix_endianess ();

  y = pkt.block[0];
  sum = XXTEA_ROUNDS * DELTA;

  while (sum)
    {
      e = (sum >> 2) & 3;

      for (p = XXTEA_BLOCK_COUNT - 1; p >= 0; p--)
	{
	  z = pkt.block[(p - 1) & (XXTEA_BLOCK_COUNT - 1)];
	  y = pkt.block[p] -= protocol_mx ();
	}

      sum -= DELTA;
    }

  protocol_fix_endianess ();
}


static void
protocol_process_packet (void)
{
  u_int8_t j, insert = 0;
  u_int16_t tmp_oid;

  oid_last_seen = htons (pkt.hdr.oid);

  /* assign new OID if a write request has been seen for our ID */
  if((pkt.hdr.flags & RFBFLAGS_OID_WRITE) && (oid_last_seen==oid_ram))
  {
    tmp_oid = ntohs(pkt.tracker.oid_last_seen);
    /* only write to EEPROM if the ID actually changed */
    if(tmp_oid!=oid_ram)
    {
      oid_ram = tmp_oid;
		
		flashblock[0] = (char)(oid_ram>>12);
		flashblock[1] = (char)(oid_ram>>8);
		flashblock[2] = (char)(oid_ram>>4);
		flashblock[3] = (char)(oid_ram);
		flashblock[4] = (char)(code_block>>12);
		flashblock[5] = (char)(code_block>>8);
		flashblock[6] = (char)(code_block>>4);
		flashblock[7] = (char)(code_block);
		
		flash_store(flashblock);
		
      global_flags |= RFBFLAGS_OID_UPDATED;
    }

    /* reset counters */
    seq = 0;
    code_block = 0;
    store_incremented_codeblock();

    /* confirmation-blink for 0.3s */
	  LED1_DIM;
    sleep_jiffies (100* TIMER1_JIFFIES_PER_MS);
	  LED1_LIT;
    sleep_jiffies (100* TIMER1_JIFFIES_PER_MS);
	  LED1_DIM;
    sleep_jiffies (100* TIMER1_JIFFIES_PER_MS);
	  LED1_LIT;

    return;
  }

  /* ignore OIDs outside of mask */
  if(oid_last_seen>PROX_TAG_ID_MASK)
    return;

  for (j = 0; (j < seen_count) && (oid_seen[j] != oid_last_seen); j++);

  if (j < seen_count)
    {

      if (pkt.tracker.strength < oid_seen_pwr[j])
	insert = 1;
      else if ((pkt.tracker.strength == oid_seen_pwr[j])
	       && (oid_seen_count[j] < PROX_TAG_COUNT_MASK))
	oid_seen_count[j]++;

    }
  else
    {

      if (seen_count < PROX_MAX)
	{
	  insert = 1;
	  seen_count++;
	}
      else
	{
	  for (j = 0; j < PROX_MAX; j++)
	    if (pkt.tracker.strength < oid_seen_pwr[j])
	      {
		insert = 1;
		break;
	      }
	}
    }

  if (insert)
    {
      oid_seen[j] = oid_last_seen;
      oid_seen_count[j] = 1;
      oid_seen_pwr[j] = (pkt.tracker.strength>PROX_TAG_STRENGTH_MASK) ?
	PROX_TAG_STRENGTH_MASK:pkt.tracker.strength;
    }
}

//! Returns 1 if NRF is connected.
int nrf_ready(){
	unsigned char channel=81;
	nRFCMD_RegWrite(5,&channel,1);
	channel=12;
	nRFCMD_RegRead(5,&channel,1);
	
	return channel==81;
}


void
main (void) 
{
  u_int8_t j, strength;
  u_int16_t crc;
  u_int8_t clicked = 0;
  char *p;
  // Stop WDT
  WDTCTL = WDTPW + WDTHOLD;

  //LED setup
  PLEDDIR=LEDBITS;
  PLED|=LEDBITS;
   
  //BUTTON setup
  P1DIR &=~0x04;

  PDIR=MOSI+SCK;//+CSN+CE;//Radio outputs
  P2DIR = CSN+CE+BIT2; //Changed to PORT 2 for new radio..

  P2OUT=CSN; //chip unselected, radio off.       POUT --> P2OUT

  PDIR |= IRQ; //Testing -- need to turn internal radio off..
  POUT |= IRQ; // As above...

  msp430_init_uart();   //
  //serial_tx(162);       //Check if character is sent on UART @ 115200
  //serial_tx('A');

	
////////////////////////////////////////////////////////////////////////////////
// Loads flash with OID and CODEBLOCK
 
  flashblock[0] = 0xF;    //
  flashblock[1] = 0xF;
  flashblock[2] = 0xF;
  flashblock[3] = 0xF;
  flashblock[4] = 0x0;
  flashblock[5] = 0x0;
  flashblock[6] = 0x0;
  flashblock[7] = 0x0;
	
  flash_store(flashblock);
	
////////////////////////////////////////////////////////////////////////////////
  do{
	  unsigned char timercounter;   
	  unsigned char array[40] = { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb };   
	  unsigned char store[40] = { 13, 13, 13, 13, 13, 13, 13, 0, 0, 0}; 
	  for(;;){ 
		  WDTCTL = WDTPW + WDTHOLD;                 // Stop WDT   
		  
		  BCSCTL1 = CALBC1_8MHZ;    
		  DCOCTL = CALDCO_8MHZ;   
		  
		  _EINT();   
		  
		  TI_USCI_I2C_transmitinit(0x1C,0x12);  // init transmitting with USCI    
		  while ( TI_USCI_I2C_notready() );         // wait for bus to be free   
		  if ( TI_USCI_I2C_slave_present(0x1C) )    // slave address may differ from   
		  {                                         // initialization 
			  //LED1_LIT;
			  TI_USCI_I2C_receiveinit(0x1C,0x12);   // init receiving with USCI   
			  while ( TI_USCI_I2C_notready() );         // wait for bus to be free   
			  TI_USCI_I2C_receive(4,store);   
			  while ( TI_USCI_I2C_notready() );         // wait for bus to be free   
			  
			  TI_USCI_I2C_transmitinit(0x1C,0x12);  // init transmitting with    
			  while ( TI_USCI_I2C_notready() );         // wait for bus to be free  
			  TI_USCI_I2C_transmit(4,array);       // start transmitting  
			  
		  }   
		  LED1_LIT;
		  LPM3;
		  		  
		 
		  
		  //int a,b;						 //Secontion of code writes to the status register
		  //a = 1;						 //and reads from the status register
		  //nRFCMD_RegReadWrite(STATUS, a);//a is what is sent and b is what is read.
		  //b = nRFCMD_RegGet(STATUS);//a and b should be identical for proper connection.	  
		  //LED1_LIT;//If a and b are not the same, this suggest one of the SPI						  
		  //LED2_LIT;//pins or more are not joined properly.						 
		  //LED3_LIT;//When this happens LED2 will light up.
		  //if (a^b !=0)
			  //LED2_DIM;
		  //nRFCMD_Init ();
		  
		//serial_tx(161);
		  //LED2_LIT;
	  }
  }while(!nrf_ready());
  for (j = 0; j <= 50; j++)// This is the flashing part
//		if(j&1){
//			LED1_DIM;} 
//		else{
//			LED1_LIT;}
//
//      sleep_jiffies (250* TIMER1_JIFFIES_PER_MS); //Jiffies creates randomness
//    }
	LED2_LIT; //save power..
//Between the forward slashes are changes in code
//	P1DIR |= 0x03;
		
	//P1OUT = (P3IN&CSN == 0) ? 0x01 : 0x02;
	
//	else if (CSN != 0 || CSN != 1){
//		P1OUT |= 0x03;
//	}
//	else if (CSN == 0 && CSN == 1){
//		P1OUT |= 0x03;
//	}
//

  // get tag OID out of EEPROM or FLASH
	oid_ram = flash_read16((char *) 0x1001);
	
  if(oid_ram==0xFFFF)
    oid_ram = (u_int16_t)oid;
  else
    global_flags|=RFBFLAGS_OID_UPDATED;

  // increment code block after power cycle
	code_block = flash_read16((char *) 0x1005);

  store_incremented_codeblock ();

  // update random seed
  seq = code_block ^ oid_ram ^ seed;
  srand (crc16 ((unsigned char *) &seq, sizeof (seq)));

  // increment code blocks to make sure that seq is
  // higher or equal after battery change
  seq = ((u_int32_t) (code_block - 1)) << 16;

  while (1)// sending and recieving of data starts from here.
    {
      // random delay to make opaque tracking based on
      // timer deviation difficult
	  sleep_jiffies(200* TIMER1_JIFFIES_PER_MS);
	  //serial_tx(161);
		//LED2_DIM;
		//if (P2IN & 0b00000100)
			//P1OUT &= 0b00000010;

      /* --------- perform RX ----------------------- */
      if ( ((u_int8_t)seq) & 1)
	{
	  nRFCMD_StartRX ();
		CONFIG_PIN_CE_HIGH;
	  sleep_jiffies (5* TIMER1_JIFFIES_PER_MS);
		CONFIG_PIN_CE_LOW;
		sleep_jiffies (5* TIMER1_JIFFIES_PER_MS);

	  nRFCMD_Stop ();
		
	  while ((nRFCMD_RegGet (FIFO_STATUS) & NRF_FIFO_RX_EMPTY) == 0)
	    {
			// receive raw data
	      nRFCMD_RegRead (RD_RX_PLOAD, pkt.byte, sizeof (pkt.byte));
	      nRFCMD_RegReadWrite (STATUS | WRITE_REG, NRF_CONFIG_MASK_RX_DR);
			
	      // decrypt data
	      //protocol_decode ();
			//serial_tx(161); 
	      // verify the crc checksum
	      crc = crc16 (pkt.byte, sizeof (pkt.tracker) - sizeof (pkt.tracker.crc));

	      // only handle RFBPROTO_PROXTRACKER packets
	      if (htons (crc) == pkt.tracker.crc && (pkt.hdr.proto == RFBPROTO_PROXTRACKER))
		protocol_process_packet ();
	    }
	}
		
      /* populate common fields */

		pkt.hdr.oid = htons (oid_ram);
      pkt.hdr.flags = global_flags;
      if(clicked)
        pkt.hdr.flags |= RFBFLAGS_SENSOR;
		

      /* --------- perform TX ----------------------- */
      if ( ((u_int8_t)seq) & 1 )
	{
	  strength = (seq & 2) ? 1 : 2;
	  nRFCMD_RegReadWrite (RF_CH | WRITE_REG,
			       CONFIG_PROX_CHANNEL);
	  pkt.hdr.proto = RFBPROTO_PROXTRACKER;
	  pkt.tracker.oid_last_seen = 0;
	}
      else
	{
	  nRFCMD_RegReadWrite (RF_CH | WRITE_REG,
			       CONFIG_TRACKER_CHANNEL);
	  strength = (((unsigned char) seq) >> 1) & 0x03;

	  pkt.hdr.proto = RFBPROTO_BEACONTRACKER;
	  pkt.tracker.oid_last_seen = htons (oid_last_seen);
	  oid_last_seen = 0;
	}
	  //serial_tx(162);
      /* for lower strength send normal tracking packet */
      if(strength<3)
      {
	pkt.tracker.strength = strength;
	pkt.tracker.powerup_count = htons (code_block);
	pkt.tracker.reserved = 0;
	pkt.tracker.seq = htonl (seq);
      }
      else
      /* for highest strength send proximity report */
      {
	pkt.hdr.proto = RFBPROTO_PROXREPORT_EXT;
	for (j = 0; j < PROX_MAX; j++)
	    pkt.prox.oid_prox[j] = (j < seen_count) ? (htons (
		(oid_seen[j]) |
		    (oid_seen_count[j] << PROX_TAG_ID_BITS) |
		    (oid_seen_pwr[j] << (PROX_TAG_ID_BITS+PROX_TAG_COUNT_BITS))
		    )) : 0;
	pkt.prox.short_seq = htons ((u_int16_t) seq);
	seen_count = 0;
      }

      // add CRC to packet
      crc = crc16 (pkt.byte, sizeof (pkt.tracker) - sizeof (pkt.tracker.crc));
      pkt.tracker.crc = htons (crc);

      // adjust transmit strength
      nRFCMD_RegReadWrite (RF_SETUP | WRITE_REG,
			   NRF_RFOPTIONS | (strength << 1));

      // encrypt the data
      //protocol_encode ();    Havent written decryption on node.js server

      // transmit data to nRF24L01 chip
      nRFCMD_RegWrite (WR_TX_PLOAD | WRITE_REG, (unsigned char *) &pkt,
		       sizeof (pkt));
	
      // send data away
      nRFCMD_Execute ();

      // update code_block so on next power up
      // the seq will be higher or equal
      // TODO: wrapping
      crc = (unsigned short) (seq >> 16);
      if (crc >= code_block)
	    store_incremented_codeblock ();

      /* --------- blinking pattern ----------------------- */
		if (( seq & ( clicked ? 1 : 0x1F ) ) == 0) 
		  LED1_DIM;

      // blink for 1ms
      sleep_jiffies (1*TIMER1_JIFFIES_PER_MS);

      // disable LED
		LED1_LIT;
		
		/* --------- handle click ----------------------- */
      if(P1IN&0x04)
	{
	  clicked = 4;
	}
		
      if (clicked > 0)
	  clicked--;

      // finally increase sequence number
      seq++;
    }
}
