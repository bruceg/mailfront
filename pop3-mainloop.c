/* pop3-mainloop.c - POP3 server main loop
 * Copyright (C) 2001  Bruce Guenter <bruceg@em.ca> or FutureQuest, Inc.
 * Development of this program was sponsored by FutureQuest, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Contact information:
 * FutureQuest Inc.
 * PO BOX 623127
 * Oviedo FL 32762-3127 USA
 * http://www.FutureQuest.net/
 * ossi@FutureQuest.net
 */
#include <stdlib.h>
#include <string.h>
#include <iobuf/iobuf.h>
#include <str/str.h>
#include "pop3.h"

const int msg_show_pid = 1;

static unsigned timeout;
static str line;
static str cmd;
static str arg;

static int parse_line(void)
{
  unsigned i;
  i = 0;
  while (i < line.len && line.s[i] != SPACE) ++i;
  if (!str_copyb(&cmd, line.s, i)) return 0;
  while (i < line.len && line.s[i] == SPACE) ++i;
  if (!str_copyb(&arg, line.s+i, line.len-i)) return 0;
  str_upper(&cmd);
  return 1;
}

static void dispatch_line(void)
{
  command* c;

  for (c = commands; c->name != 0; ++c) {
    if (str_diffs(&cmd, c->name) == 0) {
      log(c->sanitized ? c->sanitized : line.s);
      if (arg.len == 0) {
	if (c->fn0 == 0)
	  respond(err_syntax);
	else
	  c->fn0();
      }
      else {
	if (c->fn1 == 0)
	  respond(err_syntax);
	else
	  c->fn1(&arg);
      }
      return;
    }
  }
  log(line.s);
  respond(err_unimpl);
}

int main(int argc, char* argv[])
{
  const char* tmp;

  timeout = 0;
  if ((tmp = getenv("TIMEOUT")) != 0) timeout = strtoul(tmp, 0, 10);
  if (timeout <= 0) timeout = 1200;
  inbuf.io.timeout = timeout * 1000;
  outbuf.io.timeout = timeout * 1000;

  if (!startup(argc, argv)) return 0;
  respond(ok);
  while (ibuf_getstr_crlf(&inbuf, &line)) {
    if (!parse_line())
      respond(err_internal);
    else
      dispatch_line();
  }
  if (ibuf_timedout(&inbuf))
    respond("-ERR Connection timed out");
  return 0;
}
