#ifndef MAILSLOT_H
#define MAILSLOT_H

#include<Windows.h>
#include<vector>

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

	std::vector<pair_of_strings> getPlayerList();

	std::size_t getNumPlayers();

	void run(volatile bool& moribund);
};

#endif

