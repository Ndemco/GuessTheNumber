#include "Utils.h"

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