# Nintendo Switch homebrew: requires devkitPro/devkitA64, libnx,
# switch-curl, and switch-zlib. Run `make host-test` without devkitPro.
APP := IsaacWorkshopNX
DEVKITPRO ?= /opt/devkitpro
DEVKITA64 ?= $(DEVKITPRO)/devkitA64
PORTLIBS := $(DEVKITPRO)/portlibs/switch

SWITCH_CXX := $(DEVKITA64)/bin/aarch64-none-elf-g++
SWITCH_CC := $(DEVKITA64)/bin/aarch64-none-elf-gcc
SWITCH_OBJS := build/switch_main.o build/switch_core.o build/switch_zip.o build/switch_qr.o build/switch_cm_wire.o
# Match the official switch-examples application template: NROs are PIEs.
# The earlier -fPIC build failed the strict -z text relocation check; this
# tests the official -fPIE code-generation setting without suppressing it.
SWITCH_ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
SWITCH_CXXFLAGS := -std=gnu++17 -O2 -Wall -Wextra -D__SWITCH__ -ffunction-sections -fdata-sections \
	$(SWITCH_ARCH) \
	-Iinclude -Ithird_party -I$(PORTLIBS)/include -I$(DEVKITPRO)/libnx/include
SWITCH_LDFLAGS := -specs=$(DEVKITPRO)/libnx/switch.specs -Wl,--gc-sections \
	$(SWITCH_ARCH) -L$(PORTLIBS)/lib -L$(DEVKITPRO)/libnx/lib

.PHONY: all clean host-test
all: $(APP).nro

build:
	mkdir -p build

build/switch_main.o: source/main.cpp | build
	$(SWITCH_CXX) $(SWITCH_CXXFLAGS) -c $< -o $@

build/switch_core.o: source/core.cpp | build
	$(SWITCH_CXX) $(SWITCH_CXXFLAGS) -c $< -o $@

build/switch_cm_wire.o: source/cm_wire.cpp | build
	$(SWITCH_CXX) $(SWITCH_CXXFLAGS) -c $< -o $@

build/switch_zip.o: source/zip_install.cpp | build
	$(SWITCH_CXX) $(SWITCH_CXXFLAGS) -c $< -o $@

build/switch_qr.o: third_party/qrcodegen.c | build
	$(SWITCH_CC) -std=c11 -O2 -Wall -Wextra -ffunction-sections -fdata-sections $(SWITCH_ARCH) -Ithird_party -c $< -o $@

$(APP).elf: $(SWITCH_OBJS)
	$(SWITCH_CXX) $(SWITCH_LDFLAGS) $^ -Wl,--start-group -lcurl -lz -lnx -Wl,--end-group -o $@

$(APP).nacp:
	$(DEVKITPRO)/tools/bin/nacptool --create "Isaac Workshop NX" "angelcraft / Phylos" "0.4.0" $@

$(APP).nro: $(APP).elf $(APP).nacp
	$(DEVKITPRO)/tools/bin/elf2nro $(APP).elf $@ --nacp=$(APP).nacp

host-test: | build
	g++ -std=c++17 -O2 -Wall -Wextra -Iinclude source/core.cpp source/zip_install.cpp tests/host_cli.cpp -lz -o build/host_cli
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Ithird_party tests/qr_smoke.c third_party/qrcodegen.c -o build/qr_smoke
	./build/qr_smoke
	g++ -std=c++17 -O2 -Wall -Wextra -Werror -Iinclude source/cm_wire.cpp tests/cm_wire_test.cpp -o build/cm_wire_test
	./build/cm_wire_test
	python3 -m unittest discover -s tests -v

clean:
	rm -f $(APP).elf $(APP).nro $(APP).nacp build/switch_main.o build/switch_core.o build/switch_zip.o build/switch_qr.o build/switch_cm_wire.o build/host_cli build/qr_smoke build/cm_wire_test
