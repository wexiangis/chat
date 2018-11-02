CC=

target:
	$(CC)gcc -O2 -Wall -o chat-me909s serialCmd.c main.c -lpthread

clean:
	@rm -rf chat-me909s
