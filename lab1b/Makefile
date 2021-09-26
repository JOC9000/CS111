#NAME: Jorge Contreras
#EMAIL: jorgec9000@g.ucla.edu
#ID: 205379811

all: lab1b-client.c lab1b-server.c
	gcc -lz -Wall -Wextra lab1b-client.c -o lab1b-client
	gcc -lz -Wall -Wextra lab1b-server.c -o lab1b-server

lab1b-client: lab1b-client.c
	gcc -lz -Wall -Wextra lab1b-client.c -o lab1b-client

lab1b-server: lab1b-server.c
	gcc -lz -Wall -Wextra lab1b-server.c -o lab1b-server

dist: lab1b-server.c lab1b-client.c Makefile README
	tar -cvzf lab1b-205379811.tar.gz lab1b-server.c lab1b-client.c README Makefile

clean:
	rm -f lab1b-205379811.tar.gz lab1b-client lab1b-server
