
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

	switch (e.type) {
	case Event::TextEntered:
	{
		VTermModifier mod = getModifier();
		if (e.text.unicode <= 31 && mod & VTERM_MOD_CTRL) {
			// Control characters produced by <CTRL-x> keystrokes
			switch (e.text.unicode) {
				// These keys have speical meaning:
			case 0x08: //  BS (Ctrl-H) Backspace
			case 0x09: //  HT (Ctrl-I) Tab
			case 0x0A: //  LF (Ctrl-J) Line feed
			case 0x0D: //  CR (Ctrl-M) Return
			case 0x1B: // ESC (Ctrl-[) Escape
				// Let's cancel out the Ctrl modifier
				mod = VTermModifier(mod & (~VTERM_MOD_CTRL));
				break;
			default:
				e.text.unicode += 'A' - 1;
				if (!(mod & VTERM_MOD_SHIFT) && isalpha((int)e.text.unicode))
					e.text.unicode += 'a' - 'A';
			}
		}

		if (e.text.unicode == 127) {
			// Under X11, the Delete key would trigger a 0x7f text event
			// Let's ignore it
			break;
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
			break;
		}

		fprintf(stderr, "Main: Event::TextEntered, keycode=%d(%c), Ctrl:%s, Shift:%s, Alt:%s\n", (int)e.text.unicode, (char)e.text.unicode,
			(mod & VTERM_MOD_CTRL) ? "Yes" : "No", (mod & VTERM_MOD_SHIFT) ? "Yes" : "No", (mod & VTERM_MOD_ALT) ? "Yes" : "No");
		vterm_keyboard_unichar(term, e.text.unicode, mod);

		// Let's also reset the scrollback position
		if (!altScreen) {
			scrollbackOffset = 0;
			invalidate();
		}

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
		if (mouseState != VTERM_PROP_MOUSE_NONE) {
			int b = 0;
			if (e.mouseButton.button == Mouse::Left)
				b = 1; // Left
			else if (e.mouseButton.button == Mouse::Middle)
				b = 2; // Middle
			else if (e.mouseButton.button == Mouse::Right)
				b = 3; // Right;
			vterm_mouse_button(term, b, e.type == Event::MouseButtonPressed, getModifier());
		}
		break;
	}
	case Event::MouseWheelMoved:
	{
		if (mouseState != VTERM_PROP_MOUSE_NONE) {
			vterm_mouse_button(term, ((e.mouseWheel.delta > 0) ? 4 : 5), true, getModifier());
			vterm_mouse_button(term, ((e.mouseWheel.delta > 0) ? 4 : 5), false, getModifier());
		} else if (altScreen) {
			// Let's press the UP/DOWN key 3 times
			VTermModifier mod = getModifier();
			if (e.mouseWheel.delta > 0) {
				vterm_keyboard_key(term, VTERM_KEY_UP, mod);
				vterm_keyboard_key(term, VTERM_KEY_UP, mod);
				vterm_keyboard_key(term, VTERM_KEY_UP, mod);
			} else {
				vterm_keyboard_key(term, VTERM_KEY_DOWN, mod);
				vterm_keyboard_key(term, VTERM_KEY_DOWN, mod);
				vterm_keyboard_key(term, VTERM_KEY_DOWN, mod);
			}
		} else {
			// Let's scroll back/forth
			if (scrollbackOffset < 0)
				scrollbackOffset = -scrollbackOffset;
			if (e.mouseWheel.delta > 0) {
				scrollbackOffset = min(scrollbackOffset + 3, (int)scrollback.size());
			} else {
				scrollbackOffset = max(scrollbackOffset - 3, 0);
			}
			invalidate();
		}
		break;
	}
	case Event::KeyPressed:
	{
		VTermModifier mod = getModifier();
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
		case Keyboard::Delete:
			key = VTERM_KEY_DEL; break;
#ifdef SFML_SYSTEM_LINUX
		case Keyboard::Space:
			// Ctrl-Space does not fire a NUL text input under X, we have to add it
			if (mod & VTERM_MOD_CTRL)
				vterm_keyboard_unichar(term, ' ', mod);
			break;
#endif
		}
		if (e.key.code >= Keyboard::F1 && e.key.code <= Keyboard::F15)
			key = (VTermKey)(VTERM_KEY_FUNCTION((int)e.key.code - Keyboard::F1 + 1));
		if (key != VTERM_KEY_NONE)
			vterm_keyboard_key(term, key, mod);

		// Let's also reset the scrollback position
		if (!altScreen) {
			scrollbackOffset = 0;
			invalidate();
		}

		break;
	}
	case Event::Resized:
	{
		if (e.size.width / cellSize.x != cols || e.size.height / cellSize.y != rows) {
			cols = e.size.width / cellSize.x;
			rows = e.size.height / cellSize.y;
			vterm_set_size(term, rows, cols);

			frontend->resizeTerminal(rows, cols);

			// Note that the scrollback buffer might have resized
			scrollbackOffset = min(scrollbackOffset, (int)scrollback.size());

			invalidate();
		}
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


void Terminal::update() {

	char buffer[4096];
	size_t readlen;

	while (readlen = frontend->tryRead(buffer, sizeof(buffer))) {
		vterm_input_write(term, buffer, readlen);
	}

	if (!frontend->isRunning()) {
		fprintf(stderr, "Terminal::update(): frontend session ended\n");
		running = false;
	}
}

