CC = gcc
LD = gcc
CFLAGS = -g -Wall -Werror #-O3 
LDFLAGS = 
RM = /bin/rm -f
OBJS = wish.o
EXECUTABLE = wish

$(EXECUTABLE): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(EXECUTABLE)

wish.o: wish.c
	$(CC) $(CFLAGS) -c wish.c


clean:
	$(RM) $(EXECUTABLE) $(OBJS)
