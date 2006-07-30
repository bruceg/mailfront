#include <unistd.h>
#include "mailfront.h"
#include <iobuf/iobuf.h>
#include <dict/dict.h>
#include <dict/load.h>
#include "conf_qmail.c"
#include <cdb/cdb.h>

static RESPONSE(accept,250,0);
static RESPONSE(no_chdir,451,"4.3.0 Could not change to the qmail directory.");
static RESPONSE(badmailfrom,553,"5.1.0 Sorry, your envelope sender is in my badmailfrom list.");
static RESPONSE(bmt,553,"5.1.1 Sorry, that address is in my badrcptto list.");

static dict bmf;
static dict rh;
static dict brt;
static str tmp;
static int mrh_fd;
static struct cdb mrh;

static int lower(str* s)
{
  str_lower(s);
  return 1;
}

static const response* validate_init(void)
{
  const char* qh;
  
  if ((qh = getenv("QMAILHOME")) == 0)
    qh = conf_qmail;
  if (chdir(qh) == -1) return &resp_no_chdir;
  if (!dict_load_list(&bmf, "control/badmailfrom", 0, lower))
    return &resp_internal;
  if (!dict_load_list(&rh, "control/rcpthosts", 0, lower))
    return &resp_internal;
  if (!dict_load_list(&brt, "control/badrcptto", 0, lower))
    return &resp_internal;
  if ((mrh_fd = open("control/morercpthosts.cdb", O_RDONLY)) != -1)
    cdb_init(&mrh, mrh_fd);
  return 0;
}

static const response* validate_sender(str* sender)
{
  int at;
  str_copy(&tmp, sender);
  str_lower(&tmp);
  if (dict_get(&bmf, &tmp) != 0) return &resp_badmailfrom;
  if ((at = str_findlast(sender, '@')) > 0) {
    str_copyb(&tmp, sender->s + at, sender->len - at);
    str_lower(&tmp);
    if (dict_get(&bmf, &tmp)) return &resp_badmailfrom;
  }
  return 0;
}

static const response* validate_recipient(str* recipient)
{
  int at;

  str_copy(&tmp, recipient);
  str_lower(&tmp);
  if (dict_get(&brt, &tmp) != 0)
    return &resp_bmt;
  if ((at = str_findlast(recipient, '@')) > 0) {
    str_copyb(&tmp, recipient->s + at, recipient->len - at);
    str_lower(&tmp);
    if (dict_get(&brt, &tmp))
      return &resp_bmt;
    ++at;
    str_copyb(&tmp, recipient->s + at, recipient->len - at);
    str_lower(&tmp);
    for (;;) {
      if (dict_get(&rh, &tmp))
	return &resp_accept;
      /* NOTE: qmail-newmrh automatically lowercases the keys in this CDB */
      if (mrh_fd != -1 && cdb_find(&mrh, tmp.s, tmp.len) == 1)
	return &resp_accept;
      if ((at = str_findnext(&tmp, '.', 1)) <= 0)
	break;
      str_lcut(&tmp, at);
    }
  }
  return 0;
}

STRUCT_PLUGIN(backend_validate) = {
  .init = validate_init,
  .sender = validate_sender,
  .recipient = validate_recipient,
};

