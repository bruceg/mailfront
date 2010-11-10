#include <sysdeps.h>
#include <unistd.h>

#include <cvm/module.h>
#include "auto_testcvm.c"

const char program[] = "testcvm";

int cvm_module_init(void)
{
  return 0;
}

int cvm_module_lookup(void)
{
  if (cvm_module_credentials[CVM_CRED_ACCOUNT].s == 0)
    return CVME_NOCRED;
  if (str_diffs(&cvm_module_credentials[CVM_CRED_ACCOUNT], "testuser"))
    return CVME_PERMFAIL;
  return 0;
}

int cvm_module_authenticate(void)
{
  CVM_CRED_REQUIRED(PASSWORD);
  if (str_diffs(&cvm_module_credentials[CVM_CRED_PASSWORD], "testpass"))
    return CVME_PERMFAIL;
  return 0;
}

int cvm_module_results(void)
{
  cvm_fact_username = "testuser";
  cvm_fact_userid = TESTCVM_UID;
  cvm_fact_groupid = TESTCVM_GID;
  cvm_fact_realname = "Test User";
  cvm_fact_directory = TESTCVM_PWD "/tests-tmp";
  cvm_fact_shell = "/bin/false";
  cvm_fact_mailbox = TESTCVM_PWD "/tests-tmp/mail:box";
  cvm_fact_groupname = 0;
  return 0;
}

void cvm_module_stop(void)
{
}
