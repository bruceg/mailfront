#include <stdlib.h>
#include "mailfront.h"
#include <cvm/client.h>

static const char* lookup_secret;
static const char* cvm_lookup;

static RESPONSE(norcpt,553,"Sorry, that recipient does not exist.");
static RESPONSE(failed,451,"Sorry, I could not verify that recipient (internal temporary error).");

const response* cvm_validate_init(void)
{
  cvm_lookup = getenv("CVM_LOOKUP");
  if ((lookup_secret = getenv("LOOKUP_SECRET")) == 0)
    lookup_secret = "";
  return 0;
}

const response* cvm_validate_recipient(const str* recipient)
{
  const char* credentials[2] = { lookup_secret, 0 };
  str domain = {0,0,0};
  str user = {0,0,0};
  unsigned i;
  const response* r = &resp_failed;

  if (cvm_lookup == 0) return 0;
  if ((i = str_findlast(recipient, '@')) == (unsigned)-1)
    return 0;
  if (str_copyb(&user, recipient->s, i) &&
      str_copyb(&domain, recipient->s+i+1, recipient->len-i-1)) {
    switch (cvm_authenticate(cvm_lookup, user.s, domain.s, credentials, 0)) {
    case 0: r = 0; break;
    case CVME_PERMFAIL: r = &resp_norcpt;
    }
  }
  str_free(&user);
  str_free(&domain);
  return r;
}
