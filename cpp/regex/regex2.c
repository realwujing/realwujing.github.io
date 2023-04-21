#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))

static const char *const str = \
"node=uos type=PATH msg=audit(1680069812.071:2174): [file] pid=7270 name=rm unlink file  inode=8134260  filename=\"/home/uos/Desktop/fileaudit/d.txt\"\n \
node=uos type=SYSCALL msg=audit(1680069812.071:2174): arch=c000003e syscall=263 success=yes exit=0 a0=ffffff9c a1=bb44a0 a2=0 a3=\", 'f' <repeats 13 times>, \"bbb items=2 ppid=4956 pid=7270 auid=1000 uid=1000 gid=1000 euid=1000 suid=1000 fsuid=1000 egid=1000 sgid=1000 fsgid=1000 tty=pts2 ses=2 comm=\"rm\" exe=\"/usr/bin/rm\" subj=root:sysadm_r:sysadm_t:s0 key=\"file_wa_audit\"\n \
node=uos type=CWD msg=audit(1680069812.071:2174): cwd=\"/home/uos/Desktop/fileaudit\"\n \
node=uos type=PATH msg=audit(1680069812.071:2174): item=0 name=\"/home/uos/Desktop/fileaudit\" inode=8126535 dev=103:07 mode=040755 ouid=1000 ogid=1000 rdev=00:00 obj=root:object_r:user_home_t:s0 nametype=PARENT cap_fp=\", '0' <repeats 16 times>, \" cap_fi=\", '0' <repeats 16 times>, \" cap_fe=0 cap_fver=0\n \
node=uos type=PATH msg=audit(1680069812.071:2174): item=1 name=\"d.txt\" inode=8134260 dev=103:07 mode=0100644 ouid=1000 ogid=1000 rdev=00:00 obj=root:object_r:user_home_t:s0 nametype=DELETE cap_fp=\", '0' <repeats 16 times>, \" cap_fi=\", '0' <repeats 16 times>, \" cap_fe=0 cap_fver=0\n \
node=uos type=PROCTITLE msg=audit(1680069812.071:2174): proctitle=726D002D726600642E74787400652E747874\n \
node=uos type=EOE msg=audit(1680069812.071:2174): \n \
node=uos type=PATH msg=audit(1680069812.071:2175): [file] pid=7270 name=rm unlink file  inode=8135002  filename=\"/home/uos/Desktop/fileaudit/e.txt\"\n \
node=uos type=SYSCALL msg=audit(1680069812.071:2175): arch=c000003e syscall=263 success=yes exit=0 a0=ffffff9c a1=bb44a0 a2=0 a3=100 items=2 ppid=4956 pid=7270 auid=1000 uid=1000 gid=1000 euid=1000 suid=1000 fsuid=1000 egid=1000 sgid=1000 fsgid=1000 tty=pts2 ses=2 comm=\"rm\" exe=\"/usr/bin/rm\" subj=root:sysadm_r:sysadm_t:s0 key=\"file_wa_audit\"\n \
node=uos type=CWD msg=audit(1680069812.071:2175): cwd=\"/home/uos/Desktop/fileaudit\"\n \
node=uos type=PATH msg=audit(1680069812.071:2175): item=0 name=\"/home/uos/Desktop/fileaudit\" inode=8126535 dev=103:07 mode=040755 ouid=1000 ogid=1000 rdev=00:00 obj=root:object_r:user_home_t:s0 nametype=PARENT cap_fp=\", '0' <repeats 16 times>, \" cap_fi=\", '0' <repeats 16 times>, \" cap_fe=0 cap_fver=0\n \
node=uos type=PATH msg=audit(1680069812.071:2175): item=1 name=\"e.txt\" inode=8135002 dev=103:07 mode=0100644 ouid=1000 ogid=1000 rdev=00:00 obj=root:object_r:user_home_t:s0 nametype=DELETE cap_fp=\", '0' <repeats 16 times>, \" cap_fi=\", '0' <repeats 16 times>, \" cap_fe=0 cap_fver=0\n \
node=uos type=PROCTITLE msg=audit(1680069812.071:2175): proctitle=726D002D726600642E74787400652E747874\n \
node=uos type=EOE msg=audit(1680069812.071:2175): \n \
node=uos type=SERVICE_START msg=audit(1680069818.435:2176): pid=1 uid=0 auid=4294967295 ses=4294967295 subj=system_u:system_r:init_t:s0 msg='unit=lmt-poll comm=\"systemd\" exe=\"/usr/lib/systemd/systemd\" hostname=? addr=? terminal=? res=success\n \
node=uos type=SERVICE_STOP msg=audit(1680069818.435:2177): pid=1 uid=0 auid=4294967295 ses=4294967295 subj=system_u:system_r:init_t:s0 msg='unit=lmt-poll comm=\"systemd\" exe=\"/usr/lib/systemd/systemd\" hostname=? addr=? terminal=? res=success\n \
";

static const char *const re = "[^ ].*\n.*\n.*\n.*\n.*\n.*\n.*type=EOE msg=audit.*\n";

int main(void)
{
    static const char *s = str;
    regex_t     regex;
    regmatch_t  pmatch[1];
    regoff_t    off, len;

    if (regcomp(&regex, re, REG_EXTENDED | REG_NEWLINE))
        exit(EXIT_FAILURE);

    printf("re = \"%s\"\n\n\n", re);
    printf("String = \"%s\"\n\n\n", str);
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
