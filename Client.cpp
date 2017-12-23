#include <iostream>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <zmq.h>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <cstring>
#include <sys/types.h>

termios orig_termios;
void* context;
void* pushSocket;
void* subSocket;

struct MessageData {
    char Name[80];
    char Message[256];
    char Address[80];
};

struct PassData {
    int action; // 0 - login, 1 - create.
    char Name[80];
    char Password[256];
};

struct PassCheck {
    int result; // 0 - OK, 1 - wrong pass, 2 - not found;
};

char* Login() {
    std::cout << "0 - sign in, 1 - sign up: ";
    int act;
    std::cin >> act;
    char* Name = (char*) malloc(80);
    char Password[256];
    std::cout << "Please, enter your name: ";
    std::cin >> Name;
    std::cout << "Please, enter your pass: ";
    std::cin >> Password;
    void* ctx = zmq_ctx_new();
    void* reqSocket = zmq_socket(ctx, ZMQ_REQ);
    zmq_connect(reqSocket, "tcp://localhost:4043");
    PassData pd;
    pd.action = act;
    strcpy(pd.Name, Name);
    strcpy(pd.Password, Password);
    zmq_msg_t passMsg;
    zmq_msg_init_size(&passMsg, sizeof(PassData));
    memcpy(zmq_msg_data(&passMsg), &pd, sizeof(PassData));
    zmq_msg_send(&passMsg, reqSocket, 0);
    zmq_msg_close(&passMsg);
    zmq_msg_t reply;
    zmq_msg_init(&reply);
    zmq_msg_recv(&reply, reqSocket, 0);
    PassCheck* pc = (PassCheck*) zmq_msg_data(&reply);
    zmq_msg_close(&reply);
    if(pc->result == 0) {
        if(act == 0) {
            zmq_close(reqSocket);
            zmq_ctx_destroy(ctx);
            return Name;
        } else {
            zmq_close(reqSocket);
            zmq_ctx_destroy(ctx);
            return Login();
        }
    } else if(pc->result == 1) {
        if(act == 0) {
            std::cout << "Wrong Password" << std::endl;
            zmq_close(reqSocket);
            zmq_ctx_destroy(ctx);
            return Login();
        } else {
            std::cout << "Account already exists" << std::endl;
            zmq_close(reqSocket);
            zmq_ctx_destroy(ctx);
            return Login();
        }
    } else {
        std::cout << "Account doesn't exist" << std::endl;
        zmq_close(reqSocket);
        zmq_ctx_destroy(ctx);
        return Login();
    }
}

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);
    termios raw = orig_termios;
    raw.c_iflag &= ~(IXON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void destrCtx() {
    zmq_close(subSocket);
    zmq_close(pushSocket);
    zmq_ctx_destroy(context);
}

void inpAddr(char* addr) {
    char c = getchar();
    int idx = 0;
    do {
        addr[idx++] = c;
        c = getchar();
    } while(c != ' ');
    addr[idx] = 0;
    return;
}

int main(int argc, char** argv) {
    atexit(destrCtx);
    char* Name = Login();
    std::cout << "Entered as: " << Name << std::endl;
    context = zmq_ctx_new();
    pid_t display;
    if((display = fork()) == -1) {
        std::perror("Fork failed");
        return 1;
    }
    if(display == 0) {
        subSocket = zmq_socket(context, ZMQ_SUB);
        zmq_connect(subSocket, "tcp://localhost:4042");
        zmq_setsockopt(subSocket, ZMQ_SUBSCRIBE, "gr", 2);
        zmq_setsockopt(subSocket, ZMQ_SUBSCRIBE, Name, strlen(Name));
        while(1) {
            char theme[80];
            zmq_msg_t message;
            zmq_msg_init(&message);
            int th = zmq_recv(subSocket, theme, 3, 0);
            zmq_msg_recv(&message, subSocket, 0);
            MessageData *m = (MessageData*) zmq_msg_data(&message);
            if(strcmp(m->Name, Name) != 0) {
                printf("\33[2K\r");
                if(th != 2) {
                    std::cout << "!!! ";
                }
                std::cout << m->Name << ": " << m->Message << std::endl;
                std::cout << Name << ": ";
                std::cout.flush();
            }
            zmq_msg_close(&message);
        }
    } else {
        pushSocket = zmq_socket(context, ZMQ_PUSH);
        zmq_connect(pushSocket, "tcp://localhost:4041");
        int c = 0;
        c = getchar();
        while(1) {
            enableRawMode();
            MessageData message;
            strcpy(message.Name, Name);
            char addr[] = "gr\0";
            strcpy(message.Address, addr);
            std::cout << Name << ": ";
            int counter = 0;
            int counterAdr = 0;
            int entAddr = 0;
            do {
                c = getchar();
                if(c == '/' && counter == 0 && entAddr == 0) {
                    int cNext = getchar();
                    if(cNext == 'w') {
                        cNext = getchar();
                        inpAddr(message.Address);
                    }
                } else {
                    message.Message[counter++] = c;
                }
                if(c == 17) {
                    kill(display, SIGKILL);
                    _exit(0);
                }
            } while(c != '\n');
            message.Message[counter-1] = 0;
            zmq_msg_t zmqMessage;
            zmq_msg_init_size(&zmqMessage, sizeof(message));
            memcpy(zmq_msg_data(&zmqMessage), &message, sizeof(message));
            int send = zmq_msg_send(&zmqMessage, pushSocket, 0);
            if(send == -1) {
                std::perror("Can't push message");
                return 2;
            }
            zmq_msg_close(&zmqMessage);
        }
    }
}
