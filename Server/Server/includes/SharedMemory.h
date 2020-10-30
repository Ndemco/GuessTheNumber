#ifndef SHAREDMEMORY_H
#define SHAREDMEMORY_H

#include <Windows.h>
#include "../../Shared/CircularBuffer.h"

class SharedMemory
{
private:
	// Create the Shared Memory object
	HANDLE hMemory = CreateFileMapping(
		INVALID_HANDLE_VALUE,			// We will use the page file for backing
		NULL,							// Use the default security attributes
		PAGE_READWRITE,					// We need both read and write access
		0,								// Maximum Object size, use default
		SHARED_MEMORY_SIZE,				// Size of the shared memory
		TEXT("SharedMemoryBuffer")							// Name of our shared memory buffer
	);

	// Create the memory mapping, ...
	unsigned char* pBuffer = static_cast<unsigned char*>(MapViewOfFile(hMemory, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEMORY_SIZE));
	


public:

	// Create the semaphore we will use to control access to the circular buffer.
	HANDLE hSemaphore = CreateSemaphore(
		NULL,							// Use the default security attributes
		1,								// Initial 'count'
		1,								// Maximum 'count'
		TEXT("SharedMemorySemaphore")	// The name of the semaphore
	);

	// ... overlay the Circular Buffer structure and ...
	CircularBuffer* pCircularBuffer = reinterpret_cast<CircularBuffer*>(pBuffer);

	CircularBuffer* run(HANDLE hSemaphore);

	void CloseSharedMemory(HANDLE hMemory);

	unsigned char* getPBuffer();

	HANDLE getHMemory();
};

#endif
