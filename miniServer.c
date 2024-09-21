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

//소켓 nonblocking
void set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char** argv) {
    int ssock;                                                  // 서버 socket
    socklen_t clen;
    struct sockaddr_in servaddr, cliaddr;
    char mesg[BUF_SIZE];
    int yes = 1;
    pid_t pid;
    int clients[MAX_CLIENTS];
    int num_clients = 0;                                        //현재 연결된 클라이언트 수
    int pipefds[MAX_CLIENTS][2];
    char name[20];

    struct sigaction sigact;
    sigact.sa_handler = handler;                                //signal handler처리
    sigfillset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sigact, NULL);

    if ((ssock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {        //서버 소켓 생성
        perror("socket()");
        return -1;
    }
    if (setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt()");
        close(ssock);
        return -1;
    }
    set_nonblocking(ssock);                                     //서버 소켓 nonblocking모드로 설정


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
        int csock = accept(ssock, (struct sockaddr*)&cliaddr, &clen);    // 클라이언트 접속 accept

        if (csock < 0) {
            // non-blocking mode이기 때문에, 에러가 accept 실패가 아닐 수 있음
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // 더 이상 연결 대기 중인 클라이언트가 없는 경우
                // 부모 프로세스는 계속해서 클라이언트와 통신을 수행
                                // 다른 클라이언트에게 전달
                for (int i = 0; i < num_clients; i++) {
                    memset(mesg, 0, BUF_SIZE);
                    int n = read(pipefds[i][0], mesg, BUF_SIZE);  //파이프 통해 자식으로부터 읽기
                    if (n > 0) {
                        printf("Broadcasting message from client %d: %s\n", i, mesg);
                        for (int j = 0; j < num_clients; j++) {
                            if (i != j) {                    //자신이 쓴것은 다시 받지 않도록 자신 제외
                                write(clients[j], mesg, n);  // 다른 클라이언트들에게 전달
                            }
                        }
                    }
                    else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("read() from pipe");
                    }
                }
                continue;                                                 // 계속해서 다음 작업 대기
            }
            else {
                perror("accept() failed");
                continue;
            }
        }
        else {
            printf("New client connected...\n");
            set_nonblocking(csock);                         //클라이언트 소켓 nonblocking 모드 설정

            if (num_clients >= MAX_CLIENTS) {               // 접속된 클라이언트 수가 MAX_CLIENTS = 50 보다 많거나 같을 경우 클라이언트 소켓 닫음
                printf("Maximum clients.\n");
                close(csock);
                continue;
            }

            clients[num_clients] = csock;                   // 클라이언트 소켓 저장

            if (pipe(pipefds[num_clients]) == -1) {         // 연결된 클라이언트 수만큼 파이프 생성
                perror("pipe()");
                continue;
            }

            if ((pid = fork()) < 0) {                       // Fork process
                perror("fork()");
                continue;
            }

            //자식 프로세스
            if (pid == 0) {     // Child process
                close(ssock);                               // 자식은 서버 소켓을 닫음
                close(pipefds[num_clients][0]);             // 파이프의 읽기 끝 닫기

                //int getname = read(csock, name,strlen(name));

                while (1) {
                    memset(mesg, 0, BUF_SIZE);
                    int n = read(csock, mesg, BUF_SIZE);    //클라이언트 소켓 읽고
                    if (n > 0) {
                        printf("Received from client: %s\n", mesg);
                        write(pipefds[num_clients][1], mesg, n);        // 파이프에 쓰기 -> 부모에게 전달
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
                close(pipefds[num_clients][1]);  // 파이프의 쓰기 끝 닫기
                exit(0);

                //부모 프로세스
            }
            else {
                //close(csock);  
                // 부모는 클라이언트 소켓을 닫지 않음
                // 자식으로부터 받은 데이터를 파일 디스크립터를 이용하여 클라이언트 소켓에 전달할 것
                close(pipefds[num_clients][1]);  // 파이프의 쓰기 끝 닫기
                fcntl(pipefds[num_clients][0], F_SETFL, O_NONBLOCK); // 부모의 파이프를 논블로킹 모드로 설정, 다른 자식들도 동시에 사용


                num_clients++;  // 새로운 클라이언트 카운트 증가
            }
        }


    }

    close(ssock);
    return 0;
}
