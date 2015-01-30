/* pop3-capa.c -- POP3 CAPA command
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
#include <string.h>
#include <bglibs/obuf.h>
#include <bglibs/str.h>
#include <cvm/sasl.h>
#include "pop3.h"

static const char caps[] =
  "PIPELINING\r\n"
  "TOP\r\n"
  "UIDL\r\n"
  "USER\r\n";

static str auth_resp;

void cmd_capa(void)
{
  int sasl;

  if ((sasl = sasl_auth_caps(&auth_resp)) == -1
      || (sasl == 1 && auth_resp.len <= 5)) {
    respond(err_internal);
    return;
  }
  respond(ok);
  if (sasl) {
    obuf_puts(&outbuf, "SASL ");
    obuf_write(&outbuf, auth_resp.s + 5, auth_resp.len - 5);
    obuf_puts(&outbuf, CRLF);
  }
  obuf_puts(&outbuf, caps);
  respond(".");
}
