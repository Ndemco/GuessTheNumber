#pragma once
constexpr auto SHARED_MEMORY_SIZE = 103;
typedef struct
{
	unsigned char capacity;
	unsigned char tail;
	unsigned char head;
	unsigned char buffer;
} CircularBuffer;