#include <stdlib.h>
#include <mysql/mysql.h>
#include "mailfront.h"
#include "smtp.h"
#include "iobuf/iobuf.h"
#include "qmail.h"

const char program[] = "smtpd-er";

static MYSQL db;

#define QUOTE '\''
#define BACKSLASH '\\'

static RESPONSE(db_connect, 421, "Unable to connect to database.");
static RESPONSE(nomem, 421, "Out of memory.");
static RESPONSE(qualified, 553, "Sorry, I only allow fully qualified recipient names." );
static RESPONSE(db_query, 421, "Unable to query database.");
static RESPONSE(not_addr, 553, "I can't find that address in my database.");

static int str_catb_quoted(str* s, const char* ptr, unsigned long len)
{
  if (!str_catc(s, QUOTE)) return 0;
  for (; len > 0; --len, ++ptr) {
    switch (*ptr) {
    case QUOTE: if (!str_catc(s, QUOTE)) return 0; break;
    case BACKSLASH: if (!str_catc(s, BACKSLASH)) return 0; break;
    }
    str_catc(s, *ptr);
  }
  return str_catc(s, QUOTE);
}

void handle_reset(void)
{
  qmail_reset();
}

const response* handle_sender(str* sender)
{
  const response* resp;
  if ((resp = qmail_validate_sender(sender)) != 0) return resp;
  return qmail_sender(sender);
}

const response* handle_recipient(str* recip)
{
  static str query;
  
  unsigned long at;
  unsigned long* lengths;
  MYSQL_RES* result;
  MYSQL_ROW row;
  
  if ((at = str_findlast(recip, AT)) == (unsigned long)-1)
    return &resp_qualified;
  
  if (!str_copys(&query, "SELECT forward FROM er WHERE fdomain=") ||
      !str_catb_quoted(&query, recip->s+at+1, recip->len-at-1) ||
      !str_cats(&query, " AND username=") ||
      !str_catb_quoted(&query, recip->s, at))
    return &resp_nomem;

  if (mysql_real_query(&db, query.s, query.len)) return &resp_db_query;
  result = mysql_store_result(&db);
  if (mysql_num_rows(result) != 1) return &resp_not_addr;
  row = mysql_fetch_row(result);
  lengths = mysql_fetch_lengths(result);

  if (!str_copyb(recip, row[0], lengths[0])) return &resp_nomem;
  return qmail_recipient(recip);
}

const response* handle_data_start(void)
{
  return qmail_data_start();
}

void handle_data_bytes(const char* bytes, unsigned long len)
{
  qmail_data_bytes(bytes, len);
}

const response* handle_data_end(void)
{
  return qmail_data_end();
}

int main(void)
{
  const response* resp;

  mysql_init(&db);
  mysql_options(&db, MYSQL_READ_DEFAULT_GROUP, "smtpd-er");
  if (!mysql_real_connect(&db, 0, 0, 0, 0, 0, 0, 0)) {
    respond_resp(&resp_db_connect, 1);
    return 1;
  }

  if ((resp = qmail_validate_init()) != 0) { respond_resp(resp, 1); return 1; }
  return smtp_mainloop(0);
}
