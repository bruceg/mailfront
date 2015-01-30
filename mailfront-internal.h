#ifndef MAIL_FRONT__MAILFRONT_INTERNAL__H__
#define MAIL_FRONT__MAILFRONT_INTERNAL__H__

#include "mailfront.h"
#include <bglibs/ghash.h>

struct session
{
  struct protocol* protocol;
  struct plugin* backend;
  struct ghash strs;
  struct ghash nums;
  str env;
  int fd;
  struct plugin* plugin_list;
  struct plugin* plugin_tail;
  unsigned flags;
  const char* module_path;
};

GHASH_DECL(session_strs,const char*,const char*);
GHASH_DECL(session_nums,const char*,unsigned long);

extern struct session session;

/* From builtins.c */
extern struct plugin builtin_plugins[];

/* From modules.c */
extern void add_plugin(struct plugin*);
extern const response* load_modules(const char* protocol_name,
				    const char* backend_name,
				    const char** plugins);

/* From session.c */
extern void session_init(void);

#endif /* MAIL_FRONT__MAILFRONT_INTERNAL__H__ */
