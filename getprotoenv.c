#include <systime.h>
#include <stdlib.h>
#include <msg/msg.h>
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
  if (!str_copy2s(&fullname, proto, name)) die_oom(111);
  if ((env = getenv(fullname.s)) != 0
      && env[0] == 0)
    env = 0;
  return env;
}
