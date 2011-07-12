exec "$(dirname $0)"/mailfront qmqp qmail check-fqdn counters mailrules relayclient cvm-validate qmail-validate add-received patterns "$@"
