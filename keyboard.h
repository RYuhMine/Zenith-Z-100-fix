//Margaret Black
//2020-2024

/*****************************************************************************
 * This program is free software. You can redistribute it and / or modify it *
 * under the terms of the GNU General Public License version 2 as  published *
 * by the Free Software Foundation.                                          *
 *                                                                           *
 * This program is distributed in the hope  that  it  will  be  useful,  but *
 * WITHOUT  ANY   WARRANTY,   without   even   the   implied   warranty   of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU  General *
 * Public License for more details.                                          *
 *****************************************************************************/

typedef struct
{
	unsigned char fifo[17];
	unsigned int dataReg;
	int fifoHead;
	int fifoTail;
	int fifoCount;
	unsigned char pressed[128];

	int autoRepeatOn;
	int keyClickOn;
	int keyboardEnabled;
	int ASCIImode;
	int interruptsEnabled;

	int capsLock;

	int controlPressed;

	int requestInterrupt;
} Keyboard;

void keyboardCommandWrite(Keyboard*,unsigned int);

unsigned int keyboardStatusRead(Keyboard*);

unsigned int keyboardDataRead(Keyboard*);

void keyboardReset(Keyboard*);

Keyboard* newKeyboard();

void doclick();
void dobeep();


//from UI
void keyaction(Keyboard*,int code);
enum { KEY_SHIFT_RIGHT = 0x100, KEY_SHIFT_LEFT, KEY_CONTROL, KEY_CAPS_LOCK, KEY_FAST_REPEAT };
int keyboardScanCode(unsigned int code);
void keyboardKeyEvent(Keyboard*, unsigned int code, int pressed);
void keydown(char* name);
void keyup(char* name);
