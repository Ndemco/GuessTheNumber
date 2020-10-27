// Standard includes
#include <Windows.h>
#include <stdio.h>
#include <iostream>
#include <utility>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include "../../Shared/SharedMemory.h"

typedef std::pair<std::string, std::string> pair_of_strings;

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

/**********************************************************************
* TODO: Have main() handle the shared memory and (possibly?) the event
***********************************************************************/

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

	// Array of pairs to store player mailslot name and player name (respectively)
	std::vector<pair_of_strings> playerList;

	// This thread does all pre-event-set mailslot work
	std::thread t1([&num, &moribund, &playerList]() {
		// Define our Mailslot
		LPCWSTR szMailslot = L"\\\\.\\mailslot\\Server";

		// Define a buffer for holding messages
		constexpr auto BUFFER_LENGTH = 1024;
		char readBuffer[BUFFER_LENGTH] = { 0 };
		DWORD bytesRead;

		// Create the Mailslot
		HANDLE srvMailslot = CreateMailslot(
			szMailslot,		          // The name of our Mailslot
			BUFFER_LENGTH,            // The size of the input buffer
			MAILSLOT_WAIT_FOREVER,    // Wait forever for new mail
			NULL					  // Use the default security attributes
		);

		// Check for errors creating Mailslot
		if (srvMailslot == INVALID_HANDLE_VALUE)
		{
			printf("There was error creating the Mailslot: %d\n", GetLastError());
			return 1;
		}

		// Will be used to increment player_list and as the player's unique ID byte.
		char index = 0;

		// Wait for a message
		printf("Mailslot open and waiting for registrations:\n");
		while (!moribund)
		{
			bool readResult = ReadFile(srvMailslot, readBuffer, BUFFER_LENGTH, &bytesRead, NULL);  // No overlapping I/O
			if (!readResult || (bytesRead <= 0))
			{
				printf("An error occured reading the message: %d\n", GetLastError());
				CloseHandle(srvMailslot);
				return 1;
			}

			// Convert buffer to std::string
			std::string playerInfo(readBuffer);

			// Find the space to separate the mailslot name and client name
			std::size_t tok = playerInfo.find(' ');

			// Grab the mailslot name
			std::string uniqueName = playerInfo.substr(0, tok);

			// Grab the client name
			std::string playerName = playerInfo.substr(tok + 1);
			std::cout << uniqueName << " has registered." << '\n';

			// Define client Mailslot
			LPCWSTR clientMailslotBeginning = L"\\\\.\\mailslot\\";

			std::string mailslot_beginning("\\\\.\\mailslot\\");

			// Convert unique name to string for storage in vector
			std::string s_unique_name(uniqueName);

			// Full mailslot name as a string
			std::string player_mailslot_name = mailslot_beginning + s_unique_name;

			// Push full mailslot name and player name to vector of pairs
			playerList.push_back({ player_mailslot_name, playerName });

			// Conver player mailslot name to wide c-string
			wchar_t* temp = convertStringToWideString(player_mailslot_name);

			// Convert player mailslot name to wide string
			std::wstring w_player_mailslot_name(temp);

			// Convert player mailslot name to LPCWSTR
			LPCWSTR clientMailslotName = w_player_mailslot_name.c_str();

			// Define a buffer for writing messages
			char writeBuffer[BUFFER_LENGTH] = { 0 };
			DWORD bytesWritten;
			bool writeResult;

			// Open the Mailslot
			HANDLE clientMailslot = CreateFile(
				clientMailslotName,			// This is the name of our Mailslot
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
			delete[] temp;

			//Index in vector of player's info will be their unique ID.
			sprintf_s(writeBuffer, &index);
			size_t bytesToWrite = 1;
			writeResult = WriteFile(clientMailslot, writeBuffer, bytesToWrite, &bytesWritten, NULL);

			if (!writeResult || (bytesWritten != bytesToWrite))
			{
				printf("An error occured sending message %s: error code %d\n", writeBuffer, GetLastError());
				CloseHandle(clientMailslot);
			}
			else
				CloseHandle(clientMailslot);

			index++;
		}

		CloseHandle(srvMailslot);
		return 0;
		});

	// This thread handles all the shared-memory tasks
	std::thread t2([&moribund, &num, &winnerID]() {
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

						// Signal the event to be set
						moribund = true;

						// Set winner ID to be sent to all clients
						winnerID = playerID;
						return 0;
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
		return 0;
		});

	//This thread handles creating the event, setting the event, and all post-event-set tasks
	std::thread t3([&moribund, &playerList, &winnerID]() {
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
			TEXT("SampleEvent")
		);
		if (hEvent == INVALID_HANDLE_VALUE)
		{
			printf("Creating the event failed!: %d\n", GetLastError());
			return 1;
		}

		/* ********************************************************************************************
		* TODO: This is a very tight loop. It will check the moribund variable many thousands of times
		* per second. Use a condition variable to allow the thread to sleep until needed
		**********************************************************************************************/
		bool eventSet = false;
		while (!eventSet)
		{
			// A player guessed the number
			if (moribund == true)
			{
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
					for (std::size_t i = 0; i < playerList.size(); i++)
					{
						std::string mailslotName = playerList[i].first;
						std::wstring temp(convertStringToWideString(mailslotName));
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
					eventSet = true;
				}
			}
		}
		return 0;
	});

	t1.join();
	t2.join();
	t3.join();

	return 0;
}
