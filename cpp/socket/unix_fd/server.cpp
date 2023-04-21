#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define BUFFER_SIZE 100
const char file_server_socket[] = "msghdr_tmp";

int main(int argc, char *argv[])
{
    int listen_fd, cli_fd, rev_fd;
    struct sockaddr_un server_addr, client_addr;
    struct msghdr socket_msg;
    struct cmsghdr *ctrl_msg;
    struct iovec iov[1];
    socklen_t cli_len;
    int ret;
    char buf[BUFFER_SIZE];
    char buf2[BUFFER_SIZE];
    char *test_msg = (char *)"server send msg.\n";
    char ctrl_data[CMSG_SPACE(sizeof(listen_fd))];

    /* remove socket file fisrtly */
    remove(file_server_socket);

    /* create socket */
    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        printf("server: create socket failed!\n");
        return -1;
    }

    /* initilize sockaddr_un */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, file_server_socket);

    /* bind the socket and addr */
    ret = bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0)
    {
        printf("server: bind socket failed! errno=%d.\n", errno);
        close(listen_fd);
        return -1;
    }

    /* listen the socket connect */
    ret = listen(listen_fd, 5);
    if (ret < 0)
    {
        printf("server: listen socket failed! errno=%d.\n", errno);
        close(listen_fd);
        return -1;
    }
    printf("server: waiting for uds client to connect...\n");

    while (1)
    {
        /* accept to connect the client */
        cli_len = sizeof(client_addr);
        cli_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &cli_len);
        if (cli_fd < 0)
        {
            printf("server: accept client failed!\n");
            continue;
        }

        sleep(1);

        int read_len = read(cli_fd, buf2, 18);

        if (read_len)
        {
            printf("server read: %s %d\n", buf2, read_len);

            // int writelen = write(cli_fd, test_msg, strlen(test_msg) + 1);
            // if (writelen)
            // {
            //     printf("server write success!\n");
            // }
        }

        socket_msg.msg_name = NULL;
        socket_msg.msg_namelen = 0;
        iov[0].iov_base = buf;
        iov[0].iov_len = sizeof(buf);
        socket_msg.msg_iov = iov;
        socket_msg.msg_iovlen = 1;
        socket_msg.msg_control = ctrl_data;
        socket_msg.msg_controllen = sizeof(ctrl_data);

        ret = recvmsg(cli_fd, &socket_msg, 0);
        if (ret < 0)
        {
            printf("server: recvmsg failed!\n");
            return ret;
        }

        ctrl_msg = CMSG_FIRSTHDR(&socket_msg);
        if (ctrl_msg != NULL && ctrl_msg->cmsg_len == CMSG_LEN(sizeof(listen_fd)))
        {
            if (ctrl_msg->cmsg_level != SOL_SOCKET)
            {
                printf("cmsg_level is not SOL_SOCKET\n");
                continue;
            }
            if (ctrl_msg->cmsg_type != SCM_RIGHTS)
            {
                printf("cmsg_type is not SCM_RIGHTS");
                continue;
            }

            rev_fd = *((int *)CMSG_DATA(ctrl_msg));
            printf("result: recv fd = %d\n", rev_fd);

            write(rev_fd, test_msg, strlen(test_msg) + 1);

            // int writelen = write(cli_fd, test_msg, strlen(test_msg) + 1);
            // if (writelen)
            // {
            //     printf("server write success!\n");
            // }
        }
    }

    return 0;
}