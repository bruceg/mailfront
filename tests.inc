fixout() {
  sed -e '/^220 local.host mailfront ESMTP/d; s/\[[0-9]*\]:/[#]:/;'
}
fixtls() {
  sed -e '
    s/\[[0-9]*\]:/[#]:/;
    /INTERNAL ERROR: Uninitialized client session cache/d;
    s/^.* \(LOG[1-9]\)/\1/;
    /^LOG[1-9]\[ui\]: /d;
    s/handshake: TLS[1-9]\.[0-9]/handshake: TLS#.#/;
  '
}

smtpfront() {
  $src/mailfront smtp "$@" 2>&1 | fixout
}
sfecho() {
  $src/mailfront smtp echo "$@" 2>/dev/null | fixout
}
pfauth() {
  $src/pop3front-auth "$@" echo Yes. 2>/dev/null \
  | tail -n +2
}
ifauth() {
  $src/imapfront-auth sh -c 'echo Yes: $IMAPLOGINTAG' 2>/dev/null \
  | grep -v '^\* OK imapfront ready.'
}
pfmaildir() {
  $src/pop3front-maildir "$@" 2>/dev/null \
  | tail -n +2
}
maildir=$tmp/Maildir
maildir() {
  rm -rf $maildir
  mkdir -p $maildir/cur
  mkdir -p $maildir/new
  mkdir -p $maildir/tmp
}
tstmsg() {
  fn=$1
  {
    echo "Header: foo"
    echo
    echo "body"
  } >$maildir/$fn
}

setup_queuedir() {
    export QUEUEDIR=$tmp/queuedir
    mkdir -p $tmp/queuedir/new $tmp/queuedir/tmp
}

dump_queuedir() {
    echo queuedir tmp: $( find $QUEUEDIR/tmp -type f | wc -l ) new: $( find $QUEUEDIR/new -type f | wc -l )
    find $QUEUEDIR/new -type f | while read file
    do
	echo "===== queued file ====="
	cat -v "$file"
	echo "====="
	rm -f "$file"
    done
    rm -f $QUEUEDIR/tmp/*
}

cleanup_queuedir() {
    rm -rf $QUEUEDIR
}

setup_cert() {
    export TLS_CERTFILE=$tmp/tlscrt.pem
    export TLS_KEYFILE=$tmp/tlskey.pem

    if ! [ -s $TLS_CERTFILE ]
    then
        template=$tmp/cert.cfg
        echo 'cn = example.com' > $template
        certtool --generate-privkey --sec-param low --outfile $TLS_KEYFILE 2> /dev/null
        certtool --generate-self-signed --load-privkey $TLS_KEYFILE --template $template --outfile $TLS_CERTFILE 2> /dev/null
    fi
}

make_stunnel_conf() {
    modules="$1"
    shift
    {
        cat <<EOF
client = yes
debug = notice
exec = $src/mailfront
execArgs = mailfront $modules
foreground = yes
syslog = no
EOF
        for line in "$@"
        do
            echo "$line"
        done
    } > $tmp/stunnel.conf
}

PROTO=TEST
TESTLOCALIP=1.2.3.4
TESTREMOTEIP=5.6.7.8
TESTLOCALHOST=local.host
TESTREMOTEHOST=remote.host
CVM_PWFILE_PATH=$tmp/pwfile
MODULE_PATH=$src
PLUGINS=accept

export PROTO TESTLOCALIP TESTREMOTEIP TESTLOCALHOST TESTREMOTEHOST
export MAILRULES DATABYTES MAXHOPS PATTERNS MAXNOTIMPL
export PLUGINS MODULE_PATH
