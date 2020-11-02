#include "Player.h"

Player::Player(const std::string name) : name(name), unique_id(unique_id),
	unique_name(generate_unique_name(name)),
	mailslot_name(generate_mailslot_name(Player::get_unique_name())){}

void Player::set_unique_id(char unique_id)
{
	this->unique_id = unique_id;
}

char Player::get_unique_id()
{
	return unique_id;
}

void Player::set_name(std::string name)
{
	this->name = name;
}

std::string Player::get_name()
{
	return name;
}

const std::string Player::get_unique_name()
{
	return unique_name;
}

const std::string Player::get_mailslot_name()
{
	return mailslot_name;
}