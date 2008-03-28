/* iobytes.c - Report the number of I/O bytes
 * Copyright (C) 2008  Bruce Guenter <bruce@untroubled.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <iobuf/iobuf.h>
#include <msg/msg.h>
#include <str/str.h>

void report_io_bytes(void)
{
  static str tmp;
  if (str_copys(&tmp, "bytes in: ") &&
      str_catu(&tmp, inbuf.io.offset) &&
      str_cats(&tmp, " bytes out: ") &&
      str_catu(&tmp, outbuf.io.offset))
    msg1(tmp.s);
}

