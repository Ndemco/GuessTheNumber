#ifndef PLAYER_H
#define PLAYER_H

#include <Windows.h>
#include <stdio.h>
#include <ctime>
#include <cstdlib>
#include <string>
#include <chrono>

class Player
{
private:
	// Player's unique ID given by ther server
	char unique_id = 0;

	//Player's name
	std::string name;

	const std::string unique_name;

	const std::string mailslot_name;

	const std::string generate_unique_name(std::string name)
	{
		const auto time_now = std::chrono::system_clock::now();

		// Time since epoch as a string
		const std::string time_since_epoch = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
			time_now.time_since_epoch()).count()
		);

		//For storage in vector playerList -- to be sent to server
		const std::string unique_name = name + time_since_epoch;

		return unique_name;
	}

	std::string generate_mailslot_name(std::string unique_name)
	{
		std::string mailslot_prefix("\\\\.\\mailslot\\");

		std::string mailslot_name = mailslot_prefix + unique_name;

		return mailslot_name;
	}

public:

	Player(const std::string name);

	void set_unique_id(char unique_id);

	char get_unique_id();

	void set_name(std::string name);

	std::string get_name();

	const std::string get_unique_name();

	const std::string get_mailslot_name();


};

#endif
