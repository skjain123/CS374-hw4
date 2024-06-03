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
#define PERMISSION "dec_client"

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

    // alloc mem for the content
	content = (char*) malloc(sizeof(char) * (file_size + 1));
	if (content == NULL) {
        printf("malloc failed\n");
        fclose(file);
        return NULL;
    }

    // add termination char
	fread(content, sizeof(char), file_size, file);
    content[file_size] = '\0';

    // strip off newline
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

    int len_sent = send(socketFD, &length, sizeof(length), 0);
    if (len_sent < 0) {
        error("ERROR writing length to socket");
    }

    int data_sent = send(socketFD, to_send, length, 0);
    if (data_sent < 0) {
        error("ERROR writing data to socket");
    }

    return data_sent;
}

char* recieve_from_server (int socketFD) {
    int length;
    char buffer[CHUNKSIZE+1];

    // Read the length of the data
    int lengthRead = recv(socketFD, &length, sizeof(length), 0);
    if (lengthRead < 0) {
        error("ERROR reading length from socket");
        return NULL;
    }

    // if recieve nothing return nothing
    if (lengthRead == 0) {
        return strdup("");
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

    // create chunks
    for (int i = 0; i < content_len; i += CHUNKSIZE) {
        memset(chunk, 0, CHUNKSIZE + 1); // reset chunk

        // go through all content in chunks
        for (int j = 0; j < CHUNKSIZE && (i + j) < content_len; j++) {
            int curr = i + j;
            if (content[curr] != '\n') { // ignore newlines
                chunk[j] = content[curr];
            }
        }

        // send the chunk to server
        send_to_server(socketFD, chunk, CHUNKSIZE);
    }

    // tell server to stop reading
    send_to_server(socketFD, "\r", 1);
}

char* recieve_chunked_encryption (int socketFD) {
    char* to_recieve;
    char* total_content = NULL;
    int total_len = 0;

    do {
        // recieve chunks
        to_recieve = recieve_from_server(socketFD);

        if (to_recieve == NULL) {
            error("ERROR receiving from client");
        }

        // if the server told you to stop reading, stop the loop
        if (to_recieve[0] == '\r') {
            free(to_recieve);
            break;
        }

        // allocate memory to new content to append the chunk to memory
        char* new_content = (char*) realloc(total_content, total_len + strlen(to_recieve) + 1);
        if (new_content == NULL) {
            free(total_content); // Free previously allocated memory on realloc failure
            error("ERROR reallocating memory for file_content");
        }
        total_content = new_content;

        memcpy(total_content + total_len, to_recieve, strlen(to_recieve)+1);
        total_len += strlen(to_recieve)+1;
        total_content[total_len] = '\0';

        free(to_recieve);
    } while (1);

    // Ensure no extra newline is added
    if (total_content[total_len - 1] == '\n') {
        total_content[total_len - 1] = '\0';
    }

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
    if (strcmp(recieve_from_server(socketFD), "PERMISSION GRANTED") != 0) {
        // Close the socket
        close(socketFD);

        //free(to_send);
        free(text_file);
        free(key_file);

        printf("DEC_CLIENT does not have permission to run on this server!\n");

        return 1; // bad return val
    }

    send_to_server(socketFD, "2", 1);
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