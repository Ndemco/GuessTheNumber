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
#include "../../Shared/SharedMemory.h"

constexpr auto BUFFER_LENGTH = 1024;

wchar_t* convertStringToWideString(std::string ordinaryString)
{
	// Compute the length of the original string and 
	// add 1 for the terminating '\0'
	int slength = (int)ordinaryString.length() + 1;

	// Compute the length of string in "wide" characters
	int wideLen = MultiByteToWideChar(CP_ACP, 0, ordinaryString.c_str(), slength, 0, 0);

	// Allocate a buffer to hold the wide characters
	wchar_t* wideBuffer = new wchar_t[wideLen];

	// Do the actual conversion
	MultiByteToWideChar(CP_ACP, 0, ordinaryString.c_str(), slength, wideBuffer, wideLen);

	// That's it.
	return wideBuffer;
}

// Here we go!
int main(int argc, char* argv[])
{
	// Player's unique ID given by ther server
	char uniqueID = 0;

	//Player's name
	char* playerName = argv[1];

	//Convert name to std::string for convertStringToWideString() method
	std::string name(playerName);

	// Time now
	const auto time_now = std::chrono::system_clock::now();

	// Time since epoch as a wide string
	const std::wstring w_time_since_epoch = std::to_wstring(std::chrono::duration_cast<std::chrono::seconds>(time_now.time_since_epoch()).count());

	// Time since epoch as a normal string
	const std::string time_since_epoch = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(time_now.time_since_epoch()).count());

	//For storage in vector playerList -- to be sent to server
	const std::string unique_name = name + time_since_epoch;

	// Convert player name to wide string
	wchar_t* temp = convertStringToWideString(name);

	// We know the mailslot starts with this
	LPCWSTR mailSlotBeginning = L"\\\\.\\mailslot\\";

	// Full name of mailslot
	std::wstring w_string_mailslot_name = std::wstring(mailSlotBeginning) + temp + w_time_since_epoch;

	delete[] temp;

	// Convert mailslot name to LPCWSTR
	LPCWSTR myMailslot = w_string_mailslot_name.c_str();

	//Create the Mailslot
	HANDLE clientMailslot = CreateMailslot(
		myMailslot,		          // The name of our Mailslot
		BUFFER_LENGTH,            // The size of the input buffer
		MAILSLOT_WAIT_FOREVER,    // Wait forever for new mail
		NULL					  // Use the default security attributes
	);

	// This will be set when the Event is set
	volatile bool moribund = false;

	// All pre-event-set tasks for client
	std::thread t1([&argc, &playerName, &uniqueID, &moribund, &clientMailslot, &unique_name]() {
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

		if (clientMailslot == INVALID_HANDLE_VALUE)
		{
			printf("There was error creating the Mailslot: %d\n", GetLastError());
			return 1;
		}

		// We successfully opened the Mailslot. Send some messages!
		sprintf(writeBuffer, unique_name.c_str(), playerName);
		size_t bytesToWrite = strlen(writeBuffer) + 1;
		writeResult = WriteFile(srvMailslot, writeBuffer, bytesToWrite, &bytesWritten, NULL);

		if (!writeResult || (bytesWritten != bytesToWrite))
		{
			printf("An error occured sending message %s: error code %d\n", writeBuffer, GetLastError());
			CloseHandle(srvMailslot);
			return 1;
		}

		CloseHandle(srvMailslot);

		bool readResult = ReadFile(clientMailslot, readBuffer, BUFFER_LENGTH, &bytesRead, NULL);  // No overlapping I/O
		if (!readResult || (bytesRead <= 0))
		{
			printf("An error occured reading the message: %d\n", GetLastError());
			CloseHandle(clientMailslot);
			return 1;
		}

		char* ID = readBuffer;
		uniqueID = ID[0];

		printf("Unique ID is: %d\n", uniqueID);

		// Open the semaphore we will use to control access. It must have been already created.
		HANDLE hSemaphore = OpenSemaphore(SEMAPHORE_ALL_ACCESS, false, TEXT("SharedMemorySemaphore"));
		if (hSemaphore == INVALID_HANDLE_VALUE)
		{
			printf("An error occurred opening the semaphore: %d\n", GetLastError());
			return 1;
		}

		// Open the Shared Memory object. It must already have been created.
		HANDLE hMemory = OpenFileMapping(FILE_MAP_ALL_ACCESS, false, TEXT("ShareMemoryBuffer"));
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
					(&pCircularBuffer->buffer)[pCircularBuffer->head] = uniqueID;
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
	std::thread t2([&moribund, &uniqueID, &clientMailslot]() {

		// Open the event
		HANDLE hEvent = OpenEvent(
			EVENT_ALL_ACCESS |
			EVENT_MODIFY_STATE,
			false,
			TEXT("SampleEvent")
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
			bool readResult = ReadFile(clientMailslot, readBuffer, BUFFER_LENGTH, &bytesRead, NULL);  // No overlapping I/O
			if (!readResult || (bytesRead <= 0))
			{
				printf("An error occured reading the message: %d\n", GetLastError());
				CloseHandle(clientMailslot);
				return 1;
			}

			char did_i_win = readBuffer[0];

			if (did_i_win == uniqueID)
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