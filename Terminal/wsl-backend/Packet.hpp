#pragma once

#include <vector>
#include <deque>

#include <sys/types.h>
#include <stdint.h>

class Packet {
public:

	uint8_t getUint8();
	int32_t getInt32();
	uint32_t getUint32();

public:

	void putChar(char c);
	void putInt32(int32_t val);
	void putUint32(uint32_t val);
	void putRawData(const void* data, size_t size);

public:

	void clear() { readOffset = 0; data.clear(); }

	const void* getData() { return (const void*)data.data(); }
	size_t getDataSize() { return data.size(); }

	int getReadOffset() { return readOffset; }

private:

	bool checkSize(size_t s);

	friend class Connection;

	int readOffset = 0;
	std::vector<char> data;

};
