#include <stdio.h>      
#include <sys/types.h>
#include <sys/socket.h>   
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_BLUE  "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define MAXLEN 512

const char *CODES[] = {"OK", "ERR", "SYNTAX", "START", "DATA", "STATS"}; /* codes sent from server */

//const FILE *fp = fopen("client_log", "w"); /* activity log*/
char receiveBuffer[MAXLEN]; /* debug data received from server */
char data[MAXLEN]; /* data received from user */
char formattedData[MAXLEN]; /* data formatted, ready to be sent to server*/
char serverData[MAXLEN]; /* processed data received from server (mean and variance) */
char storage[MAXLEN];


/* show actions and wait for user choice */
void showOptions() {
    int choice;
    fprintf(stdout, ANSI_COLOR_BLUE "Options:\n" ANSI_COLOR_RESET
                    "1. Send data to server\n"
                    "2. Calculate mean and variance\n"
                    "3. Show usage instructions\n"
                    "0. Close connection\n");
}

void showUsage() {
    fprintf(stdout, ANSI_COLOR_BLUE "Usage instructions: \n" ANSI_COLOR_RESET
                    "1. To send data to server, select option 1 by typing '1' when prompted\n"
                    "2. Enter samples, one on each line (press enter to switch to a new line)\n"
                    "3. Enter the character 'q' on a new line when done\n"
                    "4. When the options menu appears, select option '2' to get results from server, or '1' again to send additional samples\n"
                    "The program terminates automatically after a successful transaction.\n");
}

/* format data in server readable form */
void formatData() {
    int counter = -1;

    memset(formattedData, '\0', MAXLEN);

    /* count data to be sent */
    for(int i = 0; i < strlen(data); i++) {
        if(data[i] == ' ') {
            counter++;
        }
    }

    sprintf(formattedData, "%d", counter);
    strcat(formattedData, data);
    formattedData[strlen(formattedData)-1] = '\n';  
    /* clears data buffer for later use */
    strcat(storage, data);
    memset(data, '\0', MAXLEN);
}   

/* send data to server */
int sendData(int simpleSocket) {
    int i = 0, c;
    size_t len;

    fprintf(stdout, "Enter samples (integer values) one at a time, 'q' (on a separate line) to terminate:\n");
    while((c = getchar()) != 'q') {
        if(c == '\n') {
            printf("> ");
            if(data[i-1] != ' ') {
                data[i] = ' ';
                i++;
            }
        }
        else if(isdigit(c)) {
            data[i] = c;
            i++;
        }
        else {
            fprintf(stdout, ANSI_COLOR_RED "Character %c not allowed, please enter an integer value.\n" ANSI_COLOR_RESET, c);
        }
    }
    if(i < 2) {
        fprintf(stdout, ANSI_COLOR_RED "Not enough data.\n" ANSI_COLOR_RESET);
        return 1;
    }
    formatData();
    fprintf(stdout, ANSI_COLOR_BLUE "Sending data...\n" ANSI_COLOR_RESET);
    write(simpleSocket, formattedData, MAXLEN);
    fprintf(stdout, ANSI_COLOR_BLUE "Data sent.\n" ANSI_COLOR_RESET);
    memset(formattedData, '\0', sizeof formattedData);
    return 0;
}

/* strip codes form server response and present it to user */
int handleResponse(int simpleSocket) {
    read(simpleSocket, receiveBuffer, sizeof receiveBuffer);

    char *token = strtok(receiveBuffer, " ");

    /* received OK */
    if(strcmp(token, CODES[0]) == 0) {
        token = strtok(NULL, " ");
        /* received START */
        if(strcmp(token, CODES[3]) == 0) {
            token = strtok(NULL, "\n");
            fprintf(stdout, ANSI_COLOR_GREEN "%s\n" ANSI_COLOR_RESET, token);
        }
        /* received DATA */
        if(strcmp(token, CODES[4]) == 0) {
            token = strtok(NULL, "\n");
            fprintf(stdout, ANSI_COLOR_GREEN "%s samples correctly received.\n" ANSI_COLOR_RESET, token);
        }
        /* received STATS, store in buffer for later use */
        if(strcmp(token, CODES[5]) == 0) {
            int i = 0;
            token = strtok(NULL, "\n");
            sprintf(serverData, "%s", token);
        }
    }
    /* an error occurred, received ERR */
    else if(strcmp(token, CODES[1]) == 0){
        //printf("Debug: ERR\n");
        token = strtok(NULL, " ");
        /* received SYNTAX, close connection */
        if(strcmp(token, CODES[2]) == 0) {
            token = strtok(NULL, "\n");
            printf(ANSI_COLOR_RED "Malformed message: %s\n" ANSI_COLOR_RESET, token);
            printf("Closing connection.\n");
            return -1;
        }
        /* received DATA */
        if(strcmp(token, CODES[4]) == 0) {
            token = strtok(NULL, "\n");
            fprintf(stdout, ANSI_COLOR_RED "Data error: %s\n" ANSI_COLOR_RESET, token);
            fprintf(stdout, ANSI_COLOR_RED "Please try again.\n" ANSI_COLOR_RESET);
            return 1;
        }
        /* received STATS */
        if(strcmp(token, CODES[5]) == 0) {
            token = strtok(NULL, "\n");
            fprintf(stdout, ANSI_COLOR_RED "Computation error: %s\n" ANSI_COLOR_RESET, token);
            fprintf(stdout, ANSI_COLOR_RED "Select option 1 and send data to server first.\n" ANSI_COLOR_RESET);
            return 1;
        }
    }
    memset(receiveBuffer, '\0', MAXLEN);
    return 0;
}

void showResults() {
    char *token = strtok(serverData, " ");
    token = strtok(NULL, " ");

    fprintf(stdout, ANSI_COLOR_GREEN "Samples:%s\n" ANSI_COLOR_RESET, storage);
    fprintf(stdout, ANSI_COLOR_GREEN "Mean: %s\n" ANSI_COLOR_RESET, token);
    token = strtok(NULL, "\n");
    fprintf(stdout, ANSI_COLOR_GREEN "Variance: %s\n" ANSI_COLOR_RESET, token);

    memset(storage, '\0', MAXLEN);
    memset(serverData, '\0', MAXLEN);
}

int main(int argc, char **argv) {
    /* print usage information */
    if(argc != 3) {
        fprintf(stderr, "Usage: %s <server address> <port>", argv[0]);
        exit(1);
    }

    int simpleSocket = 0;
    int simplePort = 0;
    struct sockaddr_in simpleServer;
    int connected = 0;

    /* client setup */
    simpleSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(simpleSocket < 0) {
        perror(ANSI_COLOR_RED "Socket error");
        fprintf(stdout, ANSI_COLOR_RESET);
        exit(1);
    }

    fprintf(stdout, ANSI_COLOR_BLUE "Socket created.\n" ANSI_COLOR_RESET);

    simplePort = atoi(argv[2]);

    memset(&simpleServer, '\0', sizeof simpleServer);
    simpleServer.sin_family = AF_INET;
    simpleServer.sin_addr.s_addr = inet_addr(argv[1]);
    simpleServer.sin_port = htons(simplePort);

    if(connect(simpleSocket, (struct sockaddr*)&simpleServer, sizeof simpleServer) < 0) {
        perror(ANSI_COLOR_RED "Connect error");
        fprintf(stdout, ANSI_COLOR_RESET);
        close(simpleSocket);
        exit(1);
    }

    connected = 1;
    fprintf(stdout, ANSI_COLOR_BLUE "Connected to %s.\n" ANSI_COLOR_RESET, inet_ntoa(simpleServer.sin_addr));

    /* handle welcome message from server and clear buffer */
    handleResponse(simpleSocket);
    memset(receiveBuffer, '\0', MAXLEN);
    
    /* client loop */
    while(connected) {
        char choice[MAXLEN] = "0";
        int status;

        showOptions();
        fprintf(stdout, "> ");
        fscanf(stdin, "%s", choice);
        switch(atoi(choice)) {
            /* disconnect from server and close client socket */
            case 0:
                connected = 0;
                break;
            /* send data to server and handle response */
            case 1:
                if(!sendData(simpleSocket)) {
                    status = handleResponse(simpleSocket);
                    if(status < 0) {
                        connected = 0;
                    }  
                }
                break;
            case 2:
                write(simpleSocket, "0\n", 2);
                status = handleResponse(simpleSocket);
                if(status < 0) {
                    connected = 0; 
                }
                else if(status == 0) {
                    showResults(atoi(choice));
                }
                connected = 0;
                break;
            case 3:
                showUsage();
                break;
            default: 
                fprintf(stdout, ANSI_COLOR_RED "Invalid command.\n" ANSI_COLOR_RESET);
                break;
        }
    }

    fprintf(stdout, ANSI_COLOR_BLUE "Disconnecting.\n" ANSI_COLOR_RESET);
    close(simpleSocket);
    return 0;
}