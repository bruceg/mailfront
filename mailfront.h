#ifndef MAIL_FRONT__MAILFRONT__H__
#define MAIL_FRONT__MAILFRONT__H__

#include "responses.h"
#include <adt/ghash.h>
#include <iobuf/iobuf.h>
#include <str/str.h>
#include "constants.h"

#define FLAG_NEED_FILE (1<<0)

#define PLUGIN_VERSION 2

struct plugin
{
  unsigned version;
  struct plugin* next;
  const char* name;
  unsigned flags;
  const response* (*init)(void);
  const response* (*helo)(str*);
  const response* (*reset)(void);
  const response* (*sender)(str*);
  const response* (*recipient)(str*);
  const response* (*data_start)(int fd);
  const response* (*data_block)(const char* bytes, unsigned long len);
  const response* (*message_end)(int fd);
};

#define PROTOCOL_VERSION 2

struct protocol
{
  unsigned version;
  const char* name;
  int (*respond_line)(unsigned number, int final,
		      const char* msg, unsigned long len);
  int (*init)(void);
  int (*mainloop)(void);
};

struct session
{
  struct protocol* protocol;
  struct plugin* backend;
  struct ghash strs;
  struct ghash nums;
  str env;
  int fd;
};

GHASH_DECL(session_strs,const char*,const char*);
GHASH_DECL(session_nums,const char*,unsigned long);

extern struct session session;

/* From builtins.c */
extern struct plugin builtin_plugins[];

/* From mailfront.c */
extern const char UNKNOWN[];
extern const response* handle_helo(str* host);
extern const response* handle_init(void);
extern const response* handle_reset(void);
extern const response* handle_sender(str* sender);
extern const response* handle_recipient(str* recip);
extern const response* handle_data_start(void);
extern void handle_data_bytes(const char* bytes, unsigned len);
extern const response* handle_message_end(void);
extern int respond(const response*);
extern int respond_line(unsigned number, int final,
			const char* msg, unsigned long len);
extern int scratchfile(void);

/* From netstring.c */
int get_netstring_len(ibuf* in, unsigned long* i);
int get_netstring(ibuf* in, str* s);

/* From modules.c */
extern struct plugin* plugin_list;
extern struct plugin* plugin_tail;
extern unsigned module_flags;
extern void add_plugin(struct plugin*);
extern const response* load_modules(const char* protocol_name,
				    const char* backend_name,
				    const char** plugins);

/* From session.c */
extern void session_init(void);
extern const char* session_getenv(const char* name);
extern unsigned long session_getenvu(const char* name);
extern int session_exportenv(void);
extern int session_setenv(const char* name, const char* value, int overwrite);
extern void session_resetenv(void);
extern void session_delnum(const char* name);
extern void session_delstr(const char* name);
extern unsigned long session_getnum(const char* name, unsigned long dflt);
extern int session_hasnum(const char* name, unsigned long* num);
extern const char* session_getstr(const char* name);
extern void session_setnum(const char* name, unsigned long value);
extern void session_setstr(const char* name, const char* value);

/* Defined by a protocol module */
extern int protocol_init(void);
extern int protocol_mainloop(void);

#endif /* MAIL_FRONT__MAILFRONT__H__ */
