# t32-asm Makefile
#
# Linux/macOS:
#   make
#   make install
#
# Windows with MinGW/GNU Make:
#   make
#
# Override CC, CFLAGS, PREFIX, or BINDIR as needed.

CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2

SRC := src/t32-asm.c

ifeq ($(OS),Windows_NT)
TARGET := t32-asm.exe
PREFIX ?= $(USERPROFILE)/.local
BINDIR ?= $(PREFIX)/bin
MKDIR_P := mkdir
CP := copy /Y
RM := del /Q
else
TARGET := t32-asm
PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin
MKDIR_P := mkdir -p
CP := cp
RM := rm -f
endif

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

test: $(TARGET)
ifeq ($(OS),Windows_NT)
	$(TARGET) -f bin examples/org-label-test.s -o org-label-test.bin
else
	./$(TARGET) -f bin examples/org-label-test.s -o org-label-test.bin
endif

install: $(TARGET)
ifeq ($(OS),Windows_NT)
	@if not exist "$(BINDIR)" mkdir "$(BINDIR)"
	copy /Y "$(TARGET)" "$(BINDIR)\\$(TARGET)"
else
	$(MKDIR_P) "$(BINDIR)"
	$(CP) "$(TARGET)" "$(BINDIR)/$(TARGET)"
endif

clean:
ifeq ($(OS),Windows_NT)
	-$(RM) $(TARGET) org-label-test.bin 2>NUL
else
	$(RM) $(TARGET) org-label-test.bin
endif

.PHONY: all test install clean
