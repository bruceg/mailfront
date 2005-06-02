#include <stdlib.h>
#include <string.h>
#include "mailfront.h"
#include <cvm/v2client.h>
#include <cvm/credentials.h>

static const char* lookup_secret;
static const char* cvm_lookup;

static RESPONSE(norcpt,553,"Sorry, that recipient does not exist.");
static RESPONSE(failed,451,"Sorry, I could not verify that recipient (internal temporary error).");

const response* cvm_validate_init(void)
{
  cvm_lookup = getenv("CVM_LOOKUP");
  if ((lookup_secret = getenv("CVM_LOOKUP_SECRET")) == 0)
    lookup_secret = getenv("LOOKUP_SECRET");
  /* Match the behavior of the current CVM code base: If
   * $CVM_LOOKUP_SECRET is set to an empty string, treat it as if no
   * lookup secret is required.  (If $CVM_LOOKUP_SECRET is unset, the
   * module will not operate in lookup mode).
   */
  if (lookup_secret != 0 && *lookup_secret == 0)
    lookup_secret = 0;
  return 0;
}

const response* cvm_validate_recipient(const str* recipient)
{
  struct cvm_credential creds[3];
  unsigned i;
  const response* r = &resp_failed;

  if (cvm_lookup == 0) return 0;
  if ((i = str_findlast(recipient, '@')) == (unsigned)-1)
    return 0;
  memset(creds, 0, sizeof creds);
  creds[0].type = CVM_CRED_ACCOUNT;
  creds[1].type = CVM_CRED_DOMAIN;
  creds[2].type = CVM_CRED_SECRET;
  if (str_copyb(&creds[0].value, recipient->s, i)
      && str_copyb(&creds[1].value, recipient->s+i+1, recipient->len-i-1)
      && str_copys(&creds[2].value, cvm_lookup)) {
    switch (cvm_authenticate(cvm_lookup, 3, creds)) {
    case 0: r = 0; break;
    case CVME_PERMFAIL: r = &resp_norcpt;
    }
  }
  str_free(&creds[0].value);
  str_free(&creds[1].value);
  str_free(&creds[2].value);
  return r;
}
