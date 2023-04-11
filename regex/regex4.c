#include <stdio.h>
#include <stdlib.h>
#include <regex.h>

int main()
{
    char *str = \
"node=uos type=PATH msg=audit(1680069812.071:2174): [file] pid=7270 name=rm unlink file  inode=8134260  filename=\"/home/uos/Desktop/fileaudit/d.txt\"\n \
node=uos type=SYSCALL msg=audit(1680069812.071:2174): arch=c000003e syscall=263 success=yes exit=0 a0=ffffff9c a1=bb44a0 a2=0 a3=\", 'f' <repeats 13 times>, \"bbb items=2 ppid=4956 pid=7270 auid=1000 uid=1000 gid=1000 euid=1000 suid=1000 fsuid=1000 egid=1000 sgid=1000 fsgid=1000 tty=pts2 ses=2 comm=\"rm\" exe=\"/usr/bin/rm\" subj=root:sysadm_r:sysadm_t:s0 key=\"file_wa_audit\"\n \
node=uos type=CWD msg=audit(1680069812.071:2174): cwd=\"/home/uos/Desktop/fileaudit\"\n \
node=uos type=PATH msg=audit(1680069812.071:2174): item=0 name=\"/home/uos/Desktop/fileaudit\" inode=8126535 dev=103:07 mode=040755 ouid=1000 ogid=1000 rdev=00:00 obj=root:object_r:user_home_t:s0 nametype=PARENT cap_fp=\", '0' <repeats 16 times>, \" cap_fi=\", '0' <repeats 16 times>, \" cap_fe=0 cap_fver=0\n \
node=uos type=PATH msg=audit(1680069812.071:2174): item=1 name=\"d.txt\" inode=8134260 dev=103:07 mode=0100644 ouid=1000 ogid=1000 rdev=00:00 obj=root:object_r:user_home_t:s0 nametype=DELETE cap_fp=\", '0' <repeats 16 times>, \" cap_fi=\", '0' <repeats 16 times>, \" cap_fe=0 cap_fver=0\n \
node=uos type=PROCTITLE msg=audit(1680069812.071:2174): proctitle=726D002D726600642E74787400652E747874\n \
node=uos type=EOE msg=audit(1680069812.071:2174): \n \
";
    char *pattern = "item=0 name=\"?([^\n]+)\"? inode=[^\n]+nametype=([A-Z]+) [^\n]+";

    regex_t regex;
    regmatch_t match[3];

    if (regcomp(&regex, pattern, REG_EXTENDED | REG_NEWLINE) != 0) {
        fprintf(stderr, "Could not compile regex\n");
        exit(1);
    }

    if (regexec(&regex, str, 3, match, 0) == 0) {
        char *substring;
        int i;

        for (i = 0; i < 3; i++) {
            if (match[i].rm_so == -1) {
                break;
            }

            substring = str + match[i].rm_so;
            int substring_length = match[i].rm_eo - match[i].rm_so;
            printf("Match %d: %.*s\n", i, substring_length, substring);
        }
    }

    regfree(&regex);

    return 0;
}
