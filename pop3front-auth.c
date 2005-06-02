/* pop3front-auth.c -- POP3 authentication front-end
 * Copyright (C) 2005  Bruce Guenter <bruceg@em.ca> or FutureQuest, Inc.
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
#include <unistd.h>
#include <cvm/v2client.h>
#include <iobuf/iobuf.h>
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
    execvp(nextcmd[0], nextcmd);
    respond("-ERR Could not execute second stage");
  }
  _exit(1);
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
  { "AUTH", 0,        cmd_auth, 0 },
  { "PASS", 0,        cmd_pass, "PASS XXXXXXXX" },
  { "QUIT", cmd_quit, 0,        0 },
  { "USER", 0,        cmd_user, 0 },
  { 0,      0,        0,        0 }
};

int startup(int argc, char* argv[])
{
  static const char usage[] = "usage: pop3front-auth cvm program [args...]\n";
  domain = getenv("TCPLOCALHOST");
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
