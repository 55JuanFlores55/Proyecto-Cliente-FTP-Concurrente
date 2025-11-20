# ============================================================================
#  Makefile para proyecto FTP Cliente Concurrente
# ============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -std=c11

# Objetos a compilar
OBJS = FloresJ-clienteFTP.o connectsock.o connectTCP.o errexit.o

TARGET = FloresJ-clienteFTP

# Compilación principal
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Compilaciones individuales
FloresJ-clienteFTP.o: FloresJ-clienteFTP.c
	$(CC) $(CFLAGS) -c FloresJ-clienteFTP.c

connectsock.o: connectsock.c
	$(CC) $(CFLAGS) -c connectsock.c

connectTCP.o: connectTCP.c
	$(CC) $(CFLAGS) -c connectTCP.c

errexit.o: errexit.c
	$(CC) $(CFLAGS) -c errexit.c

# Limpiar compilación
clean:
	rm -f *.o $(TARGET)
