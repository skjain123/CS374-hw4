TARGETS = enc_server enc_client dec_server dec_client keygen

SRCS = enc_server.c enc_client.c dec_server.c dec_client.c keygen.c

all: $(TARGETS)

enc_server: enc_server.c
	gcc -Wall -g -o $@ $<

enc_client: enc_client.c
	gcc -Wall -g -o $@ $<

dec_server: dec_server.c
	gcc -Wall -g -o $@ $<

dec_client: dec_client.c
	gcc -Wall -g -o $@ $<

keygen: keygen.c
	gcc -Wall -g -o $@ $<

# Clean up the executables
clean:
	rm -f $(TARGETS) *.o
