#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <errno.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_BLUE  "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define MAXLEN 512

/* welcome message */
const char WELCOME[] = "OK START Welcome, I compute mean and variance!\n";
/* error messages */
const char ERR_SYNTAX[] = "ERR SYNTAX ";
const char ERR_STATS[] = "ERR STATS ";
const char ERR_DATA[] = "ERR DATA ";
/* OK messages */
const char OK_STATS[] =  "OK STATS ";
const char OK_DATA[] =  "OK DATA ";

char data[MAXLEN];

double mean(int *values) {
    int sum = 0;
    double mean;
    int i;

    for(i = 0; values[i] != 0; i++) {
        sum += values[i];
    }
    return sum/i;
}

double variance(int *values, double mean) {
    int sum = 0;
    int i;

    for(i = 0; values[i] != 0; i++) {
        sum += pow((values[i]-mean), 2);
    }
    return sum/(i-1);
}

void calculate(int childSocket) {
    int values[MAXLEN];
    double m, v;
    int counter = 0;
    char message[MAXLEN] = "";
    char buffer[MAXLEN] = "";

    memset(values, '\0', MAXLEN);

    for(int i = 0; i < strlen(data); i++) {
        if(data[i] == ' ') {
            counter++;
        }
    } 

    if(counter < 1) {
        strcpy(message, ERR_STATS);
        strcat(message, "Not enough data to compute mean or variance.\n");
        write(childSocket, message, sizeof message);
    }
    else {
        fprintf(stdout, ANSI_COLOR_BLUE "Calculating mean and variance...\n" ANSI_COLOR_RESET);

        char *token = strtok(data, " ");
        int i = 0;

        while(token) {
            values[i] = atof(token);
            token = strtok(NULL, " ");
            i++;
        }

        for(int i = 0; values[i] != '\0'; i++) {
            printf("%d", values[i]);
        }
        printf("\n");

        m = mean(values);
        v = variance(values, m);

        sprintf(buffer, "%d %f %f\n", counter, m, v);
        strcpy(message, OK_STATS);
        strcat(message, buffer);
        write(childSocket, message, sizeof message);
        fprintf(stdout, "Message sent.\n");

        memset(values, '\0', MAXLEN);
        memset(data, '\0', MAXLEN);
    }
}

int evaluateData(int childSocket) {
    int counter = 0;
    int dataReceived;
    char buffer[MAXLEN] = "";
    char receiveBuffer[MAXLEN] = "";
    char message[MAXLEN] = "";

    read(childSocket, receiveBuffer, sizeof receiveBuffer);
    fprintf(stdout, ANSI_COLOR_BLUE "Received:%s" ANSI_COLOR_RESET, receiveBuffer);

    /* check if data has correct syntax */
    if(receiveBuffer[0] == ' ') {
        strcpy(message, ERR_SYNTAX);
        strcat(message, "Leading whitespace.\n");
        write(childSocket, message, sizeof message);
        return -1;
    }
    if(receiveBuffer[strlen(receiveBuffer)-1] != '\n') {
        strcpy(message, ERR_SYNTAX);
        strcat(message, "Terminator character absent.\n");
        write(childSocket, message, sizeof message);
        return -1;
    }
    for(int i = 0; i < strlen(receiveBuffer)-2; i++) {
        if(isalpha(receiveBuffer[i]) || ispunct(receiveBuffer[i]) || iscntrl(receiveBuffer[i])) {
            sprintf(buffer, "Invalid character: %c\n", receiveBuffer[i]);
            strcpy(message, ERR_SYNTAX);
            strcat(message, buffer);
            write(childSocket, message, sizeof message);
            return -1;
        }
    }
    /* if data has been sent, check if it's congruent and then store it */
    if(receiveBuffer[0] != '0') {
        for(int i = 0; i < strlen(receiveBuffer); i++) {
            if(receiveBuffer[i] == ' ') {
                counter++;
            }
        } 

        char *token = strtok(receiveBuffer, " ");
        
        if(atoi(token) == counter) {
            strcpy(message, OK_DATA);
            sprintf(buffer, "%d\n", counter);
            strcat(message, buffer);
            write(childSocket, message, sizeof message);
        }
        else {
            strcpy(message, ERR_DATA);
            strcat(message, "Incorrect number of samples sent.\n");
            write(childSocket, message, sizeof message);
            return 0;
        }
        token = strtok(NULL, "\n");
        if(data[0] != '\0') {
            strcat(data, " ");
        }     
        strcat(data, token);
    } 
    /* received 0, process data */
    else if (receiveBuffer[0] == '0'){
        calculate(childSocket);
    }
    return 0;
}

int main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stderr, "Usage: %s <port>", argv[0]);
        exit(1);
    }

    int serverSocket, serverPort, returnStatus;
    struct sockaddr_in server;

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(serverSocket < 0) {
        perror(ANSI_COLOR_RED "Socket error");
        fprintf(stdout, ANSI_COLOR_RESET);
        exit(1);
    }

    fprintf(stdout, ANSI_COLOR_BLUE "Socket created.\n" ANSI_COLOR_RESET);

    serverPort = atoi(argv[1]);

    memset(&server, '\0', sizeof server);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(serverPort);

    if(bind(serverSocket, (struct sockaddr*)&server, sizeof server) < 0) {
        perror(ANSI_COLOR_RED "Bind error");
        fprintf(stdout, ANSI_COLOR_RESET);
        close(serverSocket);
        exit(1);
    }

    fprintf(stdout, ANSI_COLOR_BLUE "Bind completed.\n" ANSI_COLOR_RESET);

    returnStatus = listen(serverSocket, 5);

    if (returnStatus == -1) {
        perror(ANSI_COLOR_RED "Listen error");
        fprintf(stdout, ANSI_COLOR_RESET);
	    close(serverSocket);
        exit(1);
    }

    while(1) {
        struct sockaddr_in clientName = {0};
        int childSocket;
        int clientNameLength = sizeof clientName;
        int connected = 0;
        int status = 0;

        childSocket = accept(serverSocket, (struct sockaddr*)&clientName, &clientNameLength);

        if(childSocket < 0) {
            perror(ANSI_COLOR_RED "Accept error");
            fprintf(stdout, ANSI_COLOR_RESET);
            close(serverSocket);
            exit(1);
        }

        fprintf(stdout, ANSI_COLOR_GREEN "Client connected, address %s\n" ANSI_COLOR_RESET, inet_ntoa(clientName.sin_addr));
        fprintf(stdout, ANSI_COLOR_BLUE "Sending welcome message...\n" ANSI_COLOR_RESET);
        write(childSocket, WELCOME, sizeof WELCOME);

        connected = 1;
        
        while(connected) {
            fprintf(stdout, ANSI_COLOR_BLUE "Waiting for input...\n" ANSI_COLOR_RESET);
            if(evaluateData(childSocket) < 0) {
                connected = 0;
                fprintf(stdout, ANSI_COLOR_RED "Closing connection.\n" ANSI_COLOR_RESET);
            }
        }
        /* close socket */
        close(childSocket);
        /* clear data buffer to avoid sending data to other connecting clients */
        memset(data, '\0', sizeof data);
    }
    
    close(serverSocket);
    return 0;
}