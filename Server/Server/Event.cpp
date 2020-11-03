#include <iostream>
#include <string>
#include "./include/Event.h"

HANDLE Event::run()
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
		std::cerr << "Creating the event failed!: " << GetLastError() << '\n';
	}
	return hEvent;
}