/*
	This file is part of DNSsnarf.  DNSsnarf is free software: you can
	redistribute it and/or modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation, version 2.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
	details.

	You should have received a copy of the GNU General Public License along with
	this program; if not, write to the Free Software Foundation, Inc., 51
	Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

	Copyright Cyso
*/

#ifndef __HELPER_H__
#define __HELPER_H__

#include "types.h"

void hexdump(void *ptr, int buflen);

u16 be16(u8 *p);
u32 be32(u8 *p);

#endif
