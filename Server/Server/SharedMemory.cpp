#include <iostream>
#include "./includes/SharedMemory.h"

CircularBuffer* SharedMemory::run(HANDLE hSemaphore)
{
	if (hSemaphore == INVALID_HANDLE_VALUE)
	{
		std::cerr << "An error occurred creating the semaphore: " << GetLastError() << '\n';
	}

	if (hMemory == INVALID_HANDLE_VALUE || hMemory == NULL)
	{
		std::cerr << "An error occurred created the shared memory buffer: " << GetLastError() << '\n';
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

void SharedMemory::CloseSharedMemory(HANDLE hMemory)
{
	CloseHandle(hMemory);
}

unsigned char* SharedMemory::getPBuffer()
{
	return pBuffer;
}

HANDLE SharedMemory::getHMemory()
{
	return hMemory;
}