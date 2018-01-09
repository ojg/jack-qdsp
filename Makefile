CC=gcc
CFLAGS=-std=c99 -fPIC -Wall -Wextra -Wa,-adhln

UNAME_M := $(shell uname -m)
ifneq ($(filter arm%,$(UNAME_M)),)
CFLAGS += -O3 -march=native -mfpu=neon-vfpv4 -mtune=cortex-a53 -ffast-math
else
CFLAGS += -O2 -march=native
endif

LDFLAGS_JACK=-ljack -lm
LDFLAGS_FILE=-lsndfile -lrt -lm
SOURCES_COMMON=dsp.c dsp-gate.c dsp-gain.c dsp-iir.c dsp-fir.c
SOURCES_JACK=$(SOURCES_COMMON) jack-qdsp.c
SOURCES_FILE=$(SOURCES_COMMON) file-qdsp.c
DEPS=dsp.h
OBJECTS_DIR=_build
OBJECTS_JACK=$(patsubst %.c, $(OBJECTS_DIR)/%.o, $(SOURCES_JACK))
OBJECTS_FILE=$(patsubst %.c, $(OBJECTS_DIR)/%.o, $(SOURCES_FILE))
EXECUTABLE_JACK=jack-qdsp
EXECUTABLE_FILE=file-qdsp
INSTALLDIR=/usr/local/bin
GIT_VERSION := $(shell git describe --abbrev=4 --dirty --always --tags)

.PHONY: all
all: $(OBJECTS_DIR) $(EXECUTABLE_JACK) $(EXECUTABLE_FILE)

$(OBJECTS_DIR) :
	mkdir -p $(OBJECTS_DIR)

$(EXECUTABLE_JACK): $(OBJECTS_JACK)
	$(CC) $(OBJECTS_JACK) -o $@ $(LDFLAGS_JACK)

$(EXECUTABLE_FILE): $(OBJECTS_FILE)
	$(CC) $(OBJECTS_FILE) -o $@ $(LDFLAGS_FILE)

$(OBJECTS_DIR)/%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -DVERSION=\"$(GIT_VERSION)\" -c $< -o $@ > $@.s

install:	all
	sudo install -Dm 755 $(EXECUTABLE_JACK) $(INSTALLDIR)/$(EXECUTABLE_JACK)
	sudo install -Dm 755 $(EXECUTABLE_FILE) $(INSTALLDIR)/$(EXECUTABLE_FILE)

.PHONY: clean
clean:
	rm -rf $(OBJECTS_JACK) $(EXECUTABLE_JACK)
	rm -rf $(OBJECTS_FILE) $(EXECUTABLE_FILE)

test:
	$(MAKE) -C tests

bench:
	$(MAKE) -C tests ARG=bench
