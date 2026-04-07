TARGET ?= kamenje

SRC_DIR    := .
BUILD_ROOT := ../build
BIN_DIR    := ../bin
BUILD_DIR  := $(BUILD_ROOT)/$(TARGET)
OUT_BASE   := $(BUILD_DIR)/$(TARGET)
DISK_IMAGE := $(BIN_DIR)/$(TARGET).img

CC      := sdcc
AS      := sdasz80
OBJCOPY := sdobjcopy

UGPX_LIB := /opt/sdcc/share/sdcc/lib/z80/ugpx.lib
PARTNER_DIR := ../../libpartner
PARTNER_LIB := $(PARTNER_DIR)/bin/libpartner.lib

CFLAGS  := --std-c11 --code-loc 0x100 -I/opt/sdcc/share/sdcc/include/gpx -I$(PARTNER_DIR)/bin/include
ASFLAGS := -xlos -g

C_SRCS  := $(wildcard $(SRC_DIR)/*.c)
C_OBJS  := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.rel,$(C_SRCS))
S_SRCS  := $(wildcard $(SRC_DIR)/*.s)
S_OBJS  := $(patsubst $(SRC_DIR)/%.s,$(BUILD_DIR)/%.rel,$(S_SRCS))

IHX := $(OUT_BASE).ihx
COM := $(BIN_DIR)/$(TARGET).com

all: $(DISK_IMAGE)

$(BUILD_DIR)/%.rel: $(SRC_DIR)/%.c
	@mkdir -p "$(dir $@)"
	@echo "[cc] $< -> $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.rel: $(SRC_DIR)/%.s
	@mkdir -p "$(dir $@)"
	@echo "[as] $< -> $@"
	@$(AS) $(ASFLAGS) -o $@ $<

$(IHX): $(C_OBJS) $(S_OBJS) $(UGPX_LIB) $(PARTNER_LIB)
	@mkdir -p "$(BUILD_DIR)"
	@echo "[link] $@"
	@$(CC) $(CFLAGS) -o $@ $(C_OBJS) $(S_OBJS) $(UGPX_LIB) $(PARTNER_LIB)

$(COM): $(IHX)
	@mkdir -p "$(BIN_DIR)"
	@echo "[objcopy] $< -> $@"
	@$(OBJCOPY) -I ihex -O binary "$<" "$@"

$(DISK_IMAGE): $(COM)
	@mkdir -p "$(BIN_DIR)"
	@echo "[disk] $(COM) -> $@"
	@rm -f "$@"
	@cpmdisk create "$@" fdd
	@cpmdisk add "$@" "$(COM)"

clean:
	@rm -rf "$(BUILD_DIR)" "$(COM)" "$(DISK_IMAGE)"

.PHONY: all clean
