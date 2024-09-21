
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

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {    //클라이언트 소켓 생성
        perror("socket()");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");      // ip주소 설정 - 서버가 라즈베리파이에서 실행될 경우 라즈베리파이 주소 사용 필요
    serv_addr.sin_port = htons(5100);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {   // 서버와 소켓 통신 연결
        perror("connect()");
        exit(1);
    }

    set_nonblocking(sock);

    printf("name: ");
    fgets(name, 20, stdin);        //채팅에서 사용할 이름 입력
    name[strlen(name) - 1] = '\0';

    //if (write(sock, name, strlen(name)) == -1) {
    //    perror("write()");        
    //}

    // 자식 프로세스
    if ((pid = fork()) == 0) {

        while (1) {
            memset(message, 0, BUF_SIZE);
            str_len = read(sock, message, BUF_SIZE - 1);         // 서버로부터 받은 데이터 받아오기
            if (str_len > 0) {
                message[str_len] = 0;
                printf("\n");
                printf("[Message from server]: %s", message);    // 서버로부터 받은 데이터 출력
            }
            else if (str_len == -1 && errno == EAGAIN) {
                continue;
            }
            else if (str_len == 0) {
                printf("Server disconnected.\n");
                break;
            }
        }
        //부모 프로세스
    }
    else {
        while (1) {
            printf("%s: ", name);
            fgets(message, BUF_SIZE, stdin);                     // 다른 클라이언트에게 전달할 메시지 입력
            if (!strcmp(message, "q\n") || !strcmp(message, "Q\n")) {
                close(sock);
                exit(0);
            }

            if (write(sock, message, strlen(message)) == -1) {   // 입력받은 메시지 서버 소켓에 전달(서버의 자식 프로세스에게)
                perror("write()");
                break;
            }
        }
    }

    wait(NULL);
    close(sock);
    return 0;
}
