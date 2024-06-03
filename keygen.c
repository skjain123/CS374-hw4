#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#define NUM_ALPHABET 27 // 26 + space char

int main(int argc, char *argv[]) {

    srand(time(NULL));

    int key_size = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s keylength\n", argv[0]);
        return 1;
    }

    if (!(key_size = atoi(argv[1]))) {
        printf("Invalid Input!\nUsage: %s (int)keylength\n", argv[0]);
        return 0;
    }

    if (key_size < 0) {
        printf("Invalid Input\nkeylength must be a positive integer!\n");
        return 0;
    }

    char alphabet[NUM_ALPHABET] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

    for (int i = 0; i < key_size; i++) {
        int rand_c = rand() % NUM_ALPHABET;

        printf("%c", alphabet[rand_c]);
    }
    printf("\n");

    return 0;

}   