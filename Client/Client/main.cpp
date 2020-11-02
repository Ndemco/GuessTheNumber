// Standard includes
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <stdio.h>
#include <thread>
#include <ctime>
#include <cstdlib>
#include <iostream>
#include <string>
#include <stdlib.h>
#include <chrono>
#include "Player.h"
#include "../../Shared/CircularBuffer.h"
#include "../../Shared/Utils.h"

constexpr auto BUFFER_LENGTH = 1024;

// Here we go!
int main(int argc, char* argv[])
{
	std::string player_name(argv[1]);
	Player player = Player(player_name);

	// Convert mailslot name to LPCWSTR
	LPCWSTR mailslot_name = convertStringToWideString(player.get_mailslot_name());

	//Create the Mailslot
	HANDLE client_mailslot = CreateMailslot(
		mailslot_name,		          // The name of our Mailslot
		BUFFER_LENGTH,            // The size of the input buffer
		MAILSLOT_WAIT_FOREVER,    // Wait forever for new mail
		NULL					  // Use the default security attributes
	);
	
	if (client_mailslot == INVALID_HANDLE_VALUE)
	{
		printf("There was an error creating the Mailslot: %d\n", GetLastError());
		return 1;
	}

	// This will be set when the Event is set
	volatile bool moribund = false;

	// All pre-event-set tasks for client
	std::thread t1([&argc, &player, &moribund, &client_mailslot]() {
		if (argc != 2)
		{
			printf("Invalid number of arguments. Please provide only a name with no spaces.");
			return 1;
		}

		// Define server Mailslot
		LPCWSTR szMailslot = L"\\\\.\\mailslot\\Server";

		// Define a buffer for holding messages
		char readBuffer[BUFFER_LENGTH] = { 0 };
		DWORD bytesRead;

		// Define a buffer writing messages
		char writeBuffer[BUFFER_LENGTH] = { 0 };
		DWORD bytesWritten;
		bool writeResult;

		// Open the Mailslot
		HANDLE srvMailslot = CreateFile(
			szMailslot,					// This is the name of our Mailslot
			GENERIC_WRITE,				// We need to be able to write to it.
			FILE_SHARE_READ,			// This is required of all Mailslots
			NULL,						// Use the default security attributes
			OPEN_EXISTING,				// Well, duh!
			FILE_ATTRIBUTE_NORMAL,		// Use the normal file attributes
			NULL						// There is no template file
		);

		if (srvMailslot == INVALID_HANDLE_VALUE)
		{
			printf("There was an error opening the Mailslot: %d\n", GetLastError());
			return 1;
		}

		// We successfully opened the Mailslot. Send some messages!
		sprintf(writeBuffer, player.get_unique_name().c_str(), player.get_name());
		size_t bytesToWrite = strlen(writeBuffer) + 1;
		writeResult = WriteFile(srvMailslot, writeBuffer, bytesToWrite, &bytesWritten, NULL);

		if (!writeResult || (bytesWritten != bytesToWrite))
		{
			printf("An error occured sending message %s: error code %d\n", writeBuffer, GetLastError());
			CloseHandle(srvMailslot);
			return 1;
		}

		CloseHandle(srvMailslot);

		bool readResult = ReadFile(client_mailslot, readBuffer, BUFFER_LENGTH, &bytesRead, NULL);  // No overlapping I/O
		if (!readResult || (bytesRead <= 0))
		{
			printf("An error occured reading the message: %d\n", GetLastError());
			CloseHandle(client_mailslot);
			return 1;
		}

		char* ID = readBuffer;
		player.set_unique_id(ID[0]);

		printf("Unique ID is: %d\n", player.get_unique_id());

		// Open the semaphore we will use to control access. It must have been already created.
		HANDLE hSemaphore = OpenSemaphore(SEMAPHORE_ALL_ACCESS, false, TEXT("SharedMemorySemaphore"));
		if (hSemaphore == INVALID_HANDLE_VALUE)
		{
			printf("An error occurred opening the semaphore: %d\n", GetLastError());
			return 1;
		}

		// Open the Shared Memory object. It must already have been created.
		HANDLE hMemory = OpenFileMapping(FILE_MAP_ALL_ACCESS, false, TEXT("SharedMemoryBuffer"));
		if (hMemory == INVALID_HANDLE_VALUE || hMemory == NULL)
		{
			printf("An error occurred created the shared memory buffer:%d\n", GetLastError());
			CloseHandle(hSemaphore);
			return 1;
		}

		// Create the memory mapping and ...
		unsigned char* pBuffer = (unsigned char*)MapViewOfFile(hMemory, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEMORY_SIZE);
		if (!pBuffer)
		{
			printf("An error occurred mapping the shared memory: %d\n", GetLastError());
			CloseHandle(hSemaphore);
			CloseHandle(hMemory);
			return 1;
		}

		// ... overlay the circular memory structure.
		CircularBuffer* pCircularBuffer = (CircularBuffer*)pBuffer;

		// This is the index of the 'next' item in the circular buffer to be filled
		// We'll use it to determine when the buffer is full.
		int next = 0;

		// This is the value we'll be sending
		unsigned short datum = 0;

		// Loop until we've sent the QUIT value

		while (!moribund)
		{
			unsigned short guess;
			printf("Guess a number between 0 and 255: ");
			std::cin >> guess;

			//This ensures type safety for when setting unsigned char datum == guess
			while (guess < 0 || guess > 255 || (std::cin.fail() && !(std::cin.bad() || std::cin.eof())))
			{
				if (std::cin.fail())
				{
					std::cin.clear();
					std::cin.ignore(1000, '\n');
				}
				printf("Your guess is not in range, please guess a number between 0 and 255: \n");
				std::cin >> guess;
			}

			datum = (unsigned char)guess;

			// First get the semaphore. Wait for up to 100 milliseconds.
			DWORD dwWaitResult = WaitForSingleObject(hSemaphore, 100);

			// Check if the semaphore was signalled.
			switch (dwWaitResult)
			{
			case WAIT_OBJECT_0:

				// The sempaphore was signalled.
				// Try to put our datum into the buffer. This should succeed unless
				// the buffer is already full.

				// This is where the 'next' items will go. 
				next = (pCircularBuffer->head + 2) % pCircularBuffer->capacity;

				// Check if the buffer is already full.
				if (next == pCircularBuffer->tail)
				{
					// The buffer is already full. Wait a bit and try again.
				}
				// The buffer is not yet full. Store the datum.
				else
				{
					(&pCircularBuffer->buffer)[pCircularBuffer->head] = player.get_unique_id();
					(&pCircularBuffer->buffer)[pCircularBuffer->head + 1] = datum;

					// Update the head of the buffer
					pCircularBuffer->head = next;
				}

				// Release the semaphore so another process may proceed.
				if (!ReleaseSemaphore(hSemaphore, 1, NULL))
				{
					printf("Failed to release semaphore!: %d\n", GetLastError());
				}
				break;

			case WAIT_TIMEOUT:
				// The semaphore timed out. This shouldn't happen since we are 
				// only writing to the buffer. This probably indicates that the
				// receiving end has exited for some reason.
				printf("Timed out waiting for semaphore!: %d\n", GetLastError());
				break;
			}
			//Give the event some time to be signaled before checking moribund again
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		// We're done. Shut everything down
		CloseHandle(hSemaphore);
		UnmapViewOfFile(pBuffer);
		CloseHandle(hMemory);
		return 0;
		});

	// Event checking and all post-event-set tasks of client.
	std::thread t2([&moribund, &player, &client_mailslot]() {

		// Open the event
		HANDLE hEvent = OpenEvent(
			EVENT_ALL_ACCESS |
			EVENT_MODIFY_STATE,
			false,
			TEXT("ChickenDinner")
		);

		if (hEvent == INVALID_HANDLE_VALUE || hEvent == 0)
		{
			printf("There was an error opening the Event: %d\n", GetLastError());
			return 1;
		}

		// Now wait for the event to be set. We'll wait forever
		printf("Wating for event\n");
		DWORD dwWaitResult = WaitForSingleObject(hEvent, INFINITE);
		if (dwWaitResult == WAIT_OBJECT_0)
		{
			// The Event was signaled
			printf("Game has ended\n");
			moribund = true;

			// Define a buffer for holding messages
			char readBuffer[BUFFER_LENGTH] = { 0 };
			DWORD bytesRead;

			// Read buffer (should be the unique ID of the winning player in it)
			bool readResult = ReadFile(client_mailslot, readBuffer, BUFFER_LENGTH, &bytesRead, NULL);  // No overlapping I/O
			if (!readResult || (bytesRead <= 0))
			{
				printf("An error occured reading the message: %d\n", GetLastError());
				CloseHandle(client_mailslot);
				return 1;
			}

			char did_i_win = readBuffer[0];

			if (did_i_win == player.get_unique_id())
			{
				printf("I won!");
			}
			else
			{
				printf("I lost :[");
			}
		}
		else
			printf("An error occurred waiting for the event: %d\n", GetLastError());

		CloseHandle(hEvent);

		return 0;
	});

	t1.join();
	t2.join();

	return 0;
}