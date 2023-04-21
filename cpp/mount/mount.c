#include <sys/mount.h>
#include <stdio.h>
 
int main(int argc, char *argv[]) {
        if (mount("hello", "hello1", "ext4", MS_BIND, NULL)) {
                perror("mount failed");
        }
        return 0;
}
