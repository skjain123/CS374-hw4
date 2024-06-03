#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define CHUNKSIZE 512
#define LEN_ERROR "Invalid key! The key must be longer in length than the file content!\n"
#define CHAR_ERROR "invalid character, not sending!\n"
#define PERMISSION "dec_client"
#define PERM_GRANTED "PERMISSION GRANTED"
#define PERM_NOT_GRANTED "PERMISSION NOT GRANTED"

// Error function used for reporting issues
void error(const char *msg) {
    perror(msg);
    exit(1);
}

// Set up the address struct for the server socket
void setupAddressStruct(struct sockaddr_in* address, int portNumber){
 
    // Clear out the address struct
    memset((char*) address, '\0', sizeof(*address)); 

    // The address should be network capable
    address->sin_family = AF_INET;
    // Store the port number
    address->sin_port = htons(portNumber);
    // Allow a client at any address to connect to this server
    address->sin_addr.s_addr = INADDR_ANY;
}

int respond_to_client (int socketFD, const char* response, int length) {
    // Send message to server
    // Write to the server

    int len_sent = send(socketFD, &length, sizeof(length), 0);
    if (len_sent < 0) {
        error("ERROR writing length to socket");
    }

    int data_sent = send(socketFD, response, length, 0);
    if (data_sent < 0) {
        error("ERROR writing data to socket");
    }

    return data_sent;
}

int create_socket (int port, struct sockaddr_in serverAddress) {
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) {
        error("ERROR opening socket");
    }

    // Set up the address struct for the server socket
    setupAddressStruct(&serverAddress, port);

    // Associate the socket to the port
    if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        error("ERROR on binding");
    }

    // Start listening for connetions. Allow up to 5 connections to queue up
    listen(listenSocket, 5); 
    
    return listenSocket;
}

char* recieve_from_client (int connectionSocket) {
    int length;
    char buffer[CHUNKSIZE+1];

    // Read the length of the data
    int lengthRead = recv(connectionSocket, &length, sizeof(length), 0);
    if (lengthRead < 0) {
        error("ERROR reading length from socket");
        return NULL;
    }

    // if recieve nothing return nothing
    if (lengthRead == 0) {
        return strdup("");
    }

    // Read the data
    int dataRead = recv(connectionSocket, buffer, length, 0);
    if (dataRead < 0) {
        error("ERROR reading data from socket");
        return NULL;
    }

    buffer[dataRead] = '\0';

    return strdup(buffer);
}

char* recieve_chunked_file (int connectionSocket) {
    char* to_recieve;
    char* total_content = NULL;
    int total_len = 0;

    do {
        to_recieve = recieve_from_client(connectionSocket);

        if (to_recieve == NULL) {
            error("ERROR receiving from client");
        }

        // termination char to tell when to stop loop
        if (to_recieve[0] == '\r') {
            free(to_recieve);
            break;
        }

        // append content to memory
        char* new_content = (char*) realloc(total_content, total_len + strlen(to_recieve) + 1);
        if (new_content == NULL) {
            free(total_content);
            error("ERROR reallocating memory for file_content");
        }
        total_content = new_content;

        // append content to memory
        memcpy(total_content + total_len, to_recieve, strlen(to_recieve));
        total_len += strlen(to_recieve);
        total_content[total_len] = '\0';

        free(to_recieve);
    } while (1);

    // append neccesesary chars when needed
    char* final_content = (char*) realloc(total_content, total_len + 2);
    if (final_content == NULL) {
        free(total_content);
        error("ERROR reallocating memory for final content");
    }
    total_content = final_content;

    // needed chars
    total_content[total_len] = '\n';
    total_content[total_len + 1] = '\0';

    return total_content;
}

void send_in_chunks(int connectionSocket, char* content) {
    int content_len = strlen(content);

    char chunk[CHUNKSIZE + 1];

    // make chunks
    for (int i = 0; i < content_len; i += CHUNKSIZE) {
        memset(chunk, 0, CHUNKSIZE + 1); // reset chunk

        // go through the content in chunks
        for (int j = 0; j < CHUNKSIZE && (i + j) < content_len; j++) {
            int curr = i + j;
            if (content[curr] != '\n') { // ignore newlines
                chunk[j] = content[curr];
            }
        }

        respond_to_client(connectionSocket, chunk, CHUNKSIZE+1); // send the chunk to the client
    }

    respond_to_client(connectionSocket, "\r", 1); // tell the client to stop reading
}

void decrypt_message (int connectionSocket, char* content, char* key) {
    int valid_content_chars = 1;

    for (int j = 0; j < strlen(content); j++) {

        // 10 is the Line Feed character
        if (content[j] != 10 && content[j] != ' ' 
        && (content[j] < 'A' || content[j] > 'Z')) {
            printf("Invalid character %d detected at position %d\n", content[j], j);
            valid_content_chars = 0;
            break;
        }

        // ignore newlines
        if (content[j] == '\n') {
            continue;
        }

        // handle spaces in the content
        int content_c = 0;
        if (content[j] == ' ') {
            content_c = 26;
        } else {
            content_c = content[j] - 'A';
        }
        
        // handle spaces in the keys
        int key_c = 0;
        if (key[j] == ' ') {
            key_c = 26;
        } else {
            key_c = key[j] - 'A';
        }

        // if theres a negative difference loop back by adding 27
        int decrypted_c = (content_c - key_c + 27) % 27;

        if (decrypted_c == 26) { // special case
            content[j] = ' ';
        } else { // regular case
            content[j] = decrypted_c + 'A';
        }

        // handle extra newlines
        if (content[j] == '\n') {
            printf("Invalid character newline detected at position %d\n", j);
        }
    }

    // ensure the content has a termination char
    content[strlen(content)] = '\0';

    // if the content is valid, send, if not error
    if (valid_content_chars == 1) {
        send_in_chunks(connectionSocket, content);
    } else {
        respond_to_client(connectionSocket, CHAR_ERROR, strlen(CHAR_ERROR));
    }

    
}

int main(int argc, char *argv[]){
    int connectionSocket = -1;
    struct sockaddr_in serverAddress, clientAddress;
    socklen_t sizeOfClientInfo = sizeof(clientAddress);

    // Check usage & args
    if (argc < 2) { 
        fprintf(stderr,"USAGE: %s port\n", argv[0]); 
        exit(1);
    } 
    
    // Create the socket that will listen for connections
    int listenSocket = create_socket(atoi(argv[1]), serverAddress);    

    // Accept a connection, blocking if one is not available until one connects
    while(1) {

        // wait for connection and accept
        while (connectionSocket < 0) {
            connectionSocket = accept(listenSocket, (struct sockaddr *)&clientAddress, &sizeOfClientInfo);
            if (connectionSocket < 0) {
                error("ERROR on accept");
            }
        }

        // print that the client connected and accepted
        printf("SERVER: Connected to client running at host %d port %d\n", ntohs(clientAddress.sin_addr.s_addr), ntohs(clientAddress.sin_port));

        // print that the client connected and accepted
        printf("SERVER: Connected to client running at host %d port %d\n", ntohs(clientAddress.sin_addr.s_addr), ntohs(clientAddress.sin_port));

        if (strcmp(recieve_from_client(connectionSocket), PERMISSION) == 0) {
            // give permission and carry on.
            respond_to_client(connectionSocket, PERM_GRANTED, strlen(PERM_GRANTED));
        } else {

            // do not give permission
            respond_to_client(connectionSocket, PERM_NOT_GRANTED, strlen(PERM_NOT_GRANTED));

            // Close the connection socket for this client
            close(connectionSocket);

            // reset
            connectionSocket = -1;
            continue;
        }

        // the client first sends the amount of files they want to send in chunks
        int num_files_recieve = 0;
        if (!(num_files_recieve = atoi(recieve_from_client(connectionSocket)))) {
            printf("Client did not give the amount of things to parse!\n");
        }

        // based on the number of file contents being sent over, allocate memory
        char** file_content = (char**) malloc (sizeof(char*) * num_files_recieve);
        if (file_content == NULL) {
            error("ERROR allocating memory for file_content");
        }

        // recieve the data in chunks and print
        for (int i = 0; i < num_files_recieve; i++) {
            file_content[i] = NULL;

            file_content[i] = recieve_chunked_file(connectionSocket);
        }

        if (strlen(file_content[0]) > strlen(file_content[1])) {
            send_in_chunks(connectionSocket, LEN_ERROR);

        } else {
            decrypt_message(connectionSocket, file_content[0], file_content[1]);
            send_in_chunks(connectionSocket, file_content[0]);
        }
    
        // free the data here
        for (int i = 0; i < num_files_recieve; i++) {
            free(file_content[i]);
        }

        // Close the connection socket for this client
        close(connectionSocket);

        // reset
        connectionSocket = -1;
    }

    // Close the listening socket
    close(listenSocket); 
    return 0;
}
