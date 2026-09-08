CC = gcc
LD = gcc
CFLAGS = -O3 -Wall -Werror
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
