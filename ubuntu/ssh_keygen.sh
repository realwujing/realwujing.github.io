#!/usr/bin/expect
#set enter "\n"
spawn ssh-keygen
expect {
        "*(/root/.ssh/id_rsa)" {send "\n\r";exp_continue}
        "*(empty for no passphrase)" {send "\n\r";exp_continue}
        "*again" {send "\n\r"}
}
expect eof