sfqmail() { $src/smtpfront-qmail 2>/dev/null | tail -n +2; }
sfecho() { $src/smtpfront-echo 2>/dev/null | tail -n +2; }
pfauth() { $src/pop3front-auth "$@" echo Yes. 2>/dev/null | tail -n +2; }
ifauth() { $src/imapfront-auth sh -c 'echo Yes: $IMAPLOGINTAG' 2>/dev/null | tail -n +2; }
pfmaildir() { $src/pop3front-maildir "$@" 2>/dev/null | tail -n +2; }
maildir=$tmp/Maildir
maildir() {
  rm -rf $maildir
  mkdir -p $maildir/{cur,new,tmp}
}
tstmsg() {
  fn=$1
  {
    echo "Header: foo"
    echo
    echo "body"
  } >$maildir/$fn
}

PROTO=TEST
TESTLOCALIP=1.2.3.4
TESTREMOTEIP=5.6.7.8
TESTLOCALHOST=local.host
TESTREMOTEHOST=remote.host

export PROTO TESTLOCALIP TESTREMOTEIP TESTLOCALHOST TESTREMOTEHOST

CVM_PWFILE_PATH=$tmp/pwfile
export CVM_PWFILE_PATH
echo testuser:testpass:$_UID:$_GID:Test User:$tmp:/bin/false >$CVM_PWFILE_PATH
ln -s `which cvm-pwfile` $tmp/cvm
