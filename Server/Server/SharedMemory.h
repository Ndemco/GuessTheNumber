#ifndef SHAREDMEMORY_H
#define SHAREDMEMORY_H

#include <Windows.h>
#include <string>
#include <iostream>

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
	// ... overlay the Circular Buffer structure and ...
	CircularBuffer* pCircularBuffer = reinterpret_cast<CircularBuffer*>(pBuffer);

	CircularBuffer* run(HANDLE hSemaphore)
	{

		if (hMemory == INVALID_HANDLE_VALUE || hMemory == NULL)
		{
			printf("An error occurred created the shared memory buffer:%d\n", GetLastError());
			CloseHandle(hSemaphore);
			CloseHandle(hMemory);
		}

		if (!pBuffer)
		{
			std::cerr << "An error occurred mapping the shared memory: " << GetLastError() << '\n';
			CloseHandle(hSemaphore);
			CloseHandle(hMemory);
		}

		// ... set up its structure
		pCircularBuffer->capacity = SHARED_MEMORY_SIZE - 3; //intelli-sense is not so intelligent here
		pCircularBuffer->tail = 0;
		pCircularBuffer->head = 0;

		return pCircularBuffer;
	}

	void CloseSharedMemory(HANDLE hMemory)
	{
		CloseHandle(hMemory);
	}

	unsigned char* getPBuffer()
	{
		return pBuffer;
	}

	HANDLE getHMemory()
	{
		return hMemory;
	}

};

#endif
