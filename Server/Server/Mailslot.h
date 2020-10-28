#ifndef MAILSLOT_H
#define MAILSLOT_H

#define _CRT_SECURE_NO_WARNINGS
#include<Windows.h>
#include<iostream>
#include<vector>

#include "../../Shared/Utils.h"

typedef std::pair<std::string, std::string> pair_of_strings;

class Mailslot
{
private:
	//Define our Mailslot
	const LPCWSTR mailslotName = L"\\\\.\\mailslot\\Server";

	// Define a buffer for holding messages
	static const unsigned BUFFER_LENGTH = 1024;
	char readBuffer[BUFFER_LENGTH] = { 0 };
	DWORD bytesRead;

	std::vector<pair_of_strings> playerList{};

	//Create our Mailslot
	HANDLE srvMailslot = CreateMailslot(
		mailslotName,		         // The name of our Mailslot
		BUFFER_LENGTH,            // The size of the input buffer
		MAILSLOT_WAIT_FOREVER,    // Wait forever for new mail
		NULL					  // Use the default security attributes
	);




public:

	std::vector<pair_of_strings> getPlayerList()
	{
		return playerList;
	}

	std::size_t numPlayers()
	{
		return playerList.size();
	}

	int run(volatile bool& moribund)
	{
		// Check for errors creating Mailslot
		if (srvMailslot == INVALID_HANDLE_VALUE)
		{
			std::cerr << "There was error creating the Mailslot: " << GetLastError() << '\n';
			return 1;
		}

		// Will be used to increment player_list and as the player's unique ID byte.
		char index = 0;

		// Wait for a message
		std::cout << "Mailslot open and waiting for registrations:\n";


		while (!moribund)
		{
			bool readResult = ReadFile(srvMailslot, readBuffer, BUFFER_LENGTH, &bytesRead, NULL);  // No overlapping I/O
			if (!readResult || (bytesRead <= 0))
			{
				std::cerr << "An error occured reading the message: " << GetLastError() << '\n';
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

			// Convert player mailslot name to LPCWSTR
			LPCWSTR clientMailslotName = convertStringToWideString(player_mailslot_name);

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
			
			delete[] clientMailslotName;

			//Index in vector of player's info will be their unique ID.
			sprintf_s(writeBuffer, &index);
			size_t bytesToWrite = 1;
			writeResult = WriteFile(clientMailslot, writeBuffer, bytesToWrite, &bytesWritten, NULL);

			if (!writeResult || (bytesWritten != bytesToWrite))
			{
				printf("An error occured sending message %s: error code %d\n", writeBuffer, GetLastError());
				CloseHandle(clientMailslot);
				return 1;
			}
			else
				CloseHandle(clientMailslot);

			index++;
		}

		CloseHandle(srvMailslot);
		return 0;
	}
};

#endif

