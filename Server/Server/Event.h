#ifndef EVENT_H
#define EVENT_H

#include <Windows.h>
#include <iostream>
#include <string>
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

	HANDLE run()
	{
		// Create our event. We create it as a manual reset event.
		// An auto reset event is unsignalled after the first process
		// waiting for it processes it.
		HANDLE hEvent = CreateEvent(
			&sa,	// Use our security attributes structure
			true,	// Manual-reset mode
			false,	// Initially unsignalled
			LPCTSTR(eventName)
		);

		pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
		InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION);
		SetSecurityDescriptorDacl(pSD, // Our security descriptor
			TRUE,                      // Is the ACL present?
			NULL,                      // The ACL, null in this case
			FALSE);                    // The ACL is set by hand.


		// Now set up a security attributes structure and ...
		sa.nLength = sizeof(sa);

		// ... set it up to use security descriptor.
		sa.lpSecurityDescriptor = pSD;
		sa.bInheritHandle = FALSE;

		if (hEvent == INVALID_HANDLE_VALUE)
		{
			printf("Creating the event failed!: %d\n", GetLastError());
		}
		return hEvent;
	}

};

#endif

