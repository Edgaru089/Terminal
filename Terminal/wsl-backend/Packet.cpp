
#include "Packet.hpp"
#include <arpa/inet.h>

using namespace std;

bool Packet::checkSize(size_t s) {
	return data.size() - readOffset >= s;
}

uint8_t Packet::getUint8() {
	if (checkSize(sizeof(uint8_t))) {
		uint8_t ans = *reinterpret_cast<const uint8_t*>(data.data() + readOffset);
		readOffset++;
		return ans;
	}
}

int32_t Packet::getInt32() {
	if (checkSize(sizeof(int32_t))) {
		int32_t ans = *reinterpret_cast<const int32_t*>(data.data() + readOffset);
		readOffset += sizeof(int32_t);
		return ntohl(ans);
	}
}

uint32_t Packet::getUint32() {
	if (checkSize(sizeof(uint32_t))) {
		uint32_t ans = *reinterpret_cast<const uint32_t*>(data.data() + readOffset);
		readOffset += sizeof(uint32_t);
		return ntohl(ans);
	}
}

void Packet::putChar(char c) {
	data.push_back(c);
}

void Packet::putInt32(int32_t val) {
	val = htonl(val);
	data.insert(data.end(), reinterpret_cast<char*>(&val), reinterpret_cast<char*>(&val) + sizeof(val));
}

void Packet::putUint32(uint32_t val) {
	val = htonl(val);
	data.insert(data.end(), reinterpret_cast<char*>(&val), reinterpret_cast<char*>(&val) + sizeof(val));
}

void Packet::putRawData(const void* data, size_t size) {
	this->data.insert(this->data.end(), reinterpret_cast<const char*>(data), reinterpret_cast<const char*>(data) + size);
}

