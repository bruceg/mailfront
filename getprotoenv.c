#include <bglibs/systime.h>
#include <stdlib.h>
#include <bglibs/msg.h>
#include <bglibs/wrap.h>
#include "mailfront.h"

static const char* proto;

const char* getprotoenv(const char* name)
{
  static str fullname;
  const char* env;
  if (proto == 0)
    if ((proto = getenv("PROTO")) == 0)
      proto = "TCP";
  if (name == 0 || *name == 0)
    return proto;
  wrap_str(str_copy2s(&fullname, proto, name));
  if ((env = getenv(fullname.s)) != 0
      && env[0] == 0)
    env = 0;
  return env;
}
