VPATH = vterm

CPPFLAGS =
CFLAGS   = -O2 -I/usr/local/include -g
CXXFLAGS = -O2 -Wno-unused-result -std=c++11 -I/usr/local/include -g
DFLAGS  = -s -L/usr/local/lib -g
LDLIBS   = -lsfml-graphics -lsfml-window -lsfml-system -lpthread -lutil -lm -lstdc++

objects       = Main.o Terminal.o Terminal.Draw.o Terminal.Event.o \
				SystemFrontend.o SystemFrontend.Windows.o SystemFrontend.Unix.o
objects_vterm = encoding.o keyboard.o mouse.o parser.o pen.o screen.o state.o unicode.o vterm.o

all: Terminal

Terminal: $(objects) $(objects_vterm)

clean:
	rm -f *.o Terminal vterm/*.o

.PHONY: all clean

