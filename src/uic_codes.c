/*
 * uic_codes.c
 *
 * Copyright (C) 2014 Marcos Vives Del Sol <socram8888@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <assert.h>
#include "uic_codes.h"

uic_message uic_ground_to_train_messages[] = {
	{ 0x00, "Test" },
	{ 0x02, "Run slower" },
	{ 0x03, "Extension of telegram" },
	{ 0x04, "Run faster" },
	{ 0x06, "Written order" },
	{ 0x08, "Speech" },
	{ 0x09, "Emergency stop" },
	{ 0x0C, "Announcem. by loudspeaker" },
	{ 0x55, "Idle" },
	{ -1, NULL }
};

uic_message uic_train_to_ground_messages[] = {
	{ 0x08, "Communic. desired" },
	{ 0x0A, "Acknowl. of order" },
	{ 0x06, "Advice" },
	{ 0x00, "Test" },
	{ 0x09, "Train staff wish to comm." },
	{ 0x0C, "Telephone link desired" },
	{ 0x03, "Extension of telegram" },
	{ -1, NULL }
};

const char * uic_message_meaning(unsigned int code,
	unsigned int type)
{
	uic_message * messages;
	if (type == UIC_TYPE_GROUNDTRAIN) {
		messages = uic_ground_to_train_messages;
	} else if (type == UIC_TYPE_TRAINGROUND) {
		messages = uic_train_to_ground_messages;
	} else {
		assert(0);
	}

	while (messages->code != -1) {
		if (messages->code == code) {
			return messages->meaning;
		}
		messages++;
	}

	return "Unknown";
}
