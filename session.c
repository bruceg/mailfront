#include <string.h>
#include <stdlib.h>
#include <str/env.h>
#include "mailfront.h"

struct session session = {
  .protocol = 0,
};

void session_init(void)
{
  memset(&session, 0, sizeof session);
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

int session_setenv(const char* name, const char* value, int overwrite)
{
  return envstr_set(&session.env, name, value, overwrite);
}

void session_resetenv(void)
{
  session.env.len = 0;
}
