/*
 * Drive the pre-amps' digital pots (RDACs) via the TWI/I2C port.
 *
 * Note we FIXME need to quietly invert the RDAC value here:
 *
 * RDAC: 0 - full gain, 255 - no gain.
 * USB/this API: 0 - no gain, 255 - full gain.
 *
 * Peter Gammie.
 * peteg42 at gmail dot com
 *
 * Commenced October 2008.
 */

#ifndef __PreAmp_H__
#define __PreAmp_H__

/* I2C/TWI interface. FIXME use the #defines here rather than the home-grown ones. */
#include <util/twi.h>

#include <avr/io.h>

/* I2C clock in Hz. 400kHz is the limit, I think. */
#define SCL_CLOCK  100000UL

/* The RDACs have TWI addresses 0x58 + 0-3. There are two pots per device.
 * API: number them 0-7, in pairs, i.e. (0, 1) have the same input mic. */

#define RDAC_TWI_ADDR(addr) (0x58 | (addr & 0x06))
#define RDAC_1_OR_3(addr) ((addr) & 0x1 ? 0x3 : 0x1)

/* **************************************** */

static inline uint8_t TW_WAIT_INT(void) {
  while (!(TWCR & (1 << TWINT)))
    ;
  return TWSR & 0xF8;
}

/* **************************************** */

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
   *
   * There are pull-ups on the mic board anyway. Setting or resetting
   * these doesn't seem to matter.
   *
   * FIXME we need to control the write-protect bit though. That's
   * PD7. For now, set all of PortD as outputs.
   */
  // PORTD = PIND0 | PIND1;
  DDRD = 0xFF;

  /* TWI timing: no pre-scaler. */
  TWSR = 0;
  TWBR = (F_CPU / SCL_CLOCK - 16) / 2;  /* must be > 10 for stable operation */

  /* Don't bother setting the TWAR - slave address register. */

  /* Enable the TWI module. */
  TWCR = (1 << TWEN);
}

/* Send a START condition to the bus and wait for TWINT to be set.
 * Failure: return the TWSR value. Success: -1.
 */
static inline int8_t TWI_send_start(void)
{
  // Send START
  TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
  uint8_t twsr = TW_WAIT_INT();

  // START or REPEATED_START are both OK, otherwise fail.
  return (twsr == TW_START) || (twsr == TW_REP_START) ? -1 : TWSR;
}

/* Get the current setting for a given preamp.
 *
 * FIXME we expect a value between 0 and 255 (one byte).
 *
 * Returns -1 on failure (note the return type).
 */
static inline int16_t PreAmps_get(const uint8_t addr, uint8_t *value)
{
  /* This is pretty complicated. We need to do an "RDAC Random Read",
   * Figure 29 (p17) on the datasheet.
   */

  uint16_t return_code;
  uint8_t twsr;

  if(TWI_send_start() != -1) {
    return_code = 0x0100;
    goto error;
  }

  // Send slave (chip) address, pretend we're going to write to the RDAC.
  TWDR = RDAC_TWI_ADDR(addr) | TW_WRITE;
  TWCR = (1 << TWINT) | (1 << TWEN);
  twsr = TW_WAIT_INT();

  if(twsr != TW_MT_SLA_ACK) {
    return_code = 0x0200 | twsr;
    goto error;
  }

  // Send the address of the particular RDAC (1 or 3) we want to talk to.
  TWDR = RDAC_1_OR_3(addr);
  TWCR = (1 << TWINT) | (1 << TWEN);
  twsr = TW_WAIT_INT();
  if(twsr != TW_MT_DATA_ACK) {
    return_code = 0x0300 | twsr;
    goto error;
  }

  // "Repeated START" the bus, aborting the write operation.
  if(TWI_send_start() != -1) {
    return_code = 0x0400 | twsr;
    goto error;
  }

  // Send slave (chip) address, now say we're going to read from the RDAC's wiper register.
  TWDR = RDAC_TWI_ADDR(addr) | TW_READ;
  TWCR = (1 << TWINT) | (1 << TWEN);
  twsr = TW_WAIT_INT();

  if(twsr != TW_MR_SLA_ACK) {
    return_code = 0x0500 | twsr;
    goto error;
  }

  // Get a byte back from the RDAC (value of RDACx) (nack - we only want the one value).
  TWCR = (1 << TWINT) | (1 << TWEN);
  twsr = TW_WAIT_INT();
  // *NOTE* Invert the result.
  *value = ~TWDR;
  return_code = 0;

  if(twsr != TW_MR_DATA_NACK) {
    return_code = 0x0600 | twsr;
    goto error;
  }

 error:

  // Send STOP condition
  TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
  return return_code;
}

/* Set a value for a given preamp.
 * Returns 0 on success.
 */
static inline int16_t PreAmps_set(const uint8_t addr, const uint8_t val)
{
  int16_t retval = 0xCAFE;
  uint8_t twsr;

  // FIXME Set the write-enable bit.
  PORTD = 0xFF;

  if(TWI_send_start() != -1) {
    retval = 0x0100;
    goto error;
  }

  // Send slave (chip) address, say we're going to write to the RDAC.
  TWDR = RDAC_TWI_ADDR(addr) | TW_WRITE;
  TWCR = (1 << TWINT) | (1 << TWEN);
  twsr = TW_WAIT_INT();

  if(twsr != TW_MT_SLA_ACK) {
    retval = 0x0200 | twsr;
    goto error;
  }

  // Send the address of the particular RDAC (1 or 3) we want to talk to.
  TWDR = RDAC_1_OR_3(addr);
  TWCR = (1 << TWINT) | (1 << TWEN);
  twsr = TW_WAIT_INT();

  if(twsr != TW_MT_DATA_ACK) {
    retval = 0x0300 | twsr;
    goto error;
  }

  // Write to the RDAC's wiper register.
  // *NOTE* Invert the value.
  TWDR = ~val;
  TWCR = (1 << TWINT) | (1 << TWEN);
  twsr = TW_WAIT_INT();
  if(twsr != TW_MT_DATA_ACK) {
    retval = 0x0400 | twsr;
    goto error;
  }

 error:

  // FIXME Reset the write-enable bit.
  PORTD = 0x0;

  // Send STOP condition
  TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
  return retval;
}

#endif /* __PreAmp_H__ */
