#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <zmq.h>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <pthread.h>
#include <fcntl.h>

pid_t Print;

void* context;
void* pubSocket;
void* pullSocket;

void* ctx;
void* resSocket;

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

void* checkLogins(void* ptr) {
    int passfile;
    passfile = open("./logins.log", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if(passfile == -1) {
        std::perror("Can't open file");
        return NULL;
    }
    ctx = zmq_ctx_new();
    resSocket = zmq_socket(ctx, ZMQ_REP);
    zmq_bind(resSocket, "tcp://*:4043");
    while(1) {
        zmq_msg_t passMsg;
        zmq_msg_init(&passMsg);
        zmq_msg_recv(&passMsg, resSocket, 0);
        std::cout << "PassCheck recv" << std::endl;
        PassData* pd = (PassData*) zmq_msg_data(&passMsg);
        std::cout << pd->action << " " << pd->Name << " " << pd->Password << std::endl;
        zmq_msg_close(&passMsg);
        char* name = pd->Name;
        char* pass = pd->Password;
        if(pd->action == 0) {
            lseek(passfile, 0, SEEK_SET);
            char fname[80];
            char fpass[256];
            int found = 0;
            int corPass = 0;
            int r = 0;
            while((r = read(passfile, fname, 80)) != 0) {
                if(r == -1) {
                    std::perror("Cant read file");
                }
                read(passfile, fpass, 256);
                std::cout << "Name: " << fname << std::endl;
                std::cout << "Pass: " << fpass << std::endl;
                if(strcmp(name, fname) == 0) {
                    found = 1;
                    if(strcmp(pass, fpass) == 0) {
                        corPass = 1;
                    }
                    break;
                }
            }
            PassCheck check;
            if(corPass == 1) {
                check.result = 0;
            } else if(found == 1) {
                check.result = 1;
            } else {
                check.result = 2;
            }
            zmq_msg_t reply;
            zmq_msg_init_size(&reply, sizeof(PassCheck));
            memcpy(zmq_msg_data(&reply), &check, sizeof(PassCheck));
            if(zmq_msg_send(&reply, resSocket, 0) == -1) {
                std::perror("Cant send passcheck");
                return NULL;
            } else {
                std::cout << "PasscheckIN sent" << std::endl;
            }
            zmq_msg_close(&reply);
        } else {
            lseek(passfile, 0, SEEK_SET);
            char fname[80];
            char fpass[256];
            int found = 0;
            int r = 0;
            while((r = read(passfile, fname, 80)) != 0) {
                if(r == -1) {
                    std::perror("Cant read file");
                }
                read(passfile, fpass, 256);
                if(strcmp(name, fname) == 0) {
                    found = 1;
                    break;
                }
            }
            PassCheck check;
            if(found == 1) {
                check.result = 1;
            } else {
                check.result = 0;
            }
            if(found == 0) {
                lseek(passfile, 0, SEEK_END);
                write(passfile, name, 80);
                write(passfile, pass, 256);
            }
            zmq_msg_t reply;
            zmq_msg_init_size(&reply, sizeof(PassCheck));
            memcpy(zmq_msg_data(&reply), &check, sizeof(PassCheck));
            if(zmq_msg_send(&reply, resSocket, 0) == -1) {
                std::perror("Can't send passcheck");
                return NULL;
            } else {
                std::cout << "PasscheckUP sent" << std::endl;
            }
            zmq_msg_close(&reply);
        }
    }
    return NULL;
}

void* chexit(void* ptr) {
    char ch;
    while(1) {
        std::cin >> ch;
        if(ch == 'q') {
            std::cout << "PrintExit: " << Print << std::endl;
            kill(Print, SIGKILL);
            _exit(0);
        }
    }
    return NULL;
}

void destrCtx() {
    zmq_close(pubSocket);
    zmq_close(pullSocket);
    zmq_ctx_destroy(context);
    zmq_close(resSocket);
    zmq_ctx_destroy(ctx);
}

int main(int argc, char** argv) {
    atexit(destrCtx);
    pthread_t passCheck;
    if(pthread_create(&passCheck, 0, checkLogins, NULL)) {
        std::perror("Can't create thread");
        return 4;
    }
    context = zmq_ctx_new();
    int fd[2];
    pipe(fd);
    if((Print = fork()) == -1) {
        std::perror("Fork failed");
        return 1;
    }
    if(Print == 0) {
        pubSocket = zmq_socket(context, ZMQ_PUB);
        zmq_bind(pubSocket, "tcp://*:4042");
        while(1) {
            size_t mesSize;
            MessageData* m = (MessageData*) malloc(sizeof(MessageData));
            read(fd[0], m->Name, 80);
            read(fd[0], m->Message, 256);
            read(fd[0], m->Address, 80);
            std::cout << "pub addr: " << m->Address << " "  << strlen(m->Address) << std::endl;
            if(strcmp(m->Address, "gr") != 0) {
                char histName[160];
                strcpy(histName, "./.");
                if(strcmp(m->Name, m->Address) < 0) {
                    strcat(histName, m->Name);
                    strcat(histName, m->Address);
                } else {
                    strcat(histName, m->Address);
                    strcat(histName, m->Name);
                }
                strcat(histName, ".log");
                int histfile;
                histfile = open(histName, O_CREAT | O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
                write(histfile, m->Name, strlen(m->Name));
                write(histfile, " ", 1);
                write(histfile, m->Message, strlen(m->Message));
                write(histfile, " ", 1);
                write(histfile, m->Address, strlen(m->Address));
                write(histfile, "\n", 1);
                close(histfile);
            }
            zmq_msg_t message;
            zmq_msg_init_size(&message, sizeof(MessageData));
            memcpy(zmq_msg_data(&message), m, sizeof(MessageData));
            zmq_send(pubSocket, m->Address, strlen(m->Address), ZMQ_SNDMORE);
            int send = zmq_msg_send(&message, pubSocket, 0);
            if(send == -1) {
                std::perror("Can't publish message");
                return 2;
            } else {
                std::cout << "Message sent" << std::endl;
            }
            zmq_msg_close(&message);
            free(m);
        }
    } else {
        std::cout << "Print: " << Print << std::endl;
        pthread_t checkExit;
        pthread_create(&checkExit, 0, chexit, NULL);
        pullSocket = zmq_socket(context, ZMQ_PULL);
        zmq_bind(pullSocket, "tcp://*:4041");
        while(1) {
            zmq_msg_t message;
            zmq_msg_init(&message);
            zmq_msg_recv(&message, pullSocket, 0);
            std::cout << "Message recieved" << std::endl;
            MessageData *m = (MessageData*) zmq_msg_data(&message);
            std::cout << m->Name << " " << m->Message << " " << m->Address << std::endl;
            write(fd[1], m->Name, 80);
            write(fd[1], m->Message, 256);
            write(fd[1], m->Address, 80);
            zmq_msg_close(&message);
        }
    }
}
