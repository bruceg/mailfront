#include <string.h>
#include <stdlib.h>
#include <msg/msg.h>
#include <str/env.h>
#include "mailfront-internal.h"

struct session session = {
  .protocol = 0,
};

const char* session_protocol(void)
{
  return session.protocol->name;
}

const char* session_getenv(const char* name)
{
  const char* s;
  if ((s = envstr_get(&session.env, name)) == 0)
    s = getenv(name);
  return s;
}

static unsigned long min_u_s(unsigned long u, const char* s)
{
  unsigned long newu;
  if (s != 0) {
    if ((newu = strtoul(s, (char**)&s, 10)) != 0 &&
	*s == 0 &&
	(u == 0 || newu < u))
      u = newu;
  }
  return u;
}

/* Returns the smallest non-zero value set */
unsigned long session_getenvu(const char* name)
{
  unsigned i;
  const unsigned namelen = strlen(name);
  unsigned long val;
  val = min_u_s(0, getenv(name));
  for (i = 0; i < session.env.len; i += strlen(session.env.s + i) + 1) {
    if (memcmp(session.env.s + i, name, namelen) == 0 &&
	session.env.s[i + namelen] == '=')
      val = min_u_s(val, session.env.s + i + namelen + 1);
  }
  return val;
}

int session_exportenv(void)
{
  unsigned i;
  for (i = 0; i < session.env.len; i += strlen(session.env.s + i) + 1)
    if (putenv(session.env.s + i) != 0) return 0;
  return 1;
}

int session_putenv(const char* s)
{
  if (session.env.len > 0)
    if (!str_catc(&session.env, 0))
      return 0;
  return str_catb(&session.env, s, strlen(s) + 1);
}

int session_setenv(const char* name, const char* value, int overwrite)
{
  return envstr_set(&session.env, name, value, overwrite);
}

void session_resetenv(void)
{
  session.env.len = 0;
}


GHASH_DEFN(session_strs,const char*,const char*,
	   adt_hashsp,adt_cmpsp,0,adt_copysp,0,adt_freesp);
GHASH_DEFN(session_nums,const char*,unsigned long,
	   adt_hashsp,adt_cmpsp,0,0,0,0);

void session_delnum(const char* name)
{
  session_nums_remove(&session.nums, &name);
}

void session_delstr(const char* name)
{
  session_strs_remove(&session.strs, &name);
}

const char* session_getstr(const char* name)
{
  struct session_strs_entry* p;
  if ((p = session_strs_get(&session.strs, &name)) == 0)
    return 0;
  return p->data;
}

void session_setstr(const char* name, const char* value)
{
  if (session_strs_add(&session.strs, &name, &value) == 0)
    die_oom(111);
}

int session_hasnum(const char* name, unsigned long* num)
{
  struct session_nums_entry* p;
  if ((p = session_nums_get(&session.nums, &name)) == 0)
    return 0;
  if (num != 0)
    *num = p->data;
  return 1;
}

unsigned long session_getnum(const char* name, unsigned long dflt)
{
  struct session_nums_entry* p;
  if ((p = session_nums_get(&session.nums, &name)) == 0)
    return dflt;
  return p->data;
}

void session_setnum(const char* name, unsigned long value)
{
  if (session_nums_add(&session.nums, &name, &value) == 0)
    die_oom(111);
}


void session_init(void)
{
  memset(&session, 0, sizeof session);
  session.fd = -1;
  session_strs_init(&session.strs);
  session_nums_init(&session.nums);
}
