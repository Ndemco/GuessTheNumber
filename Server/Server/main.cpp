// Standard includes
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <iostream>
#include <thread>
#include <cstdlib>
#include <ctime>

#include "./include/Event.h"
#include "./include/Mailslot.h"
#include "./include/SharedMemory.h"
#include "./include/CircularBuffer.h"
#include "./include/ServerUtils.h"

int main()
{
	// This variable will be set when the Event is set
	volatile bool moribund = false;

	// Seed the time
	std::srand(static_cast<unsigned>(std::time(0)));
	// mod 256 ensures type safety
	unsigned char num = std::rand() % 256;
	std::cout << "Winning number is " << static_cast<unsigned>(num) << '\n';

	Mailslot mailslot;
	// This thread does all pre-event-set mailslot work
	std::thread mailslotThread([&moribund, &mailslot]() {
		mailslot.run(moribund);
	});

	// Create the event
	Event event(L"ChickenDinner");
	HANDLE hEvent = event.run();

	// Create the semaphore we will use to control access to the circular buffer.
	HANDLE hSemaphore = CreateSemaphore(
		NULL,							// Use the default security attributes
		1,								// Initial 'count'
		1,								// Maximum 'count'
		TEXT("SharedMemorySemaphore")	// The name of the semaphore
	);

	if (hSemaphore == INVALID_HANDLE_VALUE)
	{
		std::cerr << "An error occurred creating the semaphore: " << GetLastError() << '\n';
	}

	SharedMemory sharedMemory;

	CircularBuffer* pCircularBuffer = sharedMemory.run(hSemaphore);

	readGuesses(num, moribund, hSemaphore, pCircularBuffer, &mailslot, hEvent);

	// We're done. Shut everything down
	CloseHandle(hSemaphore);
	UnmapViewOfFile(sharedMemory.getPBuffer());
	CloseHandle(sharedMemory.getHMemory());

	mailslotThread.join();

	return 0;
}
