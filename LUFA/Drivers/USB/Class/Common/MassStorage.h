/*
             LUFA Library
     Copyright (C) Dean Camera, 2009.
              
  dean [at] fourwalledcubicle [dot] com
      www.fourwalledcubicle.com
*/

/*
  Copyright 2009  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, and distribute this software
  and its documentation for any purpose and without fee is hereby
  granted, provided that the above copyright notice appear in all
  copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \ingroup Group_USBClassMS
 *  @defgroup Group_USBClassMSCommon  Common Class Definitions
 *
 *  \section Module Description
 *  Constants, Types and Enum definitions that are common to both Device and Host modes for the USB
 *  Mass Storage Class.
 *
 *  @{
 */

#ifndef _MS_CLASS_COMMON_H_
#define _MS_CLASS_COMMON_H_

	/* Includes: */
		#include "../../USB.h"

		#include <string.h>

	/* Enable C linkage for C++ Compilers: */
		#if defined(__cplusplus)
			extern "C" {
		#endif

	/* Macros: */
		/** Mass Storage Class specific request to reset the Mass Storage interface, ready for the next command. */
		#define REQ_MassStorageReset       0xFF

		/** Mass Storage Class specific request to retrieve the total number of Logical Units (drives) in the SCSI device. */
		#define REQ_GetMaxLUN              0xFE
		
		/** Magic signature for a Command Block Wrapper used in the Mass Storage Bulk-Only transport protocol. */
		#define MS_CBW_SIGNATURE           0x43425355UL

		/** Magic signature for a Command Status Wrapper used in the Mass Storage Bulk-Only transport protocol. */
		#define MS_CSW_SIGNATURE           0x53425355UL
		
		/** Mask for a Command Block Wrapper's flags attribute to specify a command with data sent from host-to-device. */
		#define MS_COMMAND_DIR_DATA_OUT    (0 << 7)

		/** Mask for a Command Block Wrapper's flags attribute to specify a command with data sent from device-to-host. */
		#define MS_COMMAND_DIR_DATA_IN     (1 << 7)

		/** SCSI Command Code for an INQUIRY command. */
		#define SCSI_CMD_INQUIRY                               0x12

		/** SCSI Command Code for a REQUEST SENSE command. */
		#define SCSI_CMD_REQUEST_SENSE                         0x03

		/** SCSI Command Code for a TEST UNIT READY command. */
		#define SCSI_CMD_TEST_UNIT_READY                       0x00

		/** SCSI Command Code for a READ CAPACITY (10) command. */
		#define SCSI_CMD_READ_CAPACITY_10                      0x25

		/** SCSI Command Code for a SEND DIAGNOSTIC command. */
		#define SCSI_CMD_SEND_DIAGNOSTIC                       0x1D

		/** SCSI Command Code for a PREVENT ALLOW MEDIUM REMOVAL command. */
		#define SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL          0x1E

		/** SCSI Command Code for a WRITE (10) command. */
		#define SCSI_CMD_WRITE_10                              0x2A

		/** SCSI Command Code for a READ (10) command. */
		#define SCSI_CMD_READ_10                               0x28

		/** SCSI Command Code for a WRITE (6) command. */
		#define SCSI_CMD_WRITE_6                               0x0A

		/** SCSI Command Code for a READ (6) command. */
		#define SCSI_CMD_READ_6                                0x08

		/** SCSI Command Code for a VERIFY (10) command. */
		#define SCSI_CMD_VERIFY_10                             0x2F

		/** SCSI Command Code for a MODE SENSE (6) command. */
		#define SCSI_CMD_MODE_SENSE_6                          0x1A

		/** SCSI Command Code for a MODE SENSE (10) command. */
		#define SCSI_CMD_MODE_SENSE_10                         0x5A

		/** SCSI Sense Code to indicate no error has ocurred. */
		#define SCSI_SENSE_KEY_GOOD                            0x00

		/** SCSI Sense Code to indicate that the device has recovered from an error. */
		#define SCSI_SENSE_KEY_RECOVERED_ERROR                 0x01

		/** SCSI Sense Code to indicate that the device is not ready for a new command. */
		#define SCSI_SENSE_KEY_NOT_READY                       0x02

		/** SCSI Sense Code to indicate an error whilst accessing the medium. */
		#define SCSI_SENSE_KEY_MEDIUM_ERROR                    0x03

		/** SCSI Sense Code to indicate a hardware has ocurred. */
		#define SCSI_SENSE_KEY_HARDWARE_ERROR                  0x04

		/** SCSI Sense Code to indicate that an illegal request has been issued. */
		#define SCSI_SENSE_KEY_ILLEGAL_REQUEST                 0x05

		/** SCSI Sense Code to indicate that the unit requires attention from the host to indicate
		 *  a reset event, medium removal or other condition.
		 */
		#define SCSI_SENSE_KEY_UNIT_ATTENTION                  0x06

		/** SCSI Sense Code to indicate that a write attempt on a protected block has been made. */
		#define SCSI_SENSE_KEY_DATA_PROTECT                    0x07

		/** SCSI Sense Code to indicate an error while trying to write to a write-once medium. */
		#define SCSI_SENSE_KEY_BLANK_CHECK                     0x08

		/** SCSI Sense Code to indicate a vendor specific error has ocurred. */
		#define SCSI_SENSE_KEY_VENDOR_SPECIFIC                 0x09

		/** SCSI Sense Code to indicate that an EXTENDED COPY command has aborted due to an error. */
		#define SCSI_SENSE_KEY_COPY_ABORTED                    0x0A

		/** SCSI Sense Code to indicate that the device has aborted the issued command. */
		#define SCSI_SENSE_KEY_ABORTED_COMMAND                 0x0B

		/** SCSI Sense Code to indicate an attempt to write past the end of a partition has been made. */
		#define SCSI_SENSE_KEY_VOLUME_OVERFLOW                 0x0D

		/** SCSI Sense Code to indicate that the source data did not match the data read from the medium. */
		#define SCSI_SENSE_KEY_MISCOMPARE                      0x0E

		/** SCSI Additional Sense Code to indicate no additional sense information is available. */
		#define SCSI_ASENSE_NO_ADDITIONAL_INFORMATION          0x00

		/** SCSI Additional Sense Code to indicate that the logical unit (LUN) addressed is not ready. */
		#define SCSI_ASENSE_LOGICAL_UNIT_NOT_READY             0x04

		/** SCSI Additional Sense Code to indicate an invalid field was encountered while processing the issued command. */
		#define SCSI_ASENSE_INVALID_FIELD_IN_CDB               0x24

		/** SCSI Additional Sense Code to indicate that an attemp to write to a protected area was made. */
		#define SCSI_ASENSE_WRITE_PROTECTED                    0x27

		/** SCSI Additional Sense Code to indicate an error whilst formatting the device medium. */
		#define SCSI_ASENSE_FORMAT_ERROR                       0x31

		/** SCSI Additional Sense Code to indicate an invalid command was issued. */
		#define SCSI_ASENSE_INVALID_COMMAND                    0x20

		/** SCSI Additional Sense Code to indicate a write to a block out outside of the medium's range was issued. */
		#define SCSI_ASENSE_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE 0x21

		/** SCSI Additional Sense Code to indicate that no removable medium is inserted into the device. */
		#define SCSI_ASENSE_MEDIUM_NOT_PRESENT                 0x3A

		/** SCSI Additional Sense Qualifier Code to indicate no additional sense qualifier information is available. */
		#define SCSI_ASENSEQ_NO_QUALIFIER                      0x00

		/** SCSI Additional Sense Qualifier Code to indicate that a medium format command failed to complete. */
		#define SCSI_ASENSEQ_FORMAT_COMMAND_FAILED             0x01

		/** SCSI Additional Sense Qualifier Code to indicate that an initializing command must be issued before the issued
		 *  command can be executed.
		 */
		#define SCSI_ASENSEQ_INITIALIZING_COMMAND_REQUIRED     0x02

		/** SCSI Additional Sense Qualifier Code to indicate that an operation is currently in progress. */
		#define SCSI_ASENSEQ_OPERATION_IN_PROGRESS             0x07
		
	/* Type defines: */
		/** Type define for a Command Block Wrapper, used in the Mass Storage Bulk-Only Transport protocol. */
		typedef struct
		{
			uint32_t Signature; /**< Command block signature, must be CBW_SIGNATURE to indicate a valid Command Block */
			uint32_t Tag; /**< Unique command ID value, to associate a command block wrapper with its command status wrapper */
			uint32_t DataTransferLength; /** Length of the optional data portion of the issued command, in bytes */
			uint8_t  Flags; /**< Command block flags, indicating command data direction */
			uint8_t  LUN; /**< Logical Unit number this command is issued to */
			uint8_t  SCSICommandLength; /**< Length of the issued SCSI command within the SCSI command data array */
			uint8_t  SCSICommandData[16]; /**< Issued SCSI command in the Command Block */
		} MS_CommandBlockWrapper_t;
		
		/** Type define for a Command Status Wrapper, used in the Mass Storage Bulk-Only Transport protocol. */
		typedef struct
		{
			uint32_t Signature; /**< Status block signature, must be CSW_SIGNATURE to indicate a valid Command Status */
			uint32_t Tag; /**< Unique command ID value, to associate a command block wrapper with its command status wrapper */
			uint32_t DataTransferResidue; /**< Number of bytes of data not processed in the SCSI command */
			uint8_t  Status; /**< Status code of the issued command - a value from the MassStorage_CommandStatusCodes_t enum */
		} MS_CommandStatusWrapper_t;
		
	/* Enums: */
		/** Enum for the possible command status wrapper return status codes. */
		enum MassStorage_CommandStatusCodes_t
		{
			SCSI_Command_Pass = 0, /**< Command completed with no error */
			SCSI_Command_Fail = 1, /**< Command failed to complete - host may check the exact error via a SCSI REQUEST SENSE command */
			SCSI_Phase_Error  = 2  /**< Command failed due to being invalid in the current phase */
		};
	
	/* Disable C linkage for C++ Compilers: */
		#if defined(__cplusplus)
			}
		#endif
		
#endif

/** @} */
