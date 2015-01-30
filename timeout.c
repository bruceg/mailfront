/* timeout.c - Timeout setup function
 * Copyright (C) 2008  Bruce Guenter <bruce@untroubled.org> or FutureQuest, Inc.
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
#include <bglibs/sysdeps.h>
#include <unistd.h>
#include <bglibs/iobuf.h>
#include <bglibs/sig.h>

unsigned long timeout;
unsigned long session_timeout;
extern const int authenticating;

static void handle_alarm(int unused)
{
  exit(0);
  (void)unused;
}

void set_timeout(void)
{
  const char* tmp;

  timeout = 0;
  if ((authenticating && (tmp = getenv("AUTH_TIMEOUT")) != 0)
      || (tmp = getenv("TIMEOUT")) != 0)
    timeout = strtoul(tmp, 0, 10);
  if (timeout <= 0) timeout = 1200;
  inbuf.io.timeout = timeout * 1000;
  outbuf.io.timeout = timeout * 1000;

  session_timeout = 0;
  if ((authenticating && (tmp = getenv("AUTH_SESSION_TIMEOUT")) != 0)
      || (tmp = getenv("SESSION_TIMEOUT")) != 0)
    session_timeout = strtoul(tmp, 0, 10);
  if (session_timeout <= 0) timeout = 86400;
  sig_alarm_catch(handle_alarm);
  alarm(session_timeout);
}
