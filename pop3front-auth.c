/* pop3front-auth.c -- POP3 authentication front-end
 * Copyright (C) 2008  Bruce Guenter <bruceg@em.ca> or FutureQuest, Inc.
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
#include <unistd.h>
#include <cvm/v2client.h>
#include <iobuf/iobuf.h>
#include <str/iter.h>
#include <str/str.h>
#include <cvm/sasl.h>
#include "pop3.h"

const char program[] = "pop3front-auth";
const int authenticating = 1;

static const char* cvm;
static char** nextcmd;
static const char* domain;

static str user;

static struct sasl_auth saslauth = { .prefix = "+ " };

static void do_exec(void)
{
  if (!cvm_setugid() || !cvm_setenv())
    respond(err_internal);
  else {
    alarm(0);
    execvp(nextcmd[0], nextcmd);
    respond("-ERR Could not execute second stage");
  }
  _exit(1);
}

static void cmd_capa(void)
{
  static str auth_resp;
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
  obuf_puts(&outbuf, "TOP");
  obuf_puts(&outbuf, CRLF);
  obuf_puts(&outbuf, "UIDL");
  obuf_puts(&outbuf, CRLF);
  obuf_puts(&outbuf, "USER");
  obuf_puts(&outbuf, CRLF);
  respond(".");
}

static void cmd_auth_none(void)
{
  static str auth_resp;
  striter i;

  switch (sasl_auth_caps(&auth_resp)) {
  case 0:
    respond(ok);
    break;
  case 1:
    if (auth_resp.len <= 5) {
      respond(err_internal);
      return;
    }
    respond(ok);
    str_lcut(&auth_resp, 5);
    str_strip(&auth_resp);
    striter_loop(&i, &auth_resp, ' ') {
      obuf_write(&outbuf, i.startptr, i.len);
      obuf_puts(&outbuf, CRLF);
    }
    break;
  default:
    respond(err_internal);
    return;
  }
  respond(".");
}

static void cmd_auth(const str* s)
{
  int i;
  if ((i = sasl_auth1(&saslauth, s)) == 0) 
    do_exec();
  obuf_write(&outbuf, "-ERR ", 5);
  respond(sasl_auth_msg(&i));
}

static void cmd_user(const str* s)
{
  if (!str_copy(&user, s))
    respond(err_internal);
  else
    respond(ok);
}

static void cmd_pass(const str* s)
{
  if (user.len == 0)
    respond("-ERR Send USER first");
  else {
    int cr;
    if ((cr = cvm_authenticate_password(cvm, user.s, domain, s->s, 1)) == 0)
      do_exec();
    str_truncate(&user, 0);
    if (cr == CVME_PERMFAIL)
      respond("-ERR Authentication failed");
    else
      respond(err_internal);
  }
}

static void cmd_quit(void)
{
  respond(ok);
  exit(0);
}

command commands[] = {
  { "CAPA", cmd_capa,      0,        0 },
  { "AUTH", cmd_auth_none, cmd_auth, 0 },
  { "PASS", 0,             cmd_pass, "PASS XXXXXXXX" },
  { "QUIT", cmd_quit,      0,        0 },
  { "USER", 0,             cmd_user, 0 },
  { 0,      0,             0,        0 }
};

int startup(int argc, char* argv[])
{
  static const char usage[] = "usage: pop3front-auth cvm program [args...]\n";
  if ((domain = cvm_ucspi_domain()) == 0)
    domain = "unknown";
  if (argc < 3) {
    obuf_putsflush(&errbuf, usage);
    return 0;
  }
  cvm = argv[1];
  nextcmd = argv+2;
  if (!sasl_auth_init(&saslauth)) {
    respond("-ERR Could not initialize SASL AUTH");
    return 0;
  }
  return 1;
}
