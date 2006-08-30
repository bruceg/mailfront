exec "$(dirname $0)"/mailfront smtp qmail check-fqdn counters mailrules relayclient cvm-validate qmail-validate add-received patterns accept-sender "$@"
