
CPPFLAGS =
CXXFLAGS = -g -O2 -Wno-unused-result -std=c++11
LDFLAGS  = -pthread -s -Wl,-O2
LDLIBS   = -lutil -lm -lstdc++

objects = WslBackend.o Connection.o Packet.o

all: wsl-backend

wsl-backend: $(objects)
	$(CC) -o wsl-backend $(objects) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f *.o wsl-backend

.PHONY: all clean

