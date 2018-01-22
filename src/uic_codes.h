/*
 * uic_codes.h
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

typedef struct {
	unsigned int code;
	char * meaning;
} uic_message;

enum {
	UIC_TYPE_GROUNDTRAIN,
	UIC_TYPE_TRAINGROUND,
};

extern uic_message uic_ground_to_train_messages[];
extern uic_message uic_train_to_ground_messages[];

const char * uic_message_meaning(unsigned int code,
	unsigned int type);
