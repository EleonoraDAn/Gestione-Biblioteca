# compilatore
CC = gcc
# abilita i warnings
CFLAGS = -g -Wall

all: client clientU clientV biblioteca serverB serverG

client: client.c tessera_bibliotecaria.h settings_Client_Server.h
	$(CC) $(CFLAGS) client.c -o client

clientU: clientU.c tessera_bibliotecaria.h settings_Client_Server.h
	$(CC) $(CFLAGS) clientU.c -o clientU

clientV: clientV.c tessera_bibliotecaria.h settings_Client_Server.h
	$(CC) $(CFLAGS) clientV.c -o clientV

biblioteca: biblioteca.c tessera_bibliotecaria.h settings_Client_Server.h
	$(CC) $(CFLAGS) biblioteca.c -o biblioteca

serverB: serverB.c tessera_bibliotecaria.h settings_Client_Server.h
	$(CC) $(CFLAGS) serverB.c -o serverB

serverG: serverG.c tessera_bibliotecaria.h settings_Client_Server.h
	$(CC) $(CFLAGS) serverG.c -o serverG

clean:
	rm -f *.o
	rm -f clientU clientV client biblioteca serverG serverB