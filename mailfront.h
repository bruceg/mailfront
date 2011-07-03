#ifndef MAIL_FRONT__MAILFRONT__H__
#define MAIL_FRONT__MAILFRONT__H__

#include "responses.h"
#include <iobuf/iobuf.h>
#include <str/str.h>
#include "constants.h"

#define FLAG_NEED_FILE (1<<0)

#define PLUGIN_VERSION 3

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

#define PROTOCOL_VERSION 3

struct protocol
{
  unsigned version;
  const char* name;
  int (*respond_line)(unsigned number, int final,
		      const char* msg, unsigned long len);
  int (*init)(void);
  int (*mainloop)(void);
};

/* From getprotoenv.c */
extern const char* getprotoenv(const char*);

/* From mailfront.c */
extern const char UNKNOWN[];
extern const response* handle_helo(str* host);
extern const response* handle_reset(void);
extern const response* handle_sender(str* sender);
extern const response* handle_recipient(str* recip);
extern const response* handle_data_start(void);
extern void handle_data_bytes(const char* bytes, unsigned len);
extern const response* handle_message_end(void);
extern int respond(const response*);
extern int respond_line(unsigned number, int final,
			const char* msg, unsigned long len);
extern const response* backend_data_block(const char* data, unsigned long len);
extern int scratchfile(void);

/* From netstring.c */
int get_netstring_len(ibuf* in, unsigned long* i);
int get_netstring(ibuf* in, str* s);

/* From session.c */
extern const char* session_protocol(void);
extern const char* session_getenv(const char* name);
extern unsigned long session_getenvu(const char* name);
extern int session_exportenv(void);
extern int session_putenv(const char* s);
extern int session_setenv(const char* name, const char* value, int overwrite);
extern void session_resetenv(void);
extern void session_delnum(const char* name);
extern void session_delstr(const char* name);
extern unsigned long session_getnum(const char* name, unsigned long dflt);
extern int session_hasnum(const char* name, unsigned long* num);
extern const char* session_getstr(const char* name);
extern void session_setnum(const char* name, unsigned long value);
extern void session_setstr(const char* name, const char* value);

#endif /* MAIL_FRONT__MAILFRONT__H__ */
