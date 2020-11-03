#ifndef CIRCULARBUFFER_H
#define CIRCULARBUFFER_H

constexpr auto SHARED_MEMORY_SIZE = 103;

struct CircularBuffer
{
	unsigned char capacity;
	unsigned char tail;
	unsigned char head;
	unsigned char buffer;
};

#endif
