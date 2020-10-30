#ifndef EVENT_H
#define EVENT_H

#include <Windows.h>
#include<securitybaseapi.h>

class Event
{
private:
	LPCTSTR eventName;
	// Set up security descriptor with a null access control list.
	// This will allow any other process to access this event.
	PSECURITY_DESCRIPTOR pSD;
	SECURITY_ATTRIBUTES sa = { 0 };

public:

	Event(LPCTSTR eventName) : eventName(eventName) {}

	HANDLE run();
};

#endif

