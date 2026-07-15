# T32 Makefile
#
# Builds:
#   lib/libt32.a
#   bin/t32-run
#
# Linux/macOS:
#   make
#   make test
#   make install
#
# Windows with MinGW:
#   make
#   make test

PROJECT := t32
SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
BIN_DIR := bin
LIB_DIR := lib

ifeq ($(OS),Windows_NT)
	CC := gcc
	AR := ar
	EXEEXT := .exe

	GNU_BIN ?= C:/Program Files/GNU/bin
	MKDIR_P ?= "$(GNU_BIN)/mkdir.exe" -p
	RM_RF ?= "$(GNU_BIN)/rm.exe" -rf
	CP ?= "$(GNU_BIN)/cp.exe" -f

	PREFIX ?= C:/Program Files/libvm
	PYTHON ?= python
else
	CC ?= cc
	AR ?= ar
	EXEEXT :=

	MKDIR_P ?= mkdir -p
	RM_RF ?= rm -rf
	CP ?= cp -f

	PREFIX ?= $(HOME)/.local
	PYTHON ?= python3
endif

CPPFLAGS ?= -I$(INC_DIR)
CFLAGS ?= -Wall -Wextra -Wpedantic -O2

LIBT32 := $(LIB_DIR)/libt32.a
T32_RUN := $(BIN_DIR)/t32-run$(EXEEXT)

CORE_OBJECTS := \
	$(BUILD_DIR)/t32.o

RUN_OBJECTS := \
	$(BUILD_DIR)/main.o \
	$(BUILD_DIR)/cli.o \
	$(BUILD_DIR)/log.o

all: $(LIBT32) $(T32_RUN)

$(LIBT32): $(CORE_OBJECTS)
	@$(MKDIR_P) "$(LIB_DIR)"
	$(AR) rcs $@ $^

$(T32_RUN): $(RUN_OBJECTS) $(LIBT32)
	@$(MKDIR_P) "$(BIN_DIR)"
	$(CC) $(RUN_OBJECTS) $(LIBT32) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@$(MKDIR_P) "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

test: all
	$(PYTHON) tests/isa_smoke.py

install: all
	@$(MKDIR_P) "$(PREFIX)/bin"
	@$(MKDIR_P) "$(PREFIX)/lib"
	@$(MKDIR_P) "$(PREFIX)/include"
	@$(CP) "$(T32_RUN)" "$(PREFIX)/bin/"
	@$(CP) "$(LIBT32)" "$(PREFIX)/lib/"
	@$(CP) "$(INC_DIR)/t32.h" "$(PREFIX)/include/"
	@$(CP) "$(INC_DIR)/t32_opcodes.h" "$(PREFIX)/include/"
	@$(CP) "$(INC_DIR)/version.h" "$(PREFIX)/include/"
	@echo "Installed t32-run to $(PREFIX)/bin"
	@echo "Installed libt32.a to $(PREFIX)/lib"

clean:
	@$(RM_RF) "$(BUILD_DIR)"
	@$(RM_RF) "$(BIN_DIR)"
	@$(RM_RF) "$(LIB_DIR)"
	@$(RM_RF) "tests/00-smoke/001-r0-47/test.log"
	@$(RM_RF) "tests/00-smoke/001-r0-47/test.t32"
	@$(RM_RF) "tests/01-core-iset/002-mov/mov.log"
	@$(RM_RF) "tests/01-core-iset/002-mov/mov.t32"

rebuild: clean all

.PHONY: all test install clean rebuild
