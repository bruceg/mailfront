#ifndef MAIL_FRONT__POP3__H__
#define MAIL_FRONT__POP3__H__

#include "constants.h"
struct str;

struct command
{
  const char* name;
  void (*fn0)(void);
  void (*fn1)(const struct str* s);
};
typedef struct command command;

extern const char err_internal[];
extern const char err_unimpl[];
extern const char ok[];
extern const char err_syntax[];
extern void log(const char* msg);
extern void respond(const char*);

extern command commands[];
extern int startup(int argc, char* argv[]);

#endif
