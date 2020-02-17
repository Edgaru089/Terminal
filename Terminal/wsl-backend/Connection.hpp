#pragma once

#include "Packet.hpp"

class Connection {
public:

	bool connect(const char* ipAddress, uint16_t port);

	bool receive(Packet& packet);

	bool send(Packet& packet);

	void disconnect();

private:

	int fd = -1;

};

