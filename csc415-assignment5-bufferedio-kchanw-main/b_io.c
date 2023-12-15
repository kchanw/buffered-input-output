/**************************************************************
* Class:  CSC-415-03 Fall 2022
* Name: Kurtis Chan
* Student ID:918139175
* GitHub UserID:kchanw
* Project: Assignment 5 – Buffered I/O
*
* File: b_io.c
*
* Description: Write three functions, b_open(), b_read(), and b_close. Open returns a file descriptor
* much like open() where it gets file info and allocates memory for the buffer. b_read() reads a file 
* in chunks of size 512 bytes using LBAread() for each block. after reading the requested bytes,
* it will copy it into the caller's buffer. b_close() will free any resources used. 
*
**************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "b_io.h"
#include "fsLowSmall.h"

#define MAXFCBS 20	//The maximum number of files open at one time


// This structure is all the information needed to maintain an open file
// It contains a pointer to a fileInfo strucutre and any other information
// that you need to maintain your open file.
typedef struct b_fcb
	{
	fileInfo * fi;	//holds the low level systems file info

	// Add any other needed variables here to track the individual open file
	int fcbIndex; // index in the array of fcbArray
	int filePos; //where we are in the file
	int buffPos; //where we are in the buffer
	char* buffLoc; //buffer for this file
	int total;

	} b_fcb;
	
//static array of file control blocks
b_fcb fcbArray[MAXFCBS];

// Indicates that the file control block array has not been initialized
int startup = 0;	

// Method to initialize our file system / file control blocks
// Anything else that needs one time initialization can go in this routine
void b_init ()
	{
	if (startup)
		return;			//already initialized

	//init fcbArray to all free
	for (int i = 0; i < MAXFCBS; i++)
		{
		fcbArray[i].fi = NULL; //indicates a free fcbArray
		}
		
	startup = 1;
	}

//Method to get a free File Control Block FCB element
b_io_fd b_getFCB ()
	{
	for (int i = 0; i < MAXFCBS; i++)
		{
		if (fcbArray[i].fi == NULL)
			{
			fcbArray[i].fi = (fileInfo *)-2; // used but not assigned
			return i;		//Not thread safe but okay for this project
			}
		}

	return (-1);  //all in use
	}

// b_open is called by the "user application" to open a file.  This routine is 
// similar to the Linux open function.  	
// You will create your own file descriptor which is just an integer index into an
// array of file control blocks (fcbArray) that you maintain for each open file.  

b_io_fd b_open (char * filename, int flags)
	{
	if (startup == 0) b_init();  //Initialize our system
	

	//*** TODO ***:  Write open function to return your file descriptor
	//				 You may want to allocate the buffer here as well
	//				 But make sure every file has its own buffer

	// This is where you are going to want to call GetFileInfo and b_getFCB
	b_io_fd fcbIndex = b_getFCB();
	fcbArray[fcbIndex].fi = GetFileInfo(filename);
	if(fcbIndex == -1){//when the fcb array is full
	   return -1;
	}

	if(fcbArray[fcbIndex].fi == NULL){//when fails to open file
		return -1;
	}

	fcbArray[fcbIndex].buffLoc = malloc(B_CHUNK_SIZE);
	fcbArray[fcbIndex].filePos = 0;
	fcbArray[fcbIndex].buffPos = 0;
	fcbArray[fcbIndex].total = 0;
	printf("Location: %d\nSize: %d\n", fcbArray[fcbIndex].fi->location, fcbArray[fcbIndex].fi->fileSize);
	LBAread(fcbArray[fcbIndex].buffLoc, 1, fcbArray[fcbIndex].fi->location);
	return fcbIndex; //index will be the file descriptor

	

	}

// b_read functions just like its Linux counterpart read.  The user passes in
// the file descriptor (index into fcbArray), a buffer where thay want you to 
// place the data, and a count of how many bytes they want from the file.
// The return value is the number of bytes you have copied into their buffer.
// The return value can never be greater then the requested count, but it can
// be less only when you have run out of bytes to read.  i.e. End of File	
int b_read (b_io_fd fd, char * buffer, int count)
	{
	//*** TODO ***:  
	// Write buffered read function to return the data and # bytes read
	// You must use LBAread and you must buffer the data in B_CHUNK_SIZE byte chunks.
		
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return (-1); 					//invalid file descriptor
		}

	// and check that the specified FCB is actually in use	
	if (fcbArray[fd].fi == NULL)		//File not open for this descriptor
		{
		return -1;
		}	


	// Your Read code here - the only function you call to get data is LBAread.
	// Track which byte in the buffer you are at, and which block in the file	
	int bytesRead = 0;
	int bytesLeft = 0;
	int cnt = 0;
	//printf("reading");


	//printf("\n\n-----%d-----\n\n", count);
	//printf("\n-----%d-----\n",fcbArray[fd].filePos);

	if((fcbArray[fd].filePos + count) >= fcbArray[fd].fi->fileSize){//handling EOF
		cnt = fcbArray[fd].fi->fileSize - fcbArray[fd].filePos;
	}
	else{//cnt will always be count till eof case
		cnt = count;
	}

	fcbArray[fd].total += cnt;
	//printf("\n\nTotalCnt: %d\n\n",fcbArray[fd].total);
	
	//printf("\n\nPassed in:%d\n\nNew Count:%d\n\n",count,cnt);


	if(cnt == 0){//handling when reached end of file
		//printf("\n\nfinished with file %d\n\n", fcbArray[fd].fcbIndex);
		return 0;
	}
	else if((B_CHUNK_SIZE - fcbArray[fd].buffPos) > cnt){ //whenever there is still space in block
		memcpy(buffer, fcbArray[fd].buffLoc + fcbArray[fd].buffPos, cnt);
		fcbArray[fd].buffPos += cnt;
		fcbArray[fd].filePos += cnt;
		bytesRead = cnt;
	}
	else if(((B_CHUNK_SIZE - fcbArray[fd].buffPos) + B_CHUNK_SIZE) < cnt){ //when multiple blocks are needed fill buffer, commit, then use next block

		memcpy(buffer, fcbArray[fd].buffLoc + fcbArray[fd].buffPos, B_CHUNK_SIZE - fcbArray[fd].buffPos);//fills till end of buffer

		fcbArray[fd].fi->location++;
		fcbArray[fd].filePos += B_CHUNK_SIZE - fcbArray[fd].buffPos;
		bytesLeft = cnt - (B_CHUNK_SIZE - fcbArray[fd].buffPos) - B_CHUNK_SIZE;
		bytesRead += B_CHUNK_SIZE - fcbArray[fd].buffPos;
		fcbArray[fd].buffPos = 0;

		LBAread(fcbArray[fd].buffLoc, 1, fcbArray[fd].fi->location);//getting the next block

		memcpy(buffer,fcbArray[fd].buffLoc, B_CHUNK_SIZE);//whole block 
		fcbArray[fd].fi->location++;
		fcbArray[fd].filePos += B_CHUNK_SIZE;
		bytesRead += B_CHUNK_SIZE;
		LBAread(fcbArray[fd].buffLoc, 1, fcbArray[fd].fi->location);//next block 

		memcpy(buffer,fcbArray[fd].buffLoc, bytesLeft);//fill part of the buffer
		fcbArray[fd].filePos += bytesLeft;
		fcbArray[fd].buffPos += bytesLeft;
		bytesRead += bytesLeft; 
	}
	else {//no more space in block when need next block for read
		//printf("\n\nAT THE ELSE STATEMENT\n\n");
		memcpy(buffer, fcbArray[fd].buffLoc + fcbArray[fd].buffPos, B_CHUNK_SIZE - fcbArray[fd].buffPos);
		fcbArray[fd].fi->location++;
		fcbArray[fd].filePos += B_CHUNK_SIZE - fcbArray[fd].buffPos;
		bytesRead += B_CHUNK_SIZE - fcbArray[fd].buffPos;
		bytesLeft = cnt - bytesRead;
		fcbArray[fd].buffPos = 0;
		//printf("%d  %d", bytesRead, count);
		//printf("reading next block\n\n");
		LBAread(fcbArray[fd].buffLoc,1, fcbArray[fd].fi->location);
		//printf("copying %d\n\n", bytesLeft);
		memcpy(buffer + bytesRead, fcbArray[fd].buffLoc, bytesLeft);
		fcbArray[fd].filePos += bytesLeft;
		bytesRead += bytesLeft;
		//printf("done copying\n\n");
	
	}
	//printf("\n\n%d\n\n",cnt);
	return bytesRead;
	}
	
// b_close frees and allocated memory and places the file control block back 
// into the unused pool of file control blocks.
int b_close (b_io_fd fd)
	{
	//*** TODO ***:  Release any resources
	//printf("\n\nClosing file %d\n\n", fd);
	free(fcbArray[fd].buffLoc);
	fcbArray[fd].buffLoc = NULL;
	fcbArray[fd].fi = NULL;
	}
	
