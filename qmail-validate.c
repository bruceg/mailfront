#include <unistd.h>
#include "mailfront.h"
#include "iobuf/iobuf.h"
#include "dict/dict.h"
#include "conf_qmail.h"

static dict bmf;
static dict rh;
static str tmp;

static int read_list(const char* filename, dict* d)
{
  ibuf in;

  if (!dict_init(d)) return 0;
  if (ibuf_open(&in, filename, 0)) {
    while (ibuf_getstr(&in, &tmp, '\n')) {
      str_strip(&tmp);
      if (tmp.len > 0 && tmp.s[0] != '#')
	if (!dict_add(d, &tmp, 0)) {
	  ibuf_close(&in);
	  return 0;
	}
    }
    ibuf_close(&in);
  }
  return 1;
}

const response* qmail_validate_init(void)
{
  static const response resp_no_chdir = {0,451,"Could not change to the qmail directory."};
  static const response resp_error = {0,451,"Internal error."};
  
  if (chdir(conf_qmail) == -1) return &resp_no_chdir;
  if (!read_list("control/badmailfrom", &bmf)) return &resp_error;
  if (!read_list("control/rcpthosts", &rh)) return &resp_error;
  return 0;
}

const response* qmail_validate_sender(const str* sender)
{
  static const response resp = {0,553,"Sorry, your envelope sender is in my badmailfrom list."};
  int at;
  if (dict_get(&bmf, sender) != 0) return &resp;
  if ((at = str_findlast(sender, '@')) > 0) {
    ++at;
    str_copyb(&tmp, sender->s + at, sender->len - at);
    if (dict_get(&bmf, &tmp)) return &resp;
  }
  return 0;
}

const response* qmail_validate_recipient(const str* recipient)
{
  static const response resp = {0,553,"Sorry, that domain isn't in my list of allowed rcpthosts."};
  int at;

  if ((at = str_findlast(recipient, '@')) > 0) {
    ++at;
    str_copyb(&tmp, recipient->s + at, recipient->len - at);
    if (!dict_get(&rh, &tmp)) return &resp;
  }
  return 0;
}
