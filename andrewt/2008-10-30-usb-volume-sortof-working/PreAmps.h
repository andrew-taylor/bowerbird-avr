/*
 * Drive the pre-amps' digital pots (RDACs) via the TWI/I2C port.
 *
 * Peter Gammie.
 * peteg42 at gmail dot com
 *
 * Commenced October 2008.
 */

#ifndef __PreAmp_H__
#define __PreAmp_H__

#include <avr/io.h>

/* I2C clock in Hz. 400kHz is the limit, I think. */
#define SCL_CLOCK  100000UL

/* The RDACs have TWI addresses 0x58 + 0-3. There are two pots per device.
 * API: number them 0-7, in pairs, i.e. (0, 1) have the same input mic. */

#define RDACS_TWI_ADDR(addr) (0x58 | (addr & 0x06))
#define RDACS_1_OR_3(addr) ((addr) & 1)

/* OR these with the TWI address to indicate data direction. */
// Write to the slave device.
#define TWI_WRITE 0
// Read from the slave device.
#define TWI_READ 1

/* Bits in the TWSR. */

// START has been transmitted
#define START		0x08
// Repeated START has been transmitted
#define	REP_START	0x10

// Master Transmitter staus codes											

// SLA+W transmitted and ACK received	
#define	MTX_ADR_ACK	0x18
// SLA+W transmitted and NACK received
#define	MTX_ADR_NACK	0x20
// Data byte transmitted and ACK received
#define	MTX_DATA_ACK	0x28
// Data byte transmitted and NACK received
#define	MTX_DATA_NACK	0x30
// Arbitration lost in SLA+W or data bytes
#define	MTX_ARB_LOST	0x38

// Master Receiver staus codes	

// Arbitration lost in SLA+R or NACK bit
#define	MRX_ARB_LOST	0x38
// SLA+R transmitted and ACK received
#define	MRX_ADR_ACK	0x40
// SLA+R transmitted and NACK received
#define	MRX_ADR_NACK	0x48
// Data byte received and ACK tramsmitted
#define	MRX_DATA_ACK	0x50
// Data byte received and NACK tramsmitted
#define	MRX_DATA_NACK	0x58

/* Initialise the TWI interface. */
static inline void PreAmps_Init(void)
{
  /* Turn the TWI module on. */
  PRR0 &= ~(1 << PRTWI);

  /* FIXME: turning the TWI stuff on apparently commandeers the PORTD pins.
   * ... but on p222 of the AVR USB data sheet:
   *
   * "Note that the internal pull-ups in the AVR pads can be enabled
   * by setting the PORT bits corresponding to the SCL and SDA pins,
   * as explained in the I/O Port section. The internal pull-ups can
   * in some systems eliminate the need for external ones."
   *
   * the spec sheet doesn't say how to set those PORT bits - perhaps PORTD = ??
   */
  //  PORTD |= PIND0 | PIND1;

  /* TWI timing: no pre-scaler. */
  TWSR = 0;
  TWBR = (F_CPU / SCL_CLOCK - 16) / 2;  /* must be > 10 for stable operation */

  /* Don't bother setting the TWAR - slave address register. */

  /* Enable the TWI module. */
  //  TWCR = (1 << TWEN);
}

/* Send a START condition to the bus and wait for TWINT to be set.
 * Failure: return the TWSR value. Success: -1.
 */
static inline int8_t PreAmps_send_start(void)
{
  // Send START
  //  TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
  TWCR = (1 << TWSTA) | (1 << TWEN);

  // Wait for the TWI interrupt flag.
  while (!(TWCR & (1 << TWINT)))
    ;

  // START or REPEATED_START are both OK, otherwise fail.
  return (TWSR != START) && (TWSR != REP_START) ? TWSR : -1;
}							

/* Sent a slave address and a read/write bit to the bus.
 * Failure: return the TWSR value. Success: -1.
 */
static inline int8_t PreAmps_send_addr(uint8_t addr)
{
  // Wait for the TWI interrupt flag.
  while (!(TWCR & (1 << TWINT)))
    ;

  // Clear TWI interrupt flag to send byte.
  TWDR = addr;
  TWCR = (1 << TWINT) | (1 << TWEN);

  // Wait for the TWI interrupt flag.
  while (!(TWCR & (1 << TWINT)))
    ;

  // Fail if NACK received.
  return (TWSR != MTX_ADR_ACK) && (TWSR != MRX_ADR_ACK)
    ? TWSR
    : -1;
}	

/* Get the current setting for a given preamp. FIXME we expect a value
 * between 0 and 255 (one byte).
 * Return -1 on failure (note the return type).
 */
int16_t PreAmps_get(uint8_t addr)
{
  int RDAC1, RDAC3;

  if(PreAmps_send_start() != -1) {
    goto error;
  }

  if(PreAmps_send_addr(RDACS_TWI_ADDR(addr) | TWI_READ) != -1) {
    goto error;
  }

  // Wait for the TWI interrupt flag.
  while(!(TWCR & (1 << TWINT)))
    ;

  // ACK the first byte (the value of RDAC1).
  TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN);
  // Wait for the TWI interrupt flag.
  while (!(TWCR & (1 << TWINT)))
    ;
  RDAC1 = TWDR;
  if(TWSR != MRX_DATA_ACK)
    goto error;

  // NACK the second byte (the value of RDAC3).
  TWCR = (1 << TWINT) | (1 << TWEN);
  // Wait for the TWI interrupt flag.
  while (!(TWCR & (1 << TWINT)))
    ;
  RDAC3 = TWDR;
  // Wait for the TWI interrupt flag.
  while (!(TWCR & (1 << TWINT)))
    ;
  if(TWSR != MRX_DATA_NACK)
    goto error;

  // Send STOP condition
  TWCR = (1 << TWEN) | (1 << TWINT) | (1 << TWSTO);
  return RDACS_1_OR_3(addr) ? RDAC1 : RDAC3;

 error:
  // Send STOP condition
  TWCR = (1 << TWEN) | (1 << TWINT) | (1 << TWSTO);
  return -1;
}

#endif /* __PreAmp_H__ */
