////////////////////////////////////////////////////////////////////////////////
//
//  File           : cart_driver.c
//  Description    : This is the implementation of the standardized IO functions
//                   for used to access the CRUD storage system.
//
//  Author         : John Flanigan
//  Last Modified  : Oct 24 2016
//

// Includes
#include <stdlib.h>
#include <string.h>

// Project Includes
#include <cart_driver.h>
#include <cart_controller.h>
#include <cmpsc311_log.h>

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
	struct frame listOfFrames[CART_CARTRIDGE_SIZE];	// Sorted list of frames that make up file
							// Sorted such that frames are consecutive
};

struct file files[CART_MAX_TOTAL_FILES];
int numberOfFiles;

//TODO Implement frame allocation to support multiple files
//int firstFreeCart;
//int firstFreeFrame;
//
//int allocateFrame(int fd) {
//	int lastListIndex = files[fd].endPosition / 1024;
//	lastListIndex++;
//	files[fd].listOfFrames[lastListIndex].cartIndex = firstFreeCart;
//	files[fd].listOfFrames[lastListIndex].cartIndex = firstFreeFrame;
//	if (firstFreeFrame >= CART_CARTRIDGE_SIZE) {
//		firstFreeCart += 1;
//		firstFreeFrame = 0;
//	} else {
//		firstFreeFrame++;	
//	}
//	return (0);
//}

// Implementation

////////////////////////////////////////////////////////////////////////////////
//
// Function     : create_cart_opcode
// Description  : Packs a 64 bit cart register and returns the packed register
//
// Inputs       : ky1 - indicates the cart opcode (8 bits)
//                ky2 - unused currently (8 bits)
//                rt1 - single bit to indicate return success or failure (1 bit)
//                ct1 - cart index
//                fm1 - frame index
// Outputs      : regstate - a packed register

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

////////////////////////////////////////////////////////////////////////////////
//
// Function     : extract_cart_opcode
// Description  : Unpacks a packed register and puts each value into its own index
//                in an array
//
// Inputs       : regstate - a packed register containing operating arguments
//                oregstate - an array of CartXferRegister allocated for 5 elements
// Outputs      : 0 if successful

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
// Function     : loadCommand
// Description  : Loads a cartridge using the cart_io_bus 
//
// Inputs       : cartIndex - the index of the cart to be loaded
// Outputs      : 0 if successful, -1 if failure

int loadCommand(CartridgeIndex cartIndex) {
	CartXferRegister regstate = 0x0, ky1, ky2, rt1, fm1;
	CartXferRegister oregstate[5];
	
	ky1 = CART_OP_LDCART;
	ky2 = 0;
	rt1 = 0;
	fm1 = 0;
	regstate = create_cart_opcode(ky1, ky2, rt1, cartIndex, fm1);
	cart_io_bus(regstate, NULL);

	extract_cart_opcode(regstate, oregstate);
	if (oregstate[CART_REG_RT1] == -1) {
		logMessage(LOG_ERROR_LEVEL, "CART driver failed: failed to load cartridge %d.", cartIndex);
		return (-1);
	}
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : readCommand
// Description  : Reads a frame from the currently loaded cartridge
//                using the cart_io_bus 
//
// Inputs       : frameIndex - the index of the frame to be read
//                tempBuf - a character pointer allocated for the size of one frame.
//                          contents of frame will be written to address
// Outputs      : 0 if successful, -1 if failure

int readCommand(CartFrameIndex frameIndex, char *tempBuf) {
	CartXferRegister regstate = 0x0, ky1, ky2, rt1, ct1;
	CartXferRegister oregstate[5];
	
	ky1 = CART_OP_RDFRME;
	ky2 = 0;
	rt1 = 0;
	ct1 = 0;
	regstate = create_cart_opcode(ky1, ky2, rt1, ct1, frameIndex);
	cart_io_bus(regstate, tempBuf);

	extract_cart_opcode(regstate, oregstate);
	if (oregstate[CART_REG_RT1] == -1) {
		logMessage(LOG_ERROR_LEVEL, "CART driver failed: failed to read frame %d.", frameIndex);
		return (-1);
	}
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : writeCommand
// Description  : Writes a frame to the currently loaded cartridge
//                using the cart_io_bus 
//
// Inputs       : frameIndex - the index of the frame to be written to
//                tempBuf - a character pointer allocated for the size of one frame.
//                          contains the characters to be written
// Outputs      : 0 if successful, -1 if failure

int writeCommand(CartFrameIndex frameIndex, char *tempBuf) {
	CartXferRegister regstate = 0x0, ky1, ky2, rt1, ct1;
	CartXferRegister oregstate[5];

	ky1 = CART_OP_WRFRME;
	ky2 = 0;
	rt1 = 0;
	ct1 = 0;
	regstate = create_cart_opcode(ky1, ky2, rt1, ct1, frameIndex);
	cart_io_bus(regstate, tempBuf);

	extract_cart_opcode(regstate, oregstate);
	if (oregstate[CART_REG_RT1] == -1) {
		logMessage(LOG_ERROR_LEVEL, "CART driver failed: failed to write frame %d.", frameIndex);
		return (-1);
	}
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : checkFileHandle
// Description  : Determines whether the file handle is invalid or closed
//
// Inputs       : fd - the file handle
// Outputs      : 0 if file handle is valid, -1 if it is invalid

int checkFileHandle(int16_t fd) {
	// Invalid file handle
	if (fd < 0 || fd >= CART_MAX_TOTAL_FILES) {
		logMessage(LOG_ERROR_LEVEL, "CART driver failed: bad file handle.");
		return (-1);
	}
	// If file was already closed
	if (files[fd].openFlag == 0) {
		logMessage(LOG_ERROR_LEVEL, "CART driver failed: file is closed.");
		return (-1);
	}
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
	// Create log
	// initializeLogWithFilename(LOG_SERVICE_NAME);
	// enableLogLevels(DEFAULT_LOG_LEVEL);

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
		logMessage(LOG_ERROR_LEVEL, "CART driver failed: failed to power on.");
		return (-1);
	}

	// Load and zero all cartridges
	for (index = 0x0; index < CART_MAX_CARTRIDGES; index++) {
		// Load cartridge
		if (loadCommand(index) == -1) {
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
		for (int j = 0; j < CART_CARTRIDGE_SIZE; j++) {
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
	CartXferRegister regstate = 0x0, ky1, ky2, rt1, ct1, fm1;
	CartXferRegister oregstate[5];
	// Power off the memory system
	ky1 = CART_OP_POWOFF;
	ky2 = 0x0;
	rt1 = 0x0;
	ct1 = 0x0;
	fm1 = 0x0;
	regstate = create_cart_opcode(ky1, ky2, rt1, ct1, fm1);
	cart_io_bus(regstate, NULL);
	extract_cart_opcode(regstate, oregstate);
	if (oregstate[CART_REG_RT1] == -1) {
		logMessage(LOG_ERROR_LEVEL, "CART driver failed: failed to shut down.");
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
	// Making assumption the max file size is 100k. Therefore, only one cart needed
	// TODO Implement frame allocation to support multiple files
	for (int i = 0; i < CART_CARTRIDGE_SIZE; i++) {
		files[numberOfFiles].listOfFrames[i].cartIndex = 0;
		files[numberOfFiles].listOfFrames[i].frameIndex = i;
	}

	// Returns the file handle. If it reaches this point it is because the file
	// had to be created and therefore its file handle will be the number of files
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
	if (checkFileHandle(fd) == -1) {
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
	if (checkFileHandle(fd) == -1) {
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
	
	int positionInFrame, listIndex, bytesRemaining;
	char tempBuf[CART_FRAME_SIZE];

	bytesRemaining = bytesToRead;

	while (bytesRemaining > 0) {
		positionInFrame = files[fd].currentPosition % CART_FRAME_SIZE;	// Position in current frame
		listIndex = files[fd].currentPosition / CART_FRAME_SIZE;	// Location in frame list

		// Load cartridge of frame
		if (loadCommand(files[fd].listOfFrames[listIndex].cartIndex) == -1) {
			return (-1);
		}
		// Read frame
		if (readCommand(files[fd].listOfFrames[listIndex].frameIndex, tempBuf) == -1) {
			return (-1);
		}

		int bytesFromFrame;

		if (bytesRemaining == bytesToRead) {		// First read needs to be copied into buf
			if (bytesToRead < CART_FRAME_SIZE) {		// Read only requires one frame
				bytesFromFrame = bytesToRead;
			} else {				// Read requires subsequent frames
				bytesFromFrame = CART_FRAME_SIZE - positionInFrame;
			}
			memcpy(buf, &tempBuf[positionInFrame], bytesFromFrame);
		} else if (bytesRemaining < CART_FRAME_SIZE) {		// Last read
			bytesFromFrame = bytesRemaining;
			strncat(buf, tempBuf, bytesFromFrame);
		} else {					// Entire frame copied
			bytesFromFrame = CART_FRAME_SIZE;
			strncat(buf, tempBuf, bytesFromFrame);
		}

		bytesRemaining -= bytesFromFrame;
		files[fd].currentPosition += bytesFromFrame;
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
	if (checkFileHandle(fd) == -1) {
		return (-1);
	}
	
	char tempBuf[CART_FRAME_SIZE];
	int bytesRemaining, bytesToWrite, positionInFrame, listIndex, locationInBuf;
	bytesRemaining = count;
	locationInBuf = 0;

	while (bytesRemaining > 0) {
		positionInFrame = files[fd].currentPosition % CART_FRAME_SIZE;	// Position in current frame
		listIndex = files[fd].currentPosition / CART_FRAME_SIZE; 	// Location in frame list

		if (bytesRemaining + positionInFrame >= CART_FRAME_SIZE) {
			bytesToWrite = CART_FRAME_SIZE - positionInFrame;
		} else if ((bytesRemaining + positionInFrame) % CART_FRAME_SIZE != 0) {
			bytesToWrite = bytesRemaining % CART_FRAME_SIZE;
		} else {
			bytesToWrite = CART_FRAME_SIZE;
		}

		// Load cartridge
		if (loadCommand(files[fd].listOfFrames[listIndex].cartIndex) == -1) {
			return (-1);
		}
		// Read frame
		if (readCommand(files[fd].listOfFrames[listIndex].frameIndex, tempBuf) == -1) {
			return (-1);
		}
		// Update tempBuf before writing
		strncpy(&tempBuf[positionInFrame], buf+locationInBuf, bytesToWrite);
		// Write frame
		if (writeCommand(files[fd].listOfFrames[listIndex].frameIndex, tempBuf) == -1) {
			return (-1);
		}
		
		bytesRemaining -= bytesToWrite;
		files[fd].currentPosition += bytesToWrite;
		locationInBuf += bytesToWrite;
		if (files[fd].endPosition < files[fd].currentPosition) {
			files[fd].endPosition = files[fd].currentPosition;
		}
	}

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
	if (checkFileHandle(fd) == -1) {
		return (-1);
	}
	if (loc > files[fd].endPosition) {
		logMessage(LOG_ERROR_LEVEL, "CART driver failed: offset exceeds file length.");
		return (-1);
	}
	files[fd].currentPosition = loc;

	// Return successfully
	return (0);
}
