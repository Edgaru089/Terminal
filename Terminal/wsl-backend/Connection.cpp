
#include "Connection.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

using namespace std;

namespace {
	// Returns in network byte order
	uint32_t parseIpAddress(const char* ip) {
		int a, b, c, d;
		if (sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d) != 4)
			return 0;
		return htonl(((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | ((uint32_t)d));
	}
}


bool Connection::connect(const char* ip, uint16_t port) {
	if (fd != -1)
		close(fd);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
		return false;

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons((uint16_t)port);
	addr.sin_addr.s_addr = parseIpAddress(ip);
	if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
		return false;

	return true;
}

bool Connection::receive(Packet& pack) {

	pack.clear();

	uint32_t packSize = 0;
	size_t readlen = 0;

	while (readlen < sizeof(packSize)) {
		ssize_t cur = 0;
		if ((cur = ::recv(fd, reinterpret_cast<char*>(&packSize) + readlen, sizeof(packSize) - readlen, MSG_NOSIGNAL)) <= 0)
			return false;
		readlen += cur;
	}

	packSize = ntohl(packSize);

	pack.data.resize(packSize);
	readlen = 0;

	while (readlen < packSize) {
		ssize_t cur = 0;
		if ((cur = ::recv(fd, reinterpret_cast<void*>(pack.data.data() + readlen), packSize - readlen, MSG_NOSIGNAL)) <= 0) {
			pack.clear();
			return false;
		}
		readlen += cur;
	}

	return true;
}


bool Connection::send(Packet& pack) {

	uint32_t packSize = htonl(pack.getDataSize());

	if (::send(fd, reinterpret_cast<const void*>(&packSize), sizeof(packSize), MSG_NOSIGNAL) <= 0)
		return false;

	return ::send(fd, reinterpret_cast<const void*>(pack.getData()), ntohl(packSize), MSG_NOSIGNAL) > 0;
}


void Connection::disconnect() {
	if (fd != -1) {
		close(fd);
		fd = -1;
	}
}
