setup:
	gcc -std=gnu99 -g -Wall -o smallsh main.c command.c

clean:
	rm smallsh
