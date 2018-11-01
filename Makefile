CC=arm-linux-gnueabihf-

target:
	$(CC)gcc -O2 -Wall -o chat_for_me909s serialCmd.c main.c -lpthread

clean:
	@rm -rf chat_for_me909s
