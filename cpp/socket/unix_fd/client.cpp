#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define BUFFER_SIZE 100
#define OPEN_FILE "file_fd"
const char file_server_socket[] = "msghdr_tmp";

int main(int argc, char *argv[])
{
    int fd, cli_fd;
    struct sockaddr_un server_addr;
    struct msghdr socket_msg;
    struct cmsghdr *ctrl_msg;
    struct iovec iov[1];
    int ret;
    char buf[BUFFER_SIZE];
    char buf2[BUFFER_SIZE];
    char ctrl_data[CMSG_SPACE(sizeof(cli_fd))];

    char *test_msg = (char *)"client send msg.\n";

    /* create socket */
    cli_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (cli_fd < 0)
    {
        printf("client: create socket failed!\n");
        return -1;
    }

    fd = open(OPEN_FILE, O_CREAT | O_RDWR, 0777);
    if (fd < 0)
    {
        printf("open file_fd failed!\n");
        return -1;
    }

    /* initilize sockaddr_un */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, file_server_socket);

    /* connect uds server */
    ret = connect(cli_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0)
    {
        printf("client: connect to server failed!\n");
        return -1;
    }

    int write_len = write(cli_fd, test_msg, strlen(test_msg) + 1);

    if (write_len)
    {
        printf("client write success!\n");
    }

    // ret = read(cli_fd, buf2, BUFFER_SIZE);
    // if (ret < 0)
    // {
    //     printf("client: read failed!\n");
    //     return ret;
    // }
    // printf("client read: %s\n", buf2);

    socket_msg.msg_name = NULL;
    socket_msg.msg_namelen = 0;
    iov[0].iov_base = buf;
    iov[0].iov_len = sizeof(buf);
    socket_msg.msg_iov = iov;
    socket_msg.msg_iovlen = 1;
    socket_msg.msg_control = ctrl_data;
    socket_msg.msg_controllen = sizeof(ctrl_data);

    ctrl_msg = CMSG_FIRSTHDR(&socket_msg);
    ctrl_msg->cmsg_len = CMSG_LEN(sizeof(cli_fd));
    ctrl_msg->cmsg_level = SOL_SOCKET;
    ctrl_msg->cmsg_type = SCM_RIGHTS;

    *((int *)CMSG_DATA(ctrl_msg)) = fd;

    ret = sendmsg(cli_fd, &socket_msg, 0);
    if (ret < 0)
    {
        printf("client: sendmsg failed!\n");
        return ret;
    }
    printf("client: ret = %d.\n", ret);

    // ret = read(cli_fd, buf2, BUFFER_SIZE);
    // if (ret < 0)
    // {
    //     printf("client: read failed!\n");
    //     return ret;
    // }
    // printf("client read: %s\n", buf2);

    return 0;
}