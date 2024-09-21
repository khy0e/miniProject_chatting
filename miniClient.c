
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <wait.h>

#define BUF_SIZE 30

// non-blocking mode
void set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char* argv[]) {
    int sock;
    struct sockaddr_in serv_addr;
    char message[BUF_SIZE];
    int str_len;
    pid_t pid;
    char name[20];

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {    //Ŭ���̾�Ʈ ���� ����
        perror("socket()");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");      // ip�ּ� ���� - ������ ��������̿��� ����� ��� ��������� �ּ� ��� �ʿ�
    serv_addr.sin_port = htons(5100);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {   // ������ ���� ��� ����
        perror("connect()");
        exit(1);
    }

    set_nonblocking(sock);

    printf("name: ");
    fgets(name, 20, stdin);        //ä�ÿ��� ����� �̸� �Է�
    name[strlen(name) - 1] = '\0';

    //if (write(sock, name, strlen(name)) == -1) {
    //    perror("write()");        
    //}

    // �ڽ� ���μ���
    if ((pid = fork()) == 0) {

        while (1) {
            memset(message, 0, BUF_SIZE);
            str_len = read(sock, message, BUF_SIZE - 1);         // �����κ��� ���� ������ �޾ƿ���
            if (str_len > 0) {
                message[str_len] = 0;
                printf("\n");
                printf("[Message from server]: %s", message);    // �����κ��� ���� ������ ���
            }
            else if (str_len == -1 && errno == EAGAIN) {
                continue;
            }
            else if (str_len == 0) {
                printf("Server disconnected.\n");
                break;
            }
        }
        //�θ� ���μ���
    }
    else {
        while (1) {
            printf("%s: ", name);
            fgets(message, BUF_SIZE, stdin);                     // �ٸ� Ŭ���̾�Ʈ���� ������ �޽��� �Է�
            if (!strcmp(message, "q\n") || !strcmp(message, "Q\n")) {
                close(sock);
                exit(0);
            }

            if (write(sock, message, strlen(message)) == -1) {   // �Է¹��� �޽��� ���� ���Ͽ� ����(������ �ڽ� ���μ�������)
                perror("write()");
                break;
            }
        }
    }

    wait(NULL);
    close(sock);
    return 0;
}
