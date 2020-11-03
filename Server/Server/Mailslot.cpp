#include <iostream>
#include "./include/Mailslot.h"
#include "./include/Utils.h"

std::vector<pair_of_strings> Mailslot::getPlayerList()
{
	return playerList;
}

std::size_t Mailslot::getNumPlayers()
{
	return playerList.size();
}

void Mailslot::run(volatile bool& moribund)
{
	// Check for errors creating Mailslot
	if (srvMailslot == INVALID_HANDLE_VALUE)
	{
		std::cerr << "There was error creating the Mailslot: " << GetLastError() << '\n';
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
			std::cerr << "There was an error opening the Mailslot: " << GetLastError() << '\n';
		}

		// We successfully opened the Mailslot. Send some messages!

		delete[] clientMailslotName;

		//Index in vector of player's info will be their unique ID.
		sprintf_s(writeBuffer, &index);
		size_t bytesToWrite = 1;
		writeResult = WriteFile(clientMailslot, writeBuffer, bytesToWrite, &bytesWritten, NULL);

		if (!writeResult || (bytesWritten != bytesToWrite))
		{
			std::cerr << "An error occured sending message %s: error code " << writeBuffer <<
				GetLastError();
			
			CloseHandle(clientMailslot);
		}
		else
			CloseHandle(clientMailslot);

		index++;
	}

	CloseHandle(srvMailslot);
}