/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.	If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include <SDL.h>

#include "sys/platform.h"
#include "idlib/containers/List.h"
#include "idlib/Heap.h"
#include "framework/Common.h"
#include "framework/KeyInput.h"
#include "framework/Session.h"
#include "framework/Session_local.h"
#include "renderer/RenderSystem.h"
#include "renderer/tr_local.h"
#include "ui/DeviceContext.h"
#include "ui/UserInterface.h"

#include "sys/sys_public.h"

#if !SDL_VERSION_ATLEAST(2, 0, 0)
#define SDL_Keycode SDLKey
#define SDLK_APPLICATION SDLK_COMPOSE
#define SDLK_SCROLLLOCK SDLK_SCROLLOCK
#define SDLK_LGUI SDLK_LSUPER
#define SDLK_RGUI SDLK_RSUPER
#define SDLK_KP_0 SDLK_KP0
#define SDLK_KP_1 SDLK_KP1
#define SDLK_KP_2 SDLK_KP2
#define SDLK_KP_3 SDLK_KP3
#define SDLK_KP_4 SDLK_KP4
#define SDLK_KP_5 SDLK_KP5
#define SDLK_KP_6 SDLK_KP6
#define SDLK_KP_7 SDLK_KP7
#define SDLK_KP_8 SDLK_KP8
#define SDLK_KP_9 SDLK_KP9
#define SDLK_NUMLOCKCLEAR SDLK_NUMLOCK
#define SDLK_PRINTSCREEN SDLK_PRINT
#endif

#ifdef VITA
#include <vitasdk.h>
#endif

const char *kbdNames[] = {
	"english", "french", "german", "italian", "spanish", "turkish", "norwegian", "brazilian", NULL
};

idCVar in_kbd("in_kbd", "english", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_NOCHEAT, "keyboard layout", kbdNames, idCmdSystem::ArgCompletion_String<kbdNames> );
extern idCVar r_scaleMenusTo43; // DG: for the "scale menus to 4:3" hack

struct kbd_poll_t {
	int key;
	bool state;

	kbd_poll_t() {
	}

	kbd_poll_t(int k, bool s) {
		key = k;
		state = s;
	}
};

struct mouse_poll_t {
	int action;
	int value;

	mouse_poll_t() {
	}

	mouse_poll_t(int a, int v) {
		action = a;
		value = v;
	}
};

struct joystick_poll_t
{
	int axis;
	int value;
	
	joystick_poll_t()
	{
	}
	
	joystick_poll_t( int a, int v )
	{
		axis = a;
		value = v;
	}
};

static idList<joystick_poll_t> joystick_polls;
static idList<kbd_poll_t> kbd_polls;
static idList<mouse_poll_t> mouse_polls;

static byte mapkey(SDL_Keycode key) {
	switch (key) {
	case SDLK_BACKSPACE:
		return K_BACKSPACE;
	case SDLK_PAUSE:
		return K_PAUSE;
	}

	if (key <= SDLK_z)
		return key & 0xff;

	switch (key) {
	case SDLK_APPLICATION:
		return K_COMMAND;
	case SDLK_CAPSLOCK:
		return K_CAPSLOCK;
	case SDLK_SCROLLLOCK:
		return K_SCROLL;
	case SDLK_POWER:
		return K_POWER;

	case SDLK_UP:
		return K_UPARROW;
	case SDLK_DOWN:
		return K_DOWNARROW;
	case SDLK_LEFT:
		return K_LEFTARROW;
	case SDLK_RIGHT:
		return K_RIGHTARROW;

	case SDLK_LGUI:
		return K_LWIN;
	case SDLK_RGUI:
		return K_RWIN;
	case SDLK_MENU:
		return K_MENU;

	case SDLK_LALT:
	case SDLK_RALT:
		return K_ALT;
	case SDLK_RCTRL:
	case SDLK_LCTRL:
		return K_CTRL;
	case SDLK_RSHIFT:
	case SDLK_LSHIFT:
		return K_SHIFT;
	case SDLK_INSERT:
		return K_INS;
	case SDLK_DELETE:
		return K_DEL;
	case SDLK_PAGEDOWN:
		return K_PGDN;
	case SDLK_PAGEUP:
		return K_PGUP;
	case SDLK_HOME:
		return K_HOME;
	case SDLK_END:
		return K_END;

	case SDLK_F1:
		return K_F1;
	case SDLK_F2:
		return K_F2;
	case SDLK_F3:
		return K_F3;
	case SDLK_F4:
		return K_F4;
	case SDLK_F5:
		return K_F5;
	case SDLK_F6:
		return K_F6;
	case SDLK_F7:
		return K_F7;
	case SDLK_F8:
		return K_F8;
	case SDLK_F9:
		return K_F9;
	case SDLK_F10:
		return K_F10;
	case SDLK_F11:
		return K_F11;
	case SDLK_F12:
		return K_F12;
	// K_INVERTED_EXCLAMATION;
	case SDLK_F13:
		return K_F13;
	case SDLK_F14:
		return K_F14;
	case SDLK_F15:
		return K_F15;

	case SDLK_KP_7:
		return K_KP_HOME;
	case SDLK_KP_8:
		return K_KP_UPARROW;
	case SDLK_KP_9:
		return K_KP_PGUP;
	case SDLK_KP_4:
		return K_KP_LEFTARROW;
	case SDLK_KP_5:
		return K_KP_5;
	case SDLK_KP_6:
		return K_KP_RIGHTARROW;
	case SDLK_KP_1:
		return K_KP_END;
	case SDLK_KP_2:
		return K_KP_DOWNARROW;
	case SDLK_KP_3:
		return K_KP_PGDN;
	case SDLK_KP_ENTER:
		return K_KP_ENTER;
	case SDLK_KP_0:
		return K_KP_INS;
	case SDLK_KP_PERIOD:
		return K_KP_DEL;
	case SDLK_KP_DIVIDE:
		return K_KP_SLASH;
	// K_SUPERSCRIPT_TWO;
	case SDLK_KP_MINUS:
		return K_KP_MINUS;
	// K_ACUTE_ACCENT;
	case SDLK_KP_PLUS:
		return K_KP_PLUS;
	case SDLK_NUMLOCKCLEAR:
		return K_KP_NUMLOCK;
	case SDLK_KP_MULTIPLY:
		return K_KP_STAR;
	case SDLK_KP_EQUALS:
		return K_KP_EQUALS;

	// K_MASCULINE_ORDINATOR;
	// K_GRAVE_A;
	// K_AUX1;
	// K_CEDILLA_C;
	// K_GRAVE_E;
	// K_AUX2;
	// K_AUX3;
	// K_AUX4;
	// K_GRAVE_I;
	// K_AUX5;
	// K_AUX6;
	// K_AUX7;
	// K_AUX8;
	// K_TILDE_N;
	// K_GRAVE_O;
	// K_AUX9;
	// K_AUX10;
	// K_AUX11;
	// K_AUX12;
	// K_AUX13;
	// K_AUX14;
	// K_GRAVE_U;
	// K_AUX15;
	// K_AUX16;

	case SDLK_PRINTSCREEN:
		return K_PRINT_SCR;
	case SDLK_MODE:
		return K_RIGHT_ALT;
	}

	return 0;
}

static void PushConsoleEvent(const char *s) {
	char *b;
	size_t len;

	len = strlen(s) + 1;
	b = (char *)Mem_Alloc(len);
	strcpy(b, s);

	SDL_Event event;

	event.type = SDL_USEREVENT;
	event.user.code = SE_CONSOLE;
	event.user.data1 = (void *)len;
	event.user.data2 = b;

	SDL_PushEvent(&event);
}

static inline byte JoyToKey(int button) {
    static const int keymap[] = {
        /* KEY_A      */ K_ENTER,
        /* KEY_B      */ K_BACKSPACE,
        /* KEY_X      */ K_ALT,
        /* KEY_Y      */ K_CTRL,
        /* KEY_BACK   */ K_TAB,
        /* KEY_GUIDE  */ K_JOY32,
        /* KEY_START  */ K_ESCAPE,
        /* KEY_LSTICK */ K_AUX10,
        /* KEY_RSTICK */ K_AUX2,
        /* KEY_LSHOULD*/ K_MOUSE2,
        /* KEY_RSHOULD*/ K_MOUSE1,
		/* KEY_DUP    */ K_UPARROW,
		/* KEY_DDOWN  */ K_DOWNARROW,
        /* KEY_DLEFT  */ K_LEFTARROW,
        /* KEY_DRIGHT */ K_RIGHTARROW,
    };

    if (button < 0 || button > 15) return 0;
    return keymap[button];
}

static int joy_mouse[2] = { 0 };
static int joy_mouse_prev[2] = { 0 };

static float touch_pos[2] = { 0 };
static float touch_pos_prev[2] = { 0 };
static bool touch_pressed = false;
static bool touch_pressed_prev = false;

// all the menus are now 4:3, so we got a different origin
// 960x720 is the 4:3 resolution we get

static int touch_w = 1920;
static int touch_h = 1088;
static int menu_w = 960;
static int menu_ox = (960 - 725) / 2;
static SDL_GameController *controller = NULL;

extern idSession *session;
extern idSessionLocal sessLocal;

static inline void JoyMouseMotion(Uint8 axis, Sint16 val) {
	constexpr float mouse_modifier = 1.f / 2048.f;
	constexpr int move_modifier = 256;
	constexpr int deadzone = 32;

	if (abs(val) < deadzone) val = 0;

	joy_mouse_prev[axis] = joy_mouse[axis];
	joy_mouse[axis] = val * mouse_modifier;
}


static inline bool JoyGenerateMouseEvents(void) {
	SDL_Event ev = { 0 };

	if (joy_mouse[0] || joy_mouse[1]) {
		ev.type = SDL_MOUSEMOTION;
		ev.motion.xrel = joy_mouse[0];
		ev.motion.yrel = joy_mouse[1];
		SDL_PeepEvents(&ev, 1, SDL_ADDEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
		return true;
	}

	return false;
}

static inline void TouchMotion(float x, float y) {
	// only update cursor position in menus
	if (!session || !sessLocal.GetActiveMenu())
		return;
	touch_pos_prev[0] = touch_pos[0];
	touch_pos_prev[1] = touch_pos[1];
	touch_pos[0] = x * touch_w;
	touch_pos[1] = y * touch_h;
}

static inline void TouchPress(bool pressed) {
	touch_pressed_prev = touch_pressed;
	touch_pressed = pressed;
}

static inline bool TouchGenerateEvents(void) {
	// only update cursor position in menus
	if (!session || !sessLocal.GetActiveMenu())
		return false;

	SDL_Event ev = { 0 };
	bool ret = false;

	if (touch_pos[0] != touch_pos_prev[0] || touch_pos[1] != touch_pos_prev[1]) {
		// just set the current GUI's cursor
		auto m = sessLocal.GetActiveMenu();
		int mx = 0, my = 0;

		// but wait, there's more!
		// fullscreen UIs might be contained in a centered 4:3 window
		// so we have to transform the position (taken from UserInterface.cpp)
		if(r_scaleMenusTo43.GetBool())
			mx = (touch_pos[0] - menu_ox) / (float)menu_w * VIRTUAL_WIDTH;
		else
			mx = touch_pos[0] / (float)touch_w * VIRTUAL_WIDTH;
		my = touch_pos[1] / (float)touch_h * VIRTUAL_HEIGHT;

		m->SetCursor(mx, my);

		// prevent duplicate events until the next touch event
		touch_pos_prev[0] = touch_pos[0];
		touch_pos_prev[1] = touch_pos[1];
		ret = true;
	}

	if (touch_pressed != touch_pressed_prev) {
		ev.type = touch_pressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
		ev.button.timestamp = Sys_Milliseconds();
		ev.button.which = SDL_TOUCH_MOUSEID;
		ev.button.button = SDL_BUTTON_LEFT;
		ev.button.state = touch_pressed ? SDL_PRESSED : SDL_RELEASED;
		ev.button.clicks = 1;
		ev.button.x = touch_pos[0];
		ev.button.y = touch_pos[1];
		SDL_PeepEvents(&ev, 1, SDL_ADDEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
		ret = true;
		// prevent duplicate events until the next touch event
		touch_pressed_prev = touch_pressed;
	}

	return ret;
}

/*
=================
Sys_InitInput
=================
*/
void Sys_InitInput() {
	kbd_polls.SetGranularity(64);
	mouse_polls.SetGranularity(64);
	joystick_polls.SetGranularity(64);

	SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);

#ifdef __vita__
	sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_STOP);
	sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_STOP);
#endif
	//SDL_JoystickOpen(0);

	int numjoysticks = SDL_NumJoysticks();
	for (int i = 0; i < numjoysticks; i++) {
		if (SDL_IsGameController(i)) {
			if (!controller) {
				controller = SDL_GameControllerOpen(i);
			}
		}
	}
	
	touch_w = glConfig.vidWidth;
	touch_h = glConfig.vidHeight;
	menu_w = (int)((double)touch_h / 3.0 * 4.0);
	menu_ox = (touch_w - menu_w) / 2;

	in_kbd.SetModified();
}

/*
=================
Sys_ShutdownInput
=================
*/
void Sys_ShutdownInput() {
	kbd_polls.Clear();
	mouse_polls.Clear();
	joystick_polls.Clear();
	if (SDL_WasInit(SDL_INIT_GAMECONTROLLER))
		SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

/*
===========
Sys_InitScanTable
===========
*/
// Windows has its own version due to the tools
#ifndef _WIN32
void Sys_InitScanTable() {
}
#endif

/*
===============
Sys_GetConsoleKey
===============
*/
unsigned char Sys_GetConsoleKey(bool shifted) {
	static unsigned char keys[2] = { '`', '~' };

	if (in_kbd.IsModified()) {
		idStr lang = in_kbd.GetString();

		if (lang.Length()) {
			if (!lang.Icmp("french")) {
				keys[0] = '<';
				keys[1] = '>';
			} else if (!lang.Icmp("german")) {
				keys[0] = '^';
				keys[1] = 176; // °
			} else if (!lang.Icmp("italian")) {
				keys[0] = '\\';
				keys[1] = '|';
			} else if (!lang.Icmp("spanish")) {
				keys[0] = 186; // º
				keys[1] = 170; // ª
			} else if (!lang.Icmp("turkish")) {
				keys[0] = '"';
				keys[1] = 233; // é
			} else if (!lang.Icmp("norwegian")) {
				keys[0] = 124; // |
				keys[1] = 167; // §
			} else if (!lang.Icmp("brazilian")) {
				keys[0] = '\'';
				keys[1] = '"';
			}
		}

		in_kbd.ClearModified();
	}

	return shifted ? keys[1] : keys[0];
}

/*
===============
Sys_MapCharForKey
===============
*/
unsigned char Sys_MapCharForKey(int key) {
	return key & 0xff;
}

/*
===============
Sys_GrabMouseCursor
===============
*/
void Sys_GrabMouseCursor(bool grabIt) {
	int flags;

	if (grabIt)
		flags = GRAB_ENABLE | GRAB_HIDECURSOR | GRAB_SETSTATE;
	else
		flags = GRAB_SETSTATE;

	GLimp_GrabInput(flags);
}

/*
================
Sys_GetEvent
================
*/
#ifdef VITA
static bool VitaIsScreenKeyboardShown() {
	SDL_Window *window = SDL_GetKeyboardFocus();
	return window != NULL && SDL_IsScreenKeyboardShown( window );
}
#endif

sysEvent_t Sys_GetEvent() {
	SDL_Event ev;
	sysEvent_t res = { };
	byte key;

	static const sysEvent_t res_none = { SE_NONE, 0, 0, 0, NULL };

	static char s[SDL_TEXTINPUTEVENT_TEXT_SIZE] = {0};
	static size_t s_pos = 0;

	if (s[0] != '\0') {
		res.evType = SE_CHAR;
		res.evValue = s[s_pos];

		++s_pos;

		if (!s[s_pos] || s_pos == SDL_TEXTINPUTEVENT_TEXT_SIZE) {
			memset(s, 0, sizeof(s));
			s_pos = 0;
		}

		return res;
	}

	static byte c = 0;

	if (c) {
		res.evType = SE_CHAR;
		res.evValue = c;

		c = 0;

		return res;
	}

	// loop until there is an event we care about (will return then) or no more events
	while(SDL_PollEvent(&ev)) {
		switch (ev.type) {
		case SDL_WINDOWEVENT:
			switch (ev.window.event) {
				case SDL_WINDOWEVENT_FOCUS_GAINED: {
						// unset modifier, in case alt-tab was used to leave window and ALT is still set
						// as that can cause fullscreen-toggling when pressing enter...
						SDL_Keymod currentmod = SDL_GetModState();
					
						int newmod = KMOD_NONE;
						if (currentmod & KMOD_CAPS) // preserve capslock
							newmod |= KMOD_CAPS;

						SDL_SetModState((SDL_Keymod)newmod);
					} // new context because visual studio complains about newmod and currentmod not initialized because of the case SDL_WINDOWEVENT_FOCUS_LOST

					GLimp_GrabInput(GRAB_ENABLE | GRAB_REENABLE | GRAB_HIDECURSOR);
					break;
				case SDL_WINDOWEVENT_FOCUS_LOST:
					GLimp_GrabInput(0);
					break;
			}

			continue; // handle next event

		case SDL_KEYDOWN:
			if (ev.key.keysym.sym == SDLK_RETURN && (ev.key.keysym.mod & KMOD_ALT) > 0) {
				cvarSystem->SetCVarBool("r_fullscreen", !renderSystem->IsFullScreen());
				PushConsoleEvent("vid_restart");
				return res_none;
			}

			// fall through
		case SDL_KEYUP:
		{
			// workaround for AZERTY-keyboards, which don't have 1, 2, ..., 9, 0 in first row:
			// always map those physical keys (scancodes) to those keycodes anyway
			// see also https://bugzilla.libsdl.org/show_bug.cgi?id=3188
			SDL_Scancode sc = ev.key.keysym.scancode;
			if(sc == SDL_SCANCODE_0)
			{
				key = '0';
			}
			else if(sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
			{
				// note that the SDL_SCANCODEs are SDL_SCANCODE_1, _2, ..., _9, SDL_SCANCODE_0
				// while in ASCII it's '0', '1', ..., '9' => handle 0 and 1-9 separately
				// (doom3 uses the ASCII values for those keys)
				key = '1' + (sc - SDL_SCANCODE_1);
			}
			else
			{
				key = mapkey(ev.key.keysym.sym);
			}

			if(!key) {
				if (ev.key.keysym.scancode == SDL_SCANCODE_GRAVE) { // TODO: always do this check?
					key = Sys_GetConsoleKey(true);
				} else {
					if (ev.type == SDL_KEYDOWN) {
						common->Warning("unmapped SDL key %d", ev.key.keysym.sym);
					}
					continue; // handle next event
				}
			}
		}

#ifdef VITA
			if ( VitaIsScreenKeyboardShown() &&
				( key == K_UPARROW || key == K_DOWNARROW || key == K_LEFTARROW || key == K_RIGHTARROW ) ) {
				continue;
			}
#endif

			res.evType = SE_KEY;
			res.evValue = key;
			res.evValue2 = ev.key.state == SDL_PRESSED ? 1 : 0;

			kbd_polls.Append(kbd_poll_t(key, ev.key.state == SDL_PRESSED));

			if (key == K_BACKSPACE && ev.key.state == SDL_PRESSED)
				c = key;

			return res;

		case SDL_CONTROLLERAXISMOTION:
			if (ev.jaxis.axis == 0 || ev.jaxis.axis == 1)
			{
				// only send left stick motion to the event system, cause
				// right stick emulates mouse
				res.evType = SE_JOYSTICK_AXIS;
				res.evValue = ev.caxis.axis;
				res.evValue2 = ev.caxis.value / 256;
				joystick_polls.Append(joystick_poll_t(res.evValue, res.evValue2));
				return res;
			}
			else if ( ev.jaxis.axis == 1) // top/bot
			{
				
			}
			else if (ev.caxis.axis == 2 || ev.caxis.axis == 3)
			{
				JoyMouseMotion(ev.caxis.axis - 2, ev.caxis.value);
				continue;
			}

		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP:
			key = JoyToKey(ev.cbutton.button);
			if (!key) continue;
#ifdef VITA
			if ( VitaIsScreenKeyboardShown() &&
				( key == K_UPARROW || key == K_DOWNARROW || key == K_LEFTARROW || key == K_RIGHTARROW ) ) {
				continue;
			}
#endif
			res.evType = SE_KEY;
			res.evValue = key;
			res.evValue2 = ev.type == SDL_CONTROLLERBUTTONDOWN ? 1 : 0;
			kbd_polls.Append(kbd_poll_t(key, ev.type == SDL_CONTROLLERBUTTONDOWN));
			if (key == K_BACKSPACE && ev.type == SDL_CONTROLLERBUTTONDOWN)
				c = key;
			return res;

		case SDL_TEXTINPUT:
			if (ev.text.text[0]) {
				res.evType = SE_CHAR;
				res.evValue = ev.text.text[0];

				if (ev.text.text[1] != '\0')
				{
					memcpy(s, ev.text.text, SDL_TEXTINPUTEVENT_TEXT_SIZE);
					s_pos = 1; // pos 0 is returned
				}
				return res;
			}

			continue; // handle next event

		case SDL_TEXTEDITING:
			// on windows we get this event whenever the window gains focus.. just ignore it.
			continue;

		case SDL_FINGERDOWN:
		case SDL_FINGERUP:
			// TODO: maybe just do the same shit as in a MOUSEBUTTONDOWN event, but
			// with K_MOUSE1 only?
			TouchPress(ev.type == SDL_FINGERDOWN);
		// fallthrough
		case SDL_FINGERMOTION:
			TouchMotion(ev.tfinger.x, ev.tfinger.y);
			continue;

		case SDL_MOUSEMOTION:
			res.evType = SE_MOUSE;
			res.evValue = ev.motion.xrel;
			res.evValue2 = ev.motion.yrel;

			mouse_polls.Append(mouse_poll_t(M_DELTAX, ev.motion.xrel));
			mouse_polls.Append(mouse_poll_t(M_DELTAY, ev.motion.yrel));

			return res;

		case SDL_MOUSEWHEEL:
			res.evType = SE_KEY;

			if (ev.wheel.y > 0) {
				res.evValue = K_MWHEELUP;
				mouse_polls.Append(mouse_poll_t(M_DELTAZ, 1));
			} else {
				res.evValue = K_MWHEELDOWN;
				mouse_polls.Append(mouse_poll_t(M_DELTAZ, -1));
			}

			res.evValue2 = 1;

			return res;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			res.evType = SE_KEY;

			switch (ev.button.button) {
			case SDL_BUTTON_LEFT:
				res.evValue = K_MOUSE1;
				mouse_polls.Append(mouse_poll_t(M_ACTION1, ev.button.state == SDL_PRESSED ? 1 : 0));
				break;
			case SDL_BUTTON_MIDDLE:
				res.evValue = K_MOUSE3;
				mouse_polls.Append(mouse_poll_t(M_ACTION3, ev.button.state == SDL_PRESSED ? 1 : 0));
				break;
			case SDL_BUTTON_RIGHT:
				res.evValue = K_MOUSE2;
				mouse_polls.Append(mouse_poll_t(M_ACTION2, ev.button.state == SDL_PRESSED ? 1 : 0));
				break;
			default:
				// handle X1 button and above
				if( ev.button.button < SDL_BUTTON_LEFT + 8 ) // doesn't support more than 8 mouse buttons
				{
					int buttonIndex = ev.button.button - SDL_BUTTON_LEFT;
					res.evValue = K_MOUSE1 + buttonIndex;
					mouse_polls.Append( mouse_poll_t( M_ACTION1 + buttonIndex, ev.button.state == SDL_PRESSED ? 1 : 0 ) );
				}
				else
				continue; // handle next event
			}

			res.evValue2 = ev.button.state == SDL_PRESSED ? 1 : 0;

			return res;

		case SDL_QUIT:
			PushConsoleEvent("quit");
			return res_none;

		case SDL_USEREVENT:
			switch (ev.user.code) {
			case SE_CONSOLE:
				res.evType = SE_CONSOLE;
				res.evPtrLength = (intptr_t)ev.user.data1;
				res.evPtr = ev.user.data2;
				return res;
			default:
				common->Warning("unknown user event %u", ev.user.code);
				continue; // handle next event
			}
		default:
			// ok, I don't /really/ care about unknown SDL events. only uncomment this for debugging.
			// common->Warning("unknown SDL event 0x%x", ev.type);
			continue; // handle next event
		}
	}

	return res_none;
}

/*
================
Sys_ClearEvents
================
*/
void Sys_ClearEvents() {
	SDL_Event ev;

	while (SDL_PollEvent(&ev))
		;

	kbd_polls.SetNum(0, false);
	mouse_polls.SetNum(0, false);
	joystick_polls.SetNum(0, false);
}

/*
================
Sys_GenerateEvents
================
*/
void Sys_GenerateEvents() {
	char *s = Sys_ConsoleInput();

	if (s)
		PushConsoleEvent(s);

	JoyGenerateMouseEvents();
	TouchGenerateEvents();

	SDL_PumpEvents();
}

/*
================
Sys_PollKeyboardInputEvents
================
*/
int Sys_PollKeyboardInputEvents() {
	return kbd_polls.Num();
}

/*
================
Sys_ReturnKeyboardInputEvent
================
*/
int Sys_ReturnKeyboardInputEvent(const int n, int &key, bool &state) {
	if (n >= kbd_polls.Num())
		return 0;

	key = kbd_polls[n].key;
	state = kbd_polls[n].state;
	return 1;
}

/*
================
Sys_EndKeyboardInputEvents
================
*/
void Sys_EndKeyboardInputEvents() {
	kbd_polls.SetNum(0, false);
}

/*
================
Sys_PollJoystickInputEvents
================
*/
int Sys_PollJoystickInputEvents() {
	return joystick_polls.Num();
}

/*
================
Sys_ReturnJoystickInputEvent
================
*/
int Sys_ReturnJoystickInputEvent(const int n, int &axis, int &value) {
	if (n >= joystick_polls.Num())
		return 0;

	axis = joystick_polls[n].axis;
	value = joystick_polls[n].value;
	return 1;
}

/*
================
Sys_EndJoystickInputEvents
================
*/
void Sys_EndJoystickInputEvents() {
	joystick_polls.SetNum(0, false);
}

/*
================
Sys_PollMouseInputEvents
================
*/
int Sys_PollMouseInputEvents() {
	return mouse_polls.Num();
}

/*
================
Sys_ReturnMouseInputEvent
================
*/
int	Sys_ReturnMouseInputEvent(const int n, int &action, int &value) {
	if (n >= mouse_polls.Num())
		return 0;

	action = mouse_polls[n].action;
	value = mouse_polls[n].value;
	return 1;
}

/*
================
Sys_EndMouseInputEvents
================
*/
void Sys_EndMouseInputEvents() {
	mouse_polls.SetNum(0, false);
}
