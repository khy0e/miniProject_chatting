#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define TCP_PORT 5100
#define MAX_CLIENTS 50
#define BUF_SIZE 30

// Signal handler
void handler(int signo) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

//���� nonblocking
void set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char** argv) {
    int ssock;                                                  // ���� socket
    socklen_t clen;
    struct sockaddr_in servaddr, cliaddr;
    char mesg[BUF_SIZE];
    int yes = 1;
    pid_t pid;
    int clients[MAX_CLIENTS];
    int num_clients = 0;                                        //���� ����� Ŭ���̾�Ʈ ��
    int pipefds[MAX_CLIENTS][2];
    char name[20];

    struct sigaction sigact;
    sigact.sa_handler = handler;                                //signal handleró��
    sigfillset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sigact, NULL);

    if ((ssock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {        //���� ���� ����
        perror("socket()");
        return -1;
    }
    if (setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt()");
        close(ssock);
        return -1;
    }
    set_nonblocking(ssock);                                     //���� ���� nonblocking���� ����


    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(TCP_PORT);

    if (bind(ssock, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind()");
        close(ssock);
        return -1;
    }

    if (listen(ssock, MAX_CLIENTS) < 0) {
        perror("listen()");
        close(ssock);
        return -1;
    }

    while (1) {
        clen = sizeof(cliaddr);
        int csock = accept(ssock, (struct sockaddr*)&cliaddr, &clen);    // Ŭ���̾�Ʈ ���� accept

        if (csock < 0) {
            // non-blocking mode�̱� ������, ������ accept ���а� �ƴ� �� ����
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // �� �̻� ���� ��� ���� Ŭ���̾�Ʈ�� ���� ���
                // �θ� ���μ����� ����ؼ� Ŭ���̾�Ʈ�� ����� ����
                                // �ٸ� Ŭ���̾�Ʈ���� ����
                for (int i = 0; i < num_clients; i++) {
                    memset(mesg, 0, BUF_SIZE);
                    int n = read(pipefds[i][0], mesg, BUF_SIZE);  //������ ���� �ڽ����κ��� �б�
                    if (n > 0) {
                        printf("Broadcasting message from client %d: %s\n", i, mesg);
                        for (int j = 0; j < num_clients; j++) {
                            if (i != j) {                    //�ڽ��� ������ �ٽ� ���� �ʵ��� �ڽ� ����
                                write(clients[j], mesg, n);  // �ٸ� Ŭ���̾�Ʈ�鿡�� ����
                            }
                        }
                    }
                    else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("read() from pipe");
                    }
                }
                continue;                                                 // ����ؼ� ���� �۾� ���
            }
            else {
                perror("accept() failed");
                continue;
            }
        }
        else {
            printf("New client connected...\n");
            set_nonblocking(csock);                         //Ŭ���̾�Ʈ ���� nonblocking ��� ����

            if (num_clients >= MAX_CLIENTS) {               // ���ӵ� Ŭ���̾�Ʈ ���� MAX_CLIENTS = 50 ���� ���ų� ���� ��� Ŭ���̾�Ʈ ���� ����
                printf("Maximum clients.\n");
                close(csock);
                continue;
            }

            clients[num_clients] = csock;                   // Ŭ���̾�Ʈ ���� ����

            if (pipe(pipefds[num_clients]) == -1) {         // ����� Ŭ���̾�Ʈ ����ŭ ������ ����
                perror("pipe()");
                continue;
            }

            if ((pid = fork()) < 0) {                       // Fork process
                perror("fork()");
                continue;
            }

            //�ڽ� ���μ���
            if (pid == 0) {     // Child process
                close(ssock);                               // �ڽ��� ���� ������ ����
                close(pipefds[num_clients][0]);             // �������� �б� �� �ݱ�

                //int getname = read(csock, name,strlen(name));

                while (1) {
                    memset(mesg, 0, BUF_SIZE);
                    int n = read(csock, mesg, BUF_SIZE);    //Ŭ���̾�Ʈ ���� �а�
                    if (n > 0) {
                        printf("Received from client: %s\n", mesg);
                        write(pipefds[num_clients][1], mesg, n);        // �������� ���� -> �θ𿡰� ����
                    }
                    else if (n == 0) {
                        printf("Client disconnected.\n");
                        close(csock);
                        break;
                    }
                    else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("read()");
                        break;
                    }
                }
                close(pipefds[num_clients][1]);  // �������� ���� �� �ݱ�
                exit(0);

                //�θ� ���μ���
            }
            else {
                //close(csock);  
                // �θ�� Ŭ���̾�Ʈ ������ ���� ����
                // �ڽ����κ��� ���� �����͸� ���� ��ũ���͸� �̿��Ͽ� Ŭ���̾�Ʈ ���Ͽ� ������ ��
                close(pipefds[num_clients][1]);  // �������� ���� �� �ݱ�
                fcntl(pipefds[num_clients][0], F_SETFL, O_NONBLOCK); // �θ��� �������� ����ŷ ���� ����, �ٸ� �ڽĵ鵵 ���ÿ� ���


                num_clients++;  // ���ο� Ŭ���̾�Ʈ ī��Ʈ ����
            }
        }


    }

    close(ssock);
    return 0;
}
