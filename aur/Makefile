TARGET=siphon
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:%.c=%.o)

all: 

	gcc -c $(SOURCES)
	gcc -o $(TARGET) $(SOURCES)

clean:
	$(RM) $(OBJECTS) $(TARGET)

install:
	install -D $(TARGET) ${DESTDIR}/usr/bin/$(TARGET)