exec "$(dirname $0)"/mailfront qmtp qmail check-fqdn counters mailrules relayclient cvm-validate qmail-validate add-received patterns "$@"
