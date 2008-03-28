#ifndef MAIL_FRONT__POP3__H__
#define MAIL_FRONT__POP3__H__

#include "constants.h"
struct str;

struct command
{
  const char* name;
  void (*fn0)(void);
  void (*fn1)(const struct str* s);
  const char* sanitized;
};
typedef struct command command;

extern const char err_internal[];
extern const char err_unimpl[];
extern const char ok[];
extern const char err_syntax[];
extern void logmsg(const char* msg);
extern void respond(const char*);
extern void cmd_capa(void);

extern command commands[];
extern int startup(int argc, char* argv[]);

#endif
