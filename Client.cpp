#include <iostream>
#include <unistd.h>
#include <zmq.h>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <cstring>
#include <sys/types.h>

struct MessageData {
    char Name[80];
    char Message[256];
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

int main(int argc, char** argv) {
    char* Name = Login();
    void* context = zmq_ctx_new();
    pid_t display;
    if((display = fork()) == -1) {
        std::perror("Fork failed");
        return 1;
    }
    if(display == 0) {
        void* subSocket = zmq_socket(context, ZMQ_SUB);
        zmq_connect(subSocket, "tcp://localhost:4042");
        zmq_setsockopt(subSocket, ZMQ_SUBSCRIBE, "", 0);
        while(1) {
            zmq_msg_t message;
            zmq_msg_init(&message);
            zmq_msg_recv(&message, subSocket, 0);
            MessageData *m = (MessageData*) zmq_msg_data(&message);
            std::cout << m->Name << ": " << m->Message << std::endl;
            zmq_msg_close(&message);
        }
    } else {
        void* pushSocket = zmq_socket(context, ZMQ_PUSH);
        zmq_connect(pushSocket, "tcp://localhost:4041");
        while(1) {
            MessageData message;
            strcpy(message.Name, Name);
            std::cin >> message.Message; //TODO: write normal input with spaces;
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
