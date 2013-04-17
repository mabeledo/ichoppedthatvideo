CC = gcc
# Debug setup
CFLAGS = -Wall -static -pg -g -lssl -lrt -lm -lGeoIP -pthread -DSYSLOG_SUPPORT `mysql_config --libs` -I/usr/include/mysql -c
#CFLAGS = -march=native -O2 -m64 -falign-functions=64 -fomit-frame-pointer -Wall -static -lssl -lrt -lm -pthread -DSYSLOG_SUPPORT `mysql_config --libs` -I/usr/include/mysql -c
LDFLAGS = -Wall -pg -g -pthread -lssl -lrt -lm -lGeoIP -DSYSLOG_SUPPORT `mysql_config --libs` -I/usr/include/mysql
SOURCES = reply.c common.c signal.c logging.c conn.c request.c file.c stream.c stat.c security.c ichoppedthatvideo.c
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLE = ichoppedthatvideo

all: $(SOURCES) $(EXECUTABLE)
			
$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
	
.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(OBJECTS) $(EXECUTABLE)	
