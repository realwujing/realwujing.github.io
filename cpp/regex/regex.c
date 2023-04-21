#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))

static const char *const str = "type=PROCTITLE msg=audit(2023年03月28日 10:43:28.890:578) : proctitle=touch a.txt b.txt c.txt\n \
type=PATH msg=audit(2023年03月28日 10:43:28.890:578) : item=1 name=\"a.txt\" inode=8132017 dev=103:07 mode=file,644 ouid=uos ogid=uos rdev=00:00 obj=root:object_r:user_home_t:s0 nametype=CREATE cap_fp=none cap_fi=none cap_fe=0 cap_fver=0\n \
type=PATH msg=audit(2023年03月28日 10:43:28.890:578) : item=0 name=\"/home/uos/Desktop/fileaudit\" inode=8126535 dev=103:07 mode=dir,755 ouid=uos ogid=uos rdev=00:00 obj=root:object_r:user_home_t:s0 nametype=PARENT cap_fp=none cap_fi=none cap_fe=0 cap_fver=0\n \
type=CWD msg=audit(2023年03月28日 10:43:28.890:578) : cwd=\"/home/uos/Desktop/fileaudit\"\n \
type=SYSCALL msg=audit(2023年03月28日 10:43:28.890:578) : arch=x86_64 syscall=openat success=yes exit=3 a0=0xffffff9c a1=0x7ffcca60d3f5 a2=O_WRONLY|O_CREAT|O_NOCTTY|O_NONBLOCK a3=0x1b6 items=2 ppid=8314 pid=9375 auid=uos uid=uos gid=uos euid=uos suid=uos fsuid=uos egid=uos sgid=uos fsgid=uos tty=pts6 ses=2 comm=touch exe=\"/usr/bin/touch\" subj=root:sysadm_r:sysadm_t:s0 key=file_wa_audit\n \
type=PROCTITLE msg=audit(2023年03月28日 10:43:28.890:579) : proctitle=touch a.txt b.txt c.txt\n \
type=PATH msg=audit(2023年03月28日 10:43:28.890:579) : item=1 name=\"b.txt\" inode=8134862 dev=103:07 mode=file,644 ouid=uos ogid=uos rdev=00:00 obj=root:object_r:user_home_t:s0 nametype=CREATE cap_fp=none cap_fi=none cap_fe=0 cap_fver=0\n \
type=PATH msg=audit(2023年03月28日 10:43:28.890:579) : item=0 name=/home/uos/Desktop/fileaudit inode=8126535 dev=103:07 mode=dir,755 ouid=uos ogid=uos rdev=00:00 obj=root:object_r:user_home_t:s0 nametype=PARENT cap_fp=none cap_fi=none cap_fe=0 cap_fver=0\n \
type=CWD msg=audit(2023年03月28日 10:43:28.890:579) : cwd=\"/home/uos/Desktop/fileaudit\"\n \
type=SYSCALL msg=audit(2023年03月28日 10:43:28.890:579) : arch=x86_64 syscall=openat success=yes exit=0 a0=0xffffff9c a1=0x7ffcca60d3fb a2=O_WRONLY|O_CREAT|O_NOCTTY|O_NONBLOCK a3=0x1b6 items=2 ppid=8314 pid=9375 auid=uos uid=uos gid=uos euid=uos suid=uos fsuid=uos egid=uos sgid=uos fsgid=uos tty=pts6 ses=2 comm=touch exe=\"/usr/bin/touch\" subj=root:sysadm_r:sysadm_t:s0 key=file_wa_audit\n \
type=PROCTITLE msg=audit(2023年03月28日 10:43:28.890:580) : proctitle=touch a.txt b.txt c.txt\n \
type=PATH msg=audit(2023年03月28日 10:43:28.890:580) : item=1 name=\"c.txt\" inode=8134949 dev=103:07 mode=file,644 ouid=uos ogid=uos rdev=00:00 obj=root:object_r:user_home_t:s0 nametype=CREATE cap_fp=none cap_fi=none cap_fe=0 cap_fver=0\n \
type=PATH msg=audit(2023年03月28日 10:43:28.890:580) : item=0 name=\"/home/uos/Desktop/fileaudit\" inode=8126535 dev=103:07 mode=dir,755 ouid=uos ogid=uos rdev=00:00 obj=root:object_r:user_home_t:s0 nametype=PARENT cap_fp=none cap_fi=none cap_fe=0 cap_fver=0\n \
type=CWD msg=audit(2023年03月28日 10:43:28.890:580) : cwd=\"/home/uos/Desktop/fileaudit\"\n \
type=SYSCALL msg=audit(2023年03月28日 10:43:28.890:580) : arch=x86_64 syscall=openat success=yes exit=0 a0=0xffffff9c a1=0x7ffcca60d401 a2=O_WRONLY|O_CREAT|O_NOCTTY|O_NONBLOCK a3=0x1b6 items=2 ppid=8314 pid=9375 auid=uos uid=uos gid=uos euid=uos suid=uos fsuid=uos egid=uos sgid=uos fsgid=uos tty=pts6 ses=2 comm=touch exe=\"/usr/bin/touch\" subj=root:sysadm_r:sysadm_t:s0 key=file_wa_audit\n \
";
static const char *const re = "type=.*\n\\s*.*\n\\s*.*\n\\s*.*\n\\s*.*key=file_wa_audit";

int main(void)
{
    static const char *s = str;
    regex_t     regex;
    regmatch_t  pmatch[1];
    regoff_t    off, len;

    if (regcomp(&regex, re, REG_EXTENDED | REG_NEWLINE))
        exit(EXIT_FAILURE);

    printf("String = \"%s\"\n", str);
    printf("Matches:\n");

    for (int i = 0; ; i++) {
        if (regexec(&regex, s, ARRAY_SIZE(pmatch), pmatch, 0))
            break;

        off = pmatch[0].rm_so + (s - str);
        len = pmatch[0].rm_eo - pmatch[0].rm_so;
        printf("#%d:\n", i);
        printf("offset = %jd; length = %jd\n", (intmax_t) off,
                (intmax_t) len);
        printf("substring = \"%.*s\"\n", len, s + pmatch[0].rm_so);

        s += pmatch[0].rm_eo;
    }

    exit(EXIT_SUCCESS);
}
