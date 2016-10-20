////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_driver.c
//  Description    : This is the implementation of the standardized IO functions
//                   for used to access the CRUD storage system.
//
//  Author         : John Flanigan
//  Last Modified  : Oct 19 2016
//

// Includes
#include <stdlib.h>
#include <string.h>

// Project Includes
#include <cart_driver.h>
#include <cart_controller.h>
#include <cmpsc311_log.h>

//Defines
#define MAX_FILE_SIZE 100

// Filesystem
struct frame {
	int cartIndex;
	int frameIndex;
};

struct file {
	int openFlag;					// Zero if file is closed, one if open
	char filePath[CART_MAX_PATH_LENGTH];		// File path string
	int endPosition;				// First empty index in final frame (in bytes)
	int currentPosition;				// Current position (in bytes)
	struct frame listOfFrames[MAX_FILE_SIZE];	// Sorted list of frames that make up file
							// Sorted such that file is contiguous
};

struct file files[CART_MAX_TOTAL_FILES];
int numberOfFiles;

// Implementation

CartXferRegister create_cart_opcode(CartXferRegister ky1, CartXferRegister ky2, CartXferRegister rt1, CartXferRegister ct1, CartXferRegister fm1) {
	CartXferRegister regstate = 0x0, tempKY1, tempKY2, tempRT1, tempCT1, tempFM1, unused;
	tempKY1 = (ky1&0xff) << 56;
	tempKY2 = (ky2&0xff) << 48;
	tempRT1 = (rt1&0x1) << 47;
	tempCT1 = (ct1&0xffff) << 31;
	tempFM1 = (fm1&0xffff) << 15;
	unused = 0x0;
	regstate = tempKY1|tempKY2|tempRT1|tempCT1|tempFM1|unused;
	return (regstate);
}

int extract_cart_opcode(CartXferRegister regstate, CartXferRegister* oregstate) {		
	oregstate[CART_REG_KY1] = (regstate&0xff00000000000000) >> 56;
	oregstate[CART_REG_KY2] = (regstate&0x00ff000000000000) >> 48;
	oregstate[CART_REG_RT1] = (regstate&0x0000800000000000) >> 47;
	oregstate[CART_REG_CT1] = (regstate&0x00007FFF80000000) >> 31;
	oregstate[CART_REG_FM1] = (regstate&0x000000007FFF8000) >> 15;
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_poweron
// Description  : Startup up the CART interface, initialize filesystem
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t cart_poweron(void) {
	// TODO: Update error checking. Use example in assignment slides
	// Create log
	initializeLogWithFilename(LOG_SERVICE_NAME);

	CartXferRegister regstate = 0x0, ky1, ky2, rt1, ct1, fm1, index;
	CartXferRegister oregstate[5];
	// Initialize memory system
	ky1 = CART_OP_INITMS;
	ky2 = 0x0;
	rt1 = 0x0;
	ct1 = 0x0;
	fm1 = 0x0;
	regstate = create_cart_opcode(ky1, ky2, rt1, ct1, fm1);
	cart_io_bus(regstate, NULL);
	// Check return value
	extract_cart_opcode(regstate, oregstate);
	if (oregstate[CART_REG_RT1] == -1) {
		return (-1);
	}

	// Load and zero all cartridges
	for (index = 0x0; index < CART_MAX_CARTRIDGES; index++) {
		// Load cartridge
		ky1 = CART_OP_LDCART;
		ct1 = index;
		regstate = create_cart_opcode(ky1, ky2, rt1, ct1, fm1);
		cart_io_bus(regstate, NULL);
		
		extract_cart_opcode(regstate, oregstate);
		if (oregstate[CART_REG_RT1] == -1) {
			return (-1);
		}

		// Zero current cartridge
		ky1 = CART_OP_BZERO;
		regstate = create_cart_opcode(ky1, ky2, rt1, ct1, fm1);
		cart_io_bus(regstate, NULL);
		extract_cart_opcode(regstate, oregstate);
		if (oregstate[CART_REG_RT1] == -1) {
			return (-1);
		}
	}

	// Initialize file system
	numberOfFiles = 0;
	for (int i = 0; i < CART_MAX_TOTAL_FILES; i++) {
		files[i].openFlag = 0;
		files[i].filePath[0] = '\0';
		files[i].endPosition = 0;
		files[i].currentPosition = 0;
		for (int j = 0; j < MAX_FILE_SIZE; j++) {
			files[i].listOfFrames[j].cartIndex = 0;
			files[i].listOfFrames[j].frameIndex = 0;
		}		
	}
	// Return successfully
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_poweroff
// Description  : Shut down the CART interface, close all files
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t cart_poweroff(void) {
	// TODO: Update error checking. Use example in assignment slides
	CartXferRegister regstate = 0x0, ky1, ky2, rt1, ct1, fm1;
	CartXferRegister oregstate[5];
	// Power off the memory system
	ky1 = CART_OP_POWOFF;
	ky2 = 0x0;
	rt1 = 0x0;
	ct1 = 0x0;
	fm1 = 0x0;
	regstate = create_cart_opcode(ky1, ky2, rt1, ct1, fm1);
	logMessage(LOG_WARNING_LEVEL, "Powoff register: %016llx", regstate); 
	cart_io_bus(regstate, NULL);
	extract_cart_opcode(regstate, oregstate);
	if (oregstate[CART_REG_RT1] == -1) {
		return (-1);
	}
	// Return successfully
	return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_open
// Description  : This function opens the file and returns a file handle
//
// Inputs       : path - filename of the file to open
// Outputs      : file handle if successful, -1 if failure

int16_t cart_open(char *path) {
	int length = strlen(path) + 1; // Plus 1 so it includes null character

	// Check if file with path name exists
	for (int i = 0; i < numberOfFiles; i++) {
		// If it exists
		if (strncmp(files[i].filePath, path, length) == 0) {
			// Check if file already open. If it is, return -1. Else, open file.
			if (files[i].openFlag == 1) {
				return (-1);
			} else {
				files[i].openFlag = 1;
				files[i].currentPosition = 0;
				return (i); // Return file handle
			}
		}
	}
	
	// Create file
	numberOfFiles++;
	files[numberOfFiles].openFlag = 1;
	strncpy(files[numberOfFiles].filePath, path, length);
	files[numberOfFiles].endPosition = 0;
	files[numberOfFiles].currentPosition = 0;
	for (int i = 0; i < MAX_FILE_SIZE; i++) {
		files[numberOfFiles].listOfFrames[i].cartIndex = 0;
		files[numberOfFiles].listOfFrames[i].frameIndex = 0;
	}

	// THIS SHOULD RETURN A FILE HANDLE
	return (numberOfFiles);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_close
// Description  : This function closes the file
//
// Inputs       : fd - the file descriptor
// Outputs      : 0 if successful, -1 if failure

int16_t cart_close(int16_t fd) {
	// Invalid file handle
	if (fd < 0 || fd >= CART_MAX_TOTAL_FILES) {
		return (-1);
	}
	// If file was already closed
	if (files[fd].openFlag == 0) {
		return (-1);
	}

	// Set flag to closed
	files[fd].openFlag = 0;

	// Return successfully
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_read
// Description  : Reads "count" bytes from the file handle "fh" into the 
//                buffer "buf"
//
// Inputs       : fd - filename of the file to read from
//                buf - pointer to buffer to read into
//                count - number of bytes to read
// Outputs      : bytes read if successful, -1 if failure

int32_t cart_read(int16_t fd, void *buf, int32_t count) {
	// Invalid file handle
	if (fd < 0 || fd >= CART_MAX_TOTAL_FILES) {
		return (-1);
	}
	// Check if file is closed closed
	if (files[fd].openFlag == 0) {
		return (-1);
	}

	// Calculate bytes until end of file
	int bytesLeft, bytesToRead;
	bytesLeft = files[fd].endPosition - files[fd].currentPosition;

	// If not enough bytes are left in file, set bytesToRead to bytesLeft
	if (count > bytesLeft) {
		bytesToRead = bytesLeft;
	} else {
		bytesToRead = count;
	}
	
	// First read bytes remaining in current frame
	int positionInFrame, listIndex;
	CartXferRegister regstate, ky1, ky2, rt1, ct1, fm1;
	char tempBuf[CART_FRAME_SIZE];
	positionInFrame = files[fd].currentPosition % 1024;	// Position in current frame
	listIndex = files[fd].currentPosition / 1024; 		// Location in frame list
	// If read can be done entirely in current frame
	if (positionInFrame + bytesToRead <= 1024) {
		// Load cartridge of frame
		ky1 = CART_OP_LDCART;
		ky2 = 0;
		rt1 = 0;
		ct1 = files[fd].listOfFrames[listIndex].cartIndex;
		fm1 = 0;
		regstate = create_cart_opcode(ky1, ky2, rt1, ct1, fm1);
		cart_io_bus(regstate, NULL);
		// Read frame
		ky1= CART_OP_RDFRME;
		ky2 = 0;
		rt1 = 0;
		ct1 = files[fd].listOfFrames[listIndex].frameIndex;
		fm1 = 0;
		regstate = create_cart_opcode(ky1, ky2, rt1, ct1, fm1);
		cart_io_bus(regstate, tempBuf);
		// Copy frame contents into buffer
		memcpy(buf, &tempBuf[positionInFrame], bytesToRead);
		positionInFrame += bytesToRead;
	} else {
		
	}

	// Return successfully
	return (bytesToRead);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_write
// Description  : Writes "count" bytes to the file handle "fh" from the 
//                buffer  "buf"
//
// Inputs       : fd - filename of the file to write to
//                buf - pointer to buffer to write from
//                count - number of bytes to write
// Outputs      : bytes written if successful, -1 if failure

int32_t cart_write(int16_t fd, void *buf, int32_t count) {

	// Return successfully
	return (count);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : cart_seek
// Description  : Seek to specific point in the file
//
// Inputs       : fd - filename of the file to write to
//                loc - offfset of file in relation to beginning of file
// Outputs      : 0 if successful, -1 if failure

int32_t cart_seek(int16_t fd, uint32_t loc) {

	// Return successfully
	return (0);
}
