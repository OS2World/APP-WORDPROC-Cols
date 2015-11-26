#!make -f
CC=gcc
#CFLAGS=-Wall -s -O -DOS2 -Zmtd 
CFLAGS=-Wall -s -O -DOS2 -Zmtd -Zomf

cols.exe: cols.c
	$(CC) $(CFLAGS) -o cols.exe cols.c
