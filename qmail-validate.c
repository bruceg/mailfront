#include <unistd.h>
#include "mailfront.h"
#include "iobuf/iobuf.h"
#include "dict/dict.h"
#include "dict/load.h"
#include "conf_qmail.h"
#include "cdb/cdb.h"

static dict bmf;
static dict rh;
static dict brt;
static str tmp;

const response* qmail_validate_init(void)
{
  static const response resp_no_chdir = {0,451,"Could not change to the qmail directory."};
  static const response resp_error = {0,451,"Internal error."};
  
  if (chdir(conf_qmail) == -1) return &resp_no_chdir;
  if (!dict_load_list(&bmf, "control/badmailfrom", 0)) return &resp_error;
  if (!dict_load_list(&rh, "control/rcpthosts", 0)) return &resp_error;
  if (!dict_load_list(&brt, "control/badrcptto", 0)) return &resp_error;
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

static int cdb_lookup(const char* filename, const str* domain)
{
  int fd;
  int result = 0;
  if ((fd = open(filename, O_RDONLY)) != -1) {
    struct cdb db;
    cdb_init(&db, fd);
    if (cdb_find(&db, domain->s, domain->len) == 1) result = 1;
    cdb_free(&db);
    close(fd);
  }
  return result;
}

const response* qmail_validate_recipient(const str* recipient)
{
  static const response resp_rh = {0,553,"Sorry, that domain isn't in my list of allowed rcpthosts."};
  static const response resp_bmt = {0,553,"Sorry, that address is in my badrcptto list."};
  int at;

  if (dict_get(&brt, recipient) != 0) return &resp_bmt;
  if ((at = str_findlast(recipient, '@')) > 0) {
    ++at;
    str_copyb(&tmp, recipient->s + at, recipient->len - at);
    if (dict_get(&rh, &tmp)) return 0;
    if (cdb_lookup("control/morercpthosts.cdb", &tmp)) return 0;
    return &resp_rh;
  }
  return 0;
}
