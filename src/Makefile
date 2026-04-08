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
SDK_DIR := ../.deps/idp-sdk/current
SDK_CRT0 := $(SDK_DIR)/lib/crt0cpm3-z80.rel
SDK_LIB := $(SDK_DIR)/lib/libsdk.lib
SDK_CPM_LIB := $(SDK_DIR)/lib/libcpm3-z80.lib
SDK_SDCC_LIB := $(SDK_DIR)/lib/libsdcc-z80.lib

CFLAGS  := -mz80 --std-c11 --no-std-crt0 --nostdlib \
	-I$(SDK_DIR)/include -I/opt/sdcc/share/sdcc/include/gpx
LDFLAGS := -mz80 --std-c11 --no-std-crt0 --nostdlib \
	--code-loc 0x100 --data-loc 0
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

$(IHX): $(C_OBJS) $(S_OBJS) $(SDK_CRT0) $(UGPX_LIB) $(SDK_LIB) $(SDK_CPM_LIB) $(SDK_SDCC_LIB)
	@mkdir -p "$(BUILD_DIR)"
	@echo "[link] $@"
	@$(CC) $(LDFLAGS) -o $@ $(SDK_CRT0) $(C_OBJS) $(S_OBJS) $(UGPX_LIB) $(SDK_LIB) $(SDK_CPM_LIB) $(SDK_SDCC_LIB)

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
