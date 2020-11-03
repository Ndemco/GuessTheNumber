#ifndef SERVERUTILS_H
#define SERVERUTILS_H

#include <Windows.h>
#include <iostream>
#include "Event.h"
#include "Mailslot.h"
#include "./include/Utils.h"
#include "./include/CircularBuffer.h"

void readGuesses(unsigned char& num, volatile bool& moribund, HANDLE hSemaphore,
	CircularBuffer* pCircularBuffer, Mailslot* mailslot, HANDLE hEvent)
{
	char playerID = 0;

	unsigned char datum = 0;

	// Now wait until data is placed in the buffer
	std::cout << "All set and waiting for data to arrive..." << '\n';

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

					// Signal the event
					if (!SetEvent(hEvent))
					{
						printf("There was an error signaling the Event: %d\n", GetLastError());
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
						std::size_t numPlayers = mailslot->getNumPlayers();
						for (std::size_t i = 0; i < numPlayers; i++)
						{
							std::string mailslotName = mailslot->getPlayerList()[i].first;

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
							}

							// We successfully opened the Mailslot. Send some messages!

							// Write winnerID to buffer
							sprintf_s(writeBuffer, &playerID);
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
				// This code block is not needed and only here for readability
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
}

#endif
