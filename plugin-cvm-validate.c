#include <stdlib.h>
#include <string.h>
#include "mailfront.h"
#include <cvm/v2client.h>
#include <cvm/credentials.h>

static const char* lookup_secret;
static const char* cvm_lookup;
static int cred_count;

static RESPONSE(norcpt,553,"5.1.1 Sorry, that recipient does not exist.");
static RESPONSE(failed,451,"4.1.0 Sorry, I could not verify that recipient (internal temporary error).");

static const response* validate_init(void)
{
  if ((cvm_lookup = getenv("CVM_LOOKUP")) != 0) {

    if ((lookup_secret = getenv("CVM_LOOKUP_SECRET")) == 0)
      lookup_secret = getenv("LOOKUP_SECRET");

    /* Invoking CVMs in cvm-command mode without $CVM_LOOKUP_SECRET set
     * will fail, since the CVM will be expecting additional credentials
     * to validate.  To prevent this failure, set $CVM_LOOKUP_SECRET to
     * an empty string. */
    if (lookup_secret == 0) {
      if (putenv("CVM_LOOKUP_SECRET=") != 0)
	return &resp_oom;
      /* Also set the lookup secret to an empty string internally to
       * avoid NULL pointer issues later. */
      lookup_secret = "";
    }

    /* Match the behavior of the current CVM code base: If
     * $CVM_LOOKUP_SECRET is set to an empty string, treat it as if no
     * lookup secret is required. */
    cred_count = (*lookup_secret == 0) ? 2 : 3;
  }
  return 0;
}

static const response* validate_recipient(str* recipient, str* params)
{
  struct cvm_credential creds[3];
  unsigned i;
  unsigned long u;
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
      && str_copys(&creds[2].value, lookup_secret)) {
    switch (cvm_authenticate(cvm_lookup, cred_count, creds)) {
    case 0: r = 0; break;
    case CVME_PERMFAIL:
      /* Return a "pass" result if the CVM declared the address to be
       * out of scope. */
      r = (cvm_client_fact_uint(CVM_FACT_OUTOFSCOPE, &u) == 0
	   && u == 1)
	? 0
	: &resp_norcpt;
    }
  }
  str_free(&creds[0].value);
  str_free(&creds[1].value);
  str_free(&creds[2].value);
  return r;
  (void)params;
}

struct plugin plugin = {
  .version = PLUGIN_VERSION,
  .init = validate_init,
  .recipient = validate_recipient,
};
