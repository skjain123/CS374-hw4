#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>    // ssize_t
#include <sys/socket.h> // send(),recv()
#include <netdb.h>            // gethostbyname()

/**
* Client code
* 1. Create a socket and connect to the server specified in the command arugments.
* 2. Prompt the user for input and send that input as a message to the server.
* 3. Print the message received from the server and exit the program.
*/

#define BUFFERSIZE 512
#define CHUNKSIZE 512
#define PERMISSION "enc_client"
#define PERM_GRANTED "PERMISSION GRANTED"
#define PERM_NOT_GRANTED "PERMISSION NOT GRANTED"

// Error function used for reporting issues
void error(const char *msg) { 
    perror(msg); 
    exit(0); 
} 

// Set up the address struct
void setupAddressStruct(struct sockaddr_in* address, int portNumber, char* hostname){
 
    // Clear out the address struct
    memset((char*) address, '\0', sizeof(*address)); 

    // The address should be network capable
    address->sin_family = AF_INET;
    // Store the port number
    address->sin_port = htons(portNumber);

    // Get the DNS entry for this host name
    struct hostent* hostInfo = gethostbyname(hostname); 
    if (hostInfo == NULL) { 
        fprintf(stderr, "CLIENT: ERROR, no such host\n"); 
        exit(0); 
    }
    // Copy the first IP address from the DNS entry to sin_addr.s_addr
    memcpy((char*) &address->sin_addr.s_addr, hostInfo->h_addr_list[0], hostInfo->h_length);
}

char* fts (char* filename) {
	FILE *file = fopen(filename, "r");
    char *content;
    long file_size;

	if (file == NULL) {
        printf("Cannot open the file\n");
        return NULL;
    }

	// move file pointer to the end and get file size
    fseek(file, 0, SEEK_END);
    file_size = ftell(file);

	// go back to the beginning
    rewind(file);

    // alloc memory to the content
	content = (char*) malloc(sizeof(char) * (file_size + 1));
	if (content == NULL) {
        printf("malloc failed\n");
        fclose(file);
        return NULL;
    }

    // read all of the file to content
	fread(content, sizeof(char), file_size, file);
    content[file_size] = '\0';

    // make sure there is a termination char
    if (content[file_size - 1] == '\n') {
        content[file_size - 1] = '\0';
    }

	fclose(file);
	return content;

}


void create_socket (int* socketFD) {
    *socketFD = socket(AF_INET, SOCK_STREAM, 0); // create socket
    if (*socketFD < 0){
        error("CLIENT: ERROR opening socket");
    }
}

void connect_to_server(int socketFD, struct sockaddr_in serverAddress) {
    if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) { // create connection 
        error("CLIENT: ERROR connecting");
    }
}

int send_to_server (int socketFD, char *to_send, int length) {
    // Send message to server
    // Write to the server

    int len_sent = send(socketFD, &length, sizeof(length), 0); // send the length of the message first
    if (len_sent < 0) {
        error("ERROR writing length to socket");
    }

    int data_sent = send(socketFD, to_send, length, 0); // send the message content
    if (data_sent < 0) {
        error("ERROR writing data to socket");
    }

    return data_sent; // return the number of chars sent
}

char* recieve_from_server (int socketFD) {
    int length;
    char buffer[CHUNKSIZE+1] = {0}; // initialize

    // Read the length of the data
    int lengthRead = recv(socketFD, &length, sizeof(length), 0);
    if (lengthRead < 0) {
        error("ERROR reading length from socket");
        return NULL;
    }

    // if we got nothing return nothing
    if (lengthRead == 0) {
        return strdup("");
    }

    if (buffer == NULL) {
        error("ERROR allocating memory for buffer");
        return NULL;
    }

    // Read the data
    int dataRead = recv(socketFD, buffer, length, 0);
    if (dataRead < 0) {
        error("ERROR reading data from socket");
        return NULL;
    }

    return strdup(buffer);
}

void send_in_chunks(int socketFD, char* content) {
    int content_len = strlen(content);

    char chunk[CHUNKSIZE + 1];

    for (int i = 0; i < content_len; i += CHUNKSIZE) {
        memset(chunk, 0, CHUNKSIZE + 1); // reset chunk

        for (int j = 0; j < CHUNKSIZE && (i + j) < content_len; j++) {
            int curr = i + j;
            if (content[curr] != '\n') { // dont count newlines
                chunk[j] = content[curr];
            }
        }

        // send the chunk to the server
        send_to_server(socketFD, chunk, CHUNKSIZE+1);
    }

    // send the termination character to tell the server to stop reading for this file
    send_to_server(socketFD, "\r", 1);
}

char* recieve_chunked_encryption (int socketFD) {
    char* to_recieve;

    char* total_content = NULL;
    int total_len = 0;

    do {
        to_recieve = recieve_from_server(socketFD); // recieve chunks

        if (to_recieve == NULL) {
            error("ERROR receiving from client");
        }

        if (to_recieve[0] == '\r') { // termination char to stop the loop
            free(to_recieve);
            break;
        }

        // alloc memory and append to new content
        char* new_content = (char*) realloc(total_content, total_len + strlen(to_recieve) + 1);
        if (new_content == NULL) {
            free(total_content); // Free previously allocated memory on realloc failure
            error("ERROR reallocating memory for file_content");
        }
        total_content = new_content;

        // append chunk to memory
        memcpy(total_content + total_len, to_recieve, strlen(to_recieve)+1);
        total_len += strlen(to_recieve)+1;
        total_content[total_len-1] = '\n';
        total_content[total_len] = '\0';

        free(to_recieve);
    } while (1);

    return total_content;
}

// argv[1] = plaintext
// argv[2] = key
// argv[3] = port
int main(int argc, char *argv[]) {
    int socketFD;
    struct sockaddr_in serverAddress;

    // Check usage & args
    if (argc < 4) { 
        fprintf(stderr,"USAGE: %s plaintext key port\n", argv[0]); 
        exit(0); 
    }

	char* text_file = fts(argv[1]);
	char* key_file = fts(argv[2]);

    // Create a socket
    create_socket(&socketFD);

     // Set up the server address struct
    setupAddressStruct(&serverAddress, atoi(argv[3]), "localhost");

    // Connect to server
    connect_to_server(socketFD, serverAddress);

    // ask server for permission
    send_to_server(socketFD, PERMISSION, strlen(PERMISSION));
    
    // if server does not give permission, end program!
    if (strcmp(recieve_from_server(socketFD), PERM_GRANTED) != 0) {
        // Close the socket
        close(socketFD);

        //free(to_send);
        free(text_file);
        free(key_file);

        printf("ENC_CLIENT does not have permission to run on this server!\n");

        return 1; // bad return val
    }

    for (int i = 1; i < argc - 1; i++) {

        char* file_content = fts(argv[i]);

        send_in_chunks(socketFD, file_content);
        
    }

    printf("%s\n", recieve_chunked_encryption(socketFD));

    // Close the socket
    close(socketFD);

    //free(to_send);
	free(text_file);
	free(key_file);

    return 0;
}