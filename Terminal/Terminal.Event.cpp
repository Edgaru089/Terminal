
#include "Terminal.hpp"
#include "vterm/vterm.h"
#include "vterm/vterm_keycodes.h"

using namespace std;
using namespace sf;


namespace {
	VTermModifier getModifier() {
		int mod = VTERM_MOD_NONE;
		if (Keyboard::isKeyPressed(Keyboard::LShift) || Keyboard::isKeyPressed(Keyboard::RShift))
			mod |= VTERM_MOD_SHIFT;
		if (Keyboard::isKeyPressed(Keyboard::LControl) || Keyboard::isKeyPressed(Keyboard::RControl))
			mod |= VTERM_MOD_CTRL;
		if (Keyboard::isKeyPressed(Keyboard::LAlt) || Keyboard::isKeyPressed(Keyboard::RAlt))
			mod |= VTERM_MOD_ALT;
		return (VTermModifier)mod;
	}

	Vector2i getMouseCell(int x, int y, Vector2i cellSize) {
		return Vector2i(x / cellSize.x, y / cellSize.y);
	}
}


void Terminal::processEvent(RenderWindow& win, Event e) {
	std::lock_guard<recursive_mutex> guard(tlock);

	switch (e.type) {
	case Event::TextEntered:
	{
		VTermModifier mod = getModifier();
		if (e.text.unicode <= 31 && mod & VTERM_MOD_CTRL) {
			// Control characters produced by <CTRL-x> keystrokes
			e.text.unicode += 'A' - 1;
			if (!(mod & VTERM_MOD_SHIFT) && isalpha((int)e.text.unicode))
				e.text.unicode += 'a' - 'A';
		}

		if (e.text.unicode == 'V' && (mod & VTERM_MOD_CTRL)) {
			// Paste
			fprintf(stderr, "Paste\n");
			vterm_keyboard_start_paste(term);
			String s = Clipboard::getString();
			for (Uint32 c : s) {
				if (c == '\r')
					continue;
				vterm_keyboard_unichar(term, c, VTermModifier(0));
			}
			vterm_keyboard_end_paste(term);
			return;
		}

		fprintf(stderr, "Main: Event::TextEntered, keycode=%d(%c), Ctrl:%s, Shift:%s, Alt:%s\n", (int)e.text.unicode, (char)e.text.unicode,
			(mod & VTERM_MOD_CTRL) ? "Yes" : "No", (mod & VTERM_MOD_SHIFT) ? "Yes" : "No", (mod & VTERM_MOD_ALT) ? "Yes" : "No");
		vterm_keyboard_unichar(term, e.text.unicode, getModifier());
		break;
	}
	case Event::MouseMoved:
	{
		if (IntRect(Vector2i(0, 0), Vector2i(win.getSize())).contains(e.mouseMove.x, e.mouseMove.y) && lastMouseCell != getMouseCell(e.mouseMove.x, e.mouseMove.y, cellSize)) {
			lastMouseCell = getMouseCell(e.mouseMove.x, e.mouseMove.y, cellSize);
			vterm_mouse_move(term, lastMouseCell.y, lastMouseCell.x, getModifier());
		}
		break;
	}
	case Event::MouseButtonPressed:
	case Event::MouseButtonReleased:
	{
		int b = 0;
		if (e.mouseButton.button == Mouse::Left)
			b = 1; // Left
		else if (e.mouseButton.button == Mouse::Middle)
			b = 2; // Middle
		else if (e.mouseButton.button == Mouse::Right)
			b = 3; // Right;
		vterm_mouse_button(term, b, e.type == Event::MouseButtonPressed, getModifier());
		break;
	}
	case Event::MouseWheelMoved:
	{
		vterm_mouse_button(term, ((e.mouseWheel.delta > 0) ? 4 : 5), true, getModifier());
		vterm_mouse_button(term, ((e.mouseWheel.delta > 0) ? 4 : 5), false, getModifier());
		break;
	}
	case Event::KeyPressed:
	{
		VTermKey key = VTERM_KEY_NONE;
		switch (e.key.code) {
			//case Keyboard::Enter:
			//	key = VTERM_KEY_ENTER; break;
			//case Keyboard::BackSpace:
			//	key = VTERM_KEY_BACKSPACE; break;
			//case Keyboard::Tab:
			//	key = VTERM_KEY_TAB; break;
		case Keyboard::Escape:
			key = VTERM_KEY_ESCAPE; break;

		case Keyboard::Up:
			key = VTERM_KEY_UP; break;
		case Keyboard::Down:
			key = VTERM_KEY_DOWN; break;
		case Keyboard::Left:
			key = VTERM_KEY_LEFT; break;
		case Keyboard::Right:
			key = VTERM_KEY_RIGHT; break;

		case Keyboard::Insert:
			key = VTERM_KEY_INS; break;
		case Keyboard::Home:
			key = VTERM_KEY_HOME; break;
		case Keyboard::End:
			key = VTERM_KEY_END; break;
		case Keyboard::PageUp:
			key = VTERM_KEY_PAGEUP; break;
		case Keyboard::PageDown:
			key = VTERM_KEY_PAGEDOWN; break;
#ifdef SFML_SYSTEM_WINDOWS
		case Keyboard::Delete:
			key = VTERM_KEY_DEL; break;
#endif
		}
		if (e.key.code >= Keyboard::F1 && e.key.code <= Keyboard::F15)
			key = (VTermKey)(VTERM_KEY_FUNCTION((int)e.key.code - Keyboard::F1 + 1));
		if (key != VTERM_KEY_NONE)
			vterm_keyboard_key(term, key, getModifier());
		break;
	}
	case Event::Resized:
	{
		if (e.size.width / cellSize.x != cols || e.size.height / cellSize.y != rows) {
			cols = e.size.width / cellSize.x;
			rows = e.size.height / cellSize.y;
			vterm_set_size(term, rows, cols);
			//vterm_screen_flush_damage(vterm_obtain_screen(term));
			invalidate();
		}
#ifndef SFML_SYSTEM_WINDOWS
		struct winsize ws;
		ws.ws_row = rows;
		ws.ws_col = cols;
		ioctl(pty, TIOCSWINSZ, &ws);
		//fprintf(stderr, "Sending SIGWINCH(%d) to process %d\n", (int)SIGWINCH, (int)child);
		//kill(-tcgetpgrp(pty), SIGWINCH);
#endif
		break;
	}
#ifndef SFML_SYSTEM_WINDOWS
	case Event::GainedFocus:
	{
		invalidate();
		break;
	}
#endif
	}
}

