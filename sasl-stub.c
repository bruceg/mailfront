#include <cvm/sasl.h>
#include <str/str.h>

int sasl_auth_init(struct sasl_auth* sa)
{
  return 1;
  (void)sa;
}

int sasl_auth_caps(str* resp)
{
  return resp->s != 0;
}

int sasl_auth1(struct sasl_auth* sa, const str* ignored2)
{
  return -1;
  (void)sa;
  (void)ignored2;
}

int sasl_auth2(struct sasl_auth* sa,
	       const char* ignored2, const char* ignored3)
{
  return -1;
  (void)sa;
  (void)ignored2;
  (void)ignored3;
}

const char* sasl_auth_msg(int* code)
{
  *code = 500;
  return "Not implemented.";
}
