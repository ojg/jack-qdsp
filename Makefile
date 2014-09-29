CC=gcc
CFLAGS=-c -O3 -ffast-math -march=native -ftree-vectorize -std=c99
LDFLAGS_JACK=-ljack
LDFLAGS_FILE=-lsndfile -lrt
SOURCES_COMMON=dsp.c dsp-gate.c dsp-gain.c dsp-iir.c dsp-clip.c
SOURCES_JACK=$(SOURCES_COMMON) jack-qdsp.c
SOURCES_FILE=$(SOURCES_COMMON) file-qdsp.c
DEPS=dsp.h
OBJECTS_JACK=$(SOURCES_JACK:.c=.o)
OBJECTS_FILE=$(SOURCES_FILE:.c=.o)
EXECUTABLE_JACK=jack-qdsp
EXECUTABLE_FILE=file-qdsp
PREFIX=/usr

all: $(SOURCES_JACK) $(EXECUTABLE_JACK) $(SOURCES_FILE) $(EXECUTABLE_FILE)
   
$(EXECUTABLE_JACK): $(OBJECTS_JACK) 
	$(CC) $(OBJECTS_JACK) -o $@ $(LDFLAGS_JACK)

$(EXECUTABLE_FILE): $(OBJECTS_FILE) 
	$(CC) $(OBJECTS_FILE) -o $@ $(LDFLAGS_FILE)

%.o: %.c $(DEPS) 
	$(CC) $(CFLAGS) $(DEFINES) $< -o $@

install:	all
	sudo install -Dm 755 $(EXECUTABLE_JACK) $(DESTDIR)$(PREFIX)/bin/$(EXECUTABLE_JACK)
	sudo install -Dm 755 $(EXECUTABLE_FILE) $(DESTDIR)$(PREFIX)/bin/$(EXECUTABLE_FILE)
    
clean: 
	rm -rf $(OBJECTS_JACK) $(EXECUTABLE_JACK)
	rm -rf $(OBJECTS_FILE) $(EXECUTABLE_FILE)

test:
	$(MAKE) -C tests

