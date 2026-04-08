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
SDK_API_URL      := https://api.github.com/repos/iskra-delta/idp-sdk/releases/latest
SDK_ROOT         := $(BUILD_ROOT)/idp-sdk
SDK_DIR          := $(SDK_ROOT)/current
SDK_READY        := $(SDK_ROOT)/.ready
SDK_INCLUDE_DIR  := $(SDK_DIR)/include
SDK_LIB          := $(SDK_DIR)/lib/libsdk.lib

CFLAGS  := --std-c11 --code-loc 0x100 -I/opt/sdcc/share/sdcc/include/gpx -I$(SDK_INCLUDE_DIR)
ASFLAGS := -xlos -g

C_SRCS  := $(wildcard $(SRC_DIR)/*.c)
C_OBJS  := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.rel,$(C_SRCS))
S_SRCS  := $(wildcard $(SRC_DIR)/*.s)
S_OBJS  := $(patsubst $(SRC_DIR)/%.s,$(BUILD_DIR)/%.rel,$(S_SRCS))

IHX := $(OUT_BASE).ihx
COM := $(BIN_DIR)/$(TARGET).com

all: $(DISK_IMAGE)

$(SDK_READY): FORCE
	@mkdir -p "$(SDK_ROOT)"
	@echo "[sdk] syncing latest idp-sdk release"
	@set -e; \
	tmp_dir="$$(mktemp -d "$(SDK_ROOT)/tmp.XXXXXX")"; \
	trap 'rm -rf "$$tmp_dir"' EXIT HUP INT TERM; \
	rel_json="$$(curl -fsSL "$(SDK_API_URL)")"; \
	tag="$$(printf '%s\n' "$$rel_json" | sed -n 's/.*"tag_name":[[:space:]]*"\([^"]*\)".*/\1/p' | head -n1)"; \
	asset_url="$$(printf '%s\n' "$$rel_json" | sed -n 's/.*"browser_download_url":[[:space:]]*"\([^"]*idp-sdk-[^"]*\.tar\.gz\)".*/\1/p' | head -n1)"; \
	[ -n "$$tag" ] || { echo "Could not resolve latest idp-sdk tag"; exit 1; }; \
	[ -n "$$asset_url" ] || { echo "Could not resolve latest idp-sdk asset"; exit 1; }; \
	current_tag="$$(cat "$(SDK_ROOT)/VERSION" 2>/dev/null || true)"; \
	if [ "$$current_tag" = "$$tag" ] && \
	   [ -f "$(SDK_INCLUDE_DIR)/partner/conio.h" ] && \
	   [ -f "$(SDK_LIB)" ]; then \
		echo "[sdk] release $$tag already installed"; \
		touch "$@"; \
		exit 0; \
	fi; \
	echo "[sdk] release $$tag"; \
	curl -fsSL -o "$$tmp_dir/idp-sdk.tar.gz" "$$asset_url"; \
	tar -xzf "$$tmp_dir/idp-sdk.tar.gz" -C "$$tmp_dir"; \
	src_dir="$$(find "$$tmp_dir" -mindepth 1 -maxdepth 1 -type d -name 'idp-sdk-*' | head -n1)"; \
	[ -n "$$src_dir" ] || { echo "Could not locate extracted idp-sdk payload"; exit 1; }; \
	[ -f "$$src_dir/include/partner/conio.h" ] || { echo "SDK missing include/partner/conio.h"; exit 1; }; \
	[ -f "$$src_dir/lib/libsdk.lib" ] || { echo "SDK missing lib/libsdk.lib"; exit 1; }; \
	rm -rf "$(SDK_DIR)"; \
	mv "$$src_dir" "$(SDK_DIR)"; \
	printf '%s\n' "$$tag" > "$(SDK_ROOT)/VERSION"; \
	touch "$@"

$(C_OBJS): $(SDK_READY)

$(BUILD_DIR)/%.rel: $(SRC_DIR)/%.c
	@mkdir -p "$(dir $@)"
	@echo "[cc] $< -> $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.rel: $(SRC_DIR)/%.s
	@mkdir -p "$(dir $@)"
	@echo "[as] $< -> $@"
	@$(AS) $(ASFLAGS) -o $@ $<

$(IHX): $(SDK_READY) $(C_OBJS) $(S_OBJS) $(UGPX_LIB) $(SDK_LIB)
	@mkdir -p "$(BUILD_DIR)"
	@echo "[link] $@"
	@$(CC) $(CFLAGS) -o $@ $(C_OBJS) $(S_OBJS) $(UGPX_LIB) $(SDK_LIB)

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

.PHONY: all clean FORCE
