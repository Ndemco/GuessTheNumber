// Standard includes
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <stdio.h>
#include <iostream>
#include <utility>
#include <string>
#include <thread>
#include <vector>
#include <chrono>

#include "Utils.h"
#include "Mailslot.h"
#include "../../Shared/SharedMemory.h"

int main()
{
	// This variable will be set when the Event is set
	volatile bool moribund = false;

	// ID of the winner.
	char winnerID = 0;

	// Seed the time
	srand(time(NULL));
	// mod 256 ensures type safety
	unsigned char num = rand() % 256;
	printf("Winning number is %d\n", num);

	Mailslot mailslot;
	// This thread does all pre-event-set mailslot work
	std::thread t1([&moribund, &mailslot]() {
		mailslot.run(moribund);
	});

	// Set up security descriptor with a null access control list.
	// This will allow any other process to access this event.
	PSECURITY_DESCRIPTOR pSD
		= (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
	InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(pSD, // Our security descriptor
		TRUE,                      // Is the ACL present?
		NULL,                      // The ACL, null in this case
		FALSE);                    // The ACL is set by hand.

	// Now set up a security attributes structure and ...
	SECURITY_ATTRIBUTES sa = { 0 };
	sa.nLength = sizeof(sa);

	// ... set it up to use security descriptor.
	sa.lpSecurityDescriptor = pSD;
	sa.bInheritHandle = FALSE;

	// Create our event. We create it as a manual reset event.
	// An auto reset event is unsignalled after the first process
	// waiting for it processes it.
	HANDLE hEvent = CreateEvent(
		&sa,	// Use our security attributes structure
		true,	// Manual-reset mode
		false,	// Initially unsignalled
		TEXT("ChickenDinner")
	);
	if (hEvent == INVALID_HANDLE_VALUE)
	{
		printf("Creating the event failed!: %d\n", GetLastError());
		return 1;
	}

	// Create the semaphore we will use to control access to the circular buffer.
	HANDLE hSemaphore = CreateSemaphore(
		NULL,							// Use the default security attributes
		1,								// Initial 'count'
		1,								// Maximum 'count'
		TEXT("SharedMemorySemaphore")	// The name of the semaphore
	);
	if (hSemaphore == INVALID_HANDLE_VALUE)
	{
		printf("An error occurred creating the semaphore: %d\n", GetLastError());
		return 1;
	}

	// Create the Shared Memory object
	HANDLE hMemory = CreateFileMapping(
		INVALID_HANDLE_VALUE,			// We will use the page file for backing
		NULL,							// Use the default security attributes
		PAGE_READWRITE,					// We need both read and write access
		0,								// Maximum Object size, use default
		SHARED_MEMORY_SIZE,				// Size of the shared memory
		TEXT("ShareMemoryBuffer")		// Name of our shared memory buffer
	);
	if (hMemory == INVALID_HANDLE_VALUE || hMemory == NULL)
	{
		printf("An error occurred created the shared memory buffer:%d\n", GetLastError());
		CloseHandle(hSemaphore);
		return 1;
	}

	// Create the memory mapping, ...
	unsigned char* pBuffer = (unsigned char*)MapViewOfFile(hMemory, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEMORY_SIZE);
	if (!pBuffer)
	{
		printf("An error occurred mapping the shared memory: %d\n", GetLastError());
		CloseHandle(hSemaphore);
		CloseHandle(hMemory);
	}

	// ... overlay the Circular Buffer structure and ...
	CircularBuffer* pCircularBuffer = (CircularBuffer*)pBuffer;

	// ... set up its structure
	pCircularBuffer->capacity = SHARED_MEMORY_SIZE - 3;
	pCircularBuffer->tail = 0;
	pCircularBuffer->head = 0;

	char playerID = 0;

	unsigned char datum = 0;

	// Now wait until data is placed in the buffer
	printf("All set and waiting for data to arrive...\n");

	// This is the index of the 'next' to be read from the buffer.
	int next = 0;

	// Loop until the QUIT character is received.
	while (!moribund)
	{
		// First get the semaphore.
		// Wait for up to 100 milliseconds
		DWORD dwWaitResult = WaitForSingleObject(hSemaphore, 100);

		// Check if the semaphore was signalled.
		switch (dwWaitResult)
		{
			// Check if the semaphore was signalled
		case WAIT_OBJECT_0:
			// Check if there is anything of interest in the buffer
			if (pCircularBuffer->tail != pCircularBuffer->head)
			{
				next = (pCircularBuffer->tail + 2) % pCircularBuffer->capacity;
				playerID = (&pCircularBuffer->buffer)[pCircularBuffer->tail];
				datum = (&pCircularBuffer->buffer)[pCircularBuffer->tail + 1];

				printf("Received guess: %d ", datum);
				printf("from ID %d\n", playerID);

				// Check if the guess is correct
				if (datum == num)
				{
					printf("The answer was guessed correctly!\n");

					//Set moribund to false so this doesn't loop again
					moribund = true;
					
					// Set winner ID to be sent to all clients
					winnerID = playerID;

					// Signal the event
					if (!SetEvent(hEvent))
					{
						printf("There was an error signaling the Event: %d\n", GetLastError());
						return 1;
					}
					else
					{
						// Define a buffer for writing messages
						char writeBuffer[1024] = { 0 };
						DWORD bytesWritten;
						bool writeResult;

						// Iterate through vector first element of vector to get
						// mailslot name of all players, then write the winning ID
						// to their mailslot
						std::size_t numPlayers = mailslot.numPlayers();
						for (std::size_t i = 0; i < numPlayers; i++)
						{
							std::string mailslotName = mailslot.getPlayerList()[i].first;
							LPCWSTR w_mailslotName = convertStringToWideString(mailslotName);
							// Open the Mailslot
							HANDLE clientMailslot = CreateFile(
								w_mailslotName,				// This is the name of our Mailslot
								GENERIC_WRITE,				// We need to be able to write to it.
								FILE_SHARE_READ,			// This is required of all Mailslots
								NULL,						// Use the default security attributes
								OPEN_EXISTING,				// Well, duh!
								FILE_ATTRIBUTE_NORMAL,		// Use the normal file attributes
								NULL						// There is no template file
							);

							if (clientMailslot == INVALID_HANDLE_VALUE)
							{
								printf("There was an error opening the Mailslot: %d\n", GetLastError());
								return 1;
							}

							// We successfully opened the Mailslot. Send some messages!

							// Write winnerID to buffer
							sprintf_s(writeBuffer, &winnerID);
							size_t bytesToWrite = strlen(writeBuffer) + 1;
							writeResult = WriteFile(clientMailslot, writeBuffer, bytesToWrite, &bytesWritten, NULL);

							if (!writeResult || (bytesWritten != bytesToWrite))
							{
								printf("An error occured sending message %s: error code %d\n", writeBuffer, GetLastError());
								CloseHandle(clientMailslot);
							}
							else
								CloseHandle(clientMailslot);
						}
					}
				}
				else
				{
					// Update the tail;
					pCircularBuffer->tail = next;
				}
			}
			else
			{
				// Nothing available. Buffer is empty.
			}

			// Release the semaphore so another process may use it.
			if (!ReleaseSemaphore(hSemaphore, 1, NULL))
			{
				printf("Failed to release semaphore!: %d\n", GetLastError());
			}
			break;

			// Check if the semaphore was not signalled. This is not a problem on the sink end.
		case WAIT_TIMEOUT:
			break;
		}
	}

	// We're done. Shut everything down
	CloseHandle(hSemaphore);
	UnmapViewOfFile(pBuffer);
	CloseHandle(hMemory);

	t1.join();

	return 0;
}
