#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define NOT_OK_EXIT(code, msg); {if(code == -1){perror(msg); exit(-1);} }

static void usage(const char *pname)
{
    char usage[] = "Usage: %s [optins]\n"
                   "Options are:\n"
                   "    -i   unshare IPC namespace\n"
                   "    -m   unshare mount namespace\n"
                   "    -n   unshare network namespace\n"
                   "    -p   unshare PID namespace\n"
                   "    -u   unshare UTS namespace\n"
                   "    -U   unshare user namespace\n";
    printf(usage, pname);
    exit(0);
}

int main(int argc, char *argv[])
{
    int flags = 0, opt, ret;

    //解析命令行参数，用来决定退出哪个类型的namespace
    while ((opt = getopt(argc, argv, "imnpuUh")) != -1) {
        switch (opt) {
            case 'i': flags |= CLONE_NEWIPC;        break;
            case 'm': flags |= CLONE_NEWNS;         break;
            case 'n': flags |= CLONE_NEWNET;        break;
            case 'p': flags |= CLONE_NEWPID;        break;
            case 'u': flags |= CLONE_NEWUTS;        break;
            case 'U': flags |= CLONE_NEWUSER;       break;
            case 'h': usage(argv[0]);               break;
            default:  usage(argv[0]);
        }
    }

    if (flags == 0) {
        usage(argv[0]);
    }

    //执行完unshare函数后，当前进程就会退出当前的一个或多个类型的namespace,
    //然后进入到一个或多个新创建的不同类型的namespace
    ret = unshare(flags);
    NOT_OK_EXIT(ret, "unshare");

    //用一个新的bash来替换掉当前子进程
    execlp("bash", "bash", (char *) NULL);

    return 0;
}