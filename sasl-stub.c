#include "sasl-auth.h"
#include "cvm-sasl/cvm-sasl.h"

int sasl_auth_init(void)
{
  return 1;
}

int sasl_auth_cap(str* ignored)
{
  return 1;
}

int sasl_auth(const char* ignored1, const str* ignored2)
{
  return -1;
}

const char* sasl_auth_msg(int* code)
{
  *code = 500;
  return "Not implemented.";
}
