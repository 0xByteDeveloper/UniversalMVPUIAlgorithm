# =============================================================================
# UniversalMVPUIAlgorithm — MVP GOD STATE UI ANALYSIS SDK
# Makefile — MinGW-w64 cross build (x64 + x86) + native test build
# =============================================================================
SHELL := /bin/bash

CROSS64 ?= x86_64-w64-mingw32-
CROSS86 ?= i686-w64-mingw32-

# rootless mingw kurulumu (scripts/setup_env.sh)
MINGW_BIN ?= $(HOME)/mingw/usr/bin
export PATH := $(MINGW_BIN):$(PATH)

CXXFLAGS ?= -std=c++17 -O2 -Wall -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sign-compare
LDFLAGS   ?=

# ImGui kaynakları (test hedefleri için, çoklu versiyon)
IMGUI_MASTER ?= $(HOME)/deps/imgui-master
IMGUI_1899   ?= $(HOME)/deps/imgui-1.89.9
CAPSTONE_SRC ?= $(HOME)/deps/capstone

BUILD := build
CLI_SRC := cli/main.cpp
SDK_SRC := src/imgui_hash.cpp src/win_process.cpp src/memory_map.cpp src/layout.cpp \
           src/imgui_scan.cpp src/disasm.cpp src/analyzer.cpp src/report.cpp
SDK_INC := -Iinclude

CAP_HEADERS := $(CAPSTONE_SRC)/include

# ---------------------------------------------------------------------------
# Capstone static libs (x64 + x86)
# ---------------------------------------------------------------------------
.PHONY: capstone capstone-clean
capstone: $(BUILD)/capstone-x64.a $(BUILD)/capstone-x86.a

$(BUILD)/capstone-x64.a: $(wildcard $(CAPSTONE_SRC)/*.c)
	@mkdir -p $(BUILD)
	@cd $(CAPSTONE_SRC) && $(MAKE) clean >/dev/null 2>&1 || true
	cd $(CAPSTONE_SRC) && $(MAKE) -j$$(nproc) CROSS=$(CROSS64) CAPSTONE_ARCHS="x86" \
		CAPSTONE_BUILD_CORE_ONLY=yes >/dev/null 2>&1
	cp $(CAPSTONE_SRC)/capstone.lib $@

$(BUILD)/capstone-x86.a: $(wildcard $(CAPSTONE_SRC)/*.c)
	@mkdir -p $(BUILD)
	@cd $(CAPSTONE_SRC) && $(MAKE) clean >/dev/null 2>&1 || true
	cd $(CAPSTONE_SRC) && $(MAKE) -j$$(nproc) CROSS=$(CROSS86) CAPSTONE_ARCHS="x86" \
		CAPSTONE_BUILD_CORE_ONLY=yes >/dev/null 2>&1
	cp $(CAPSTONE_SRC)/capstone.lib $@

# ---------------------------------------------------------------------------
# UMVP CLI (analyzer)
# ---------------------------------------------------------------------------
.PHONY: cli64 cli86
cli64: $(BUILD)/umvp_cli_x64.exe
cli86: $(BUILD)/umvp_cli_x86.exe

$(BUILD)/umvp_cli_x64.exe: $(CLI_SRC) $(SDK_SRC) $(BUILD)/capstone-x64.a
	@mkdir -p $(BUILD)
	$(CROSS64)g++ $(CXXFLAGS) $(SDK_INC) -I$(CAP_HEADERS) $(CLI_SRC) $(SDK_SRC) \
		$(BUILD)/capstone-x64.a -o $@ -static -lpsapi

$(BUILD)/umvp_cli_x86.exe: $(CLI_SRC) $(SDK_SRC) $(BUILD)/capstone-x86.a
	@mkdir -p $(BUILD)
	$(CROSS86)g++ $(CXXFLAGS) -DUMVP_X86_TARGET $(SDK_INC) -I$(CAP_HEADERS) $(CLI_SRC) $(SDK_SRC) \
		$(BUILD)/capstone-x86.a -o $@ -static -lpsapi

# ---------------------------------------------------------------------------
# Test hedefleri: gerçek ImGui menülü uygulamalar
#   IMGVER=master | 1.89.9    BACKEND=dx11 | dx9
# ---------------------------------------------------------------------------
IMGUI_CORE = imgui.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp

define build_target
$(BUILD)/target_$(1)_$(2).exe: tests/targets/target_menu.cpp $$(wildcard $(3)/backends/imgui_impl_win32.cpp) $$(addprefix $(3)/,$(IMGUI_CORE))
	@mkdir -p $(BUILD)
	$(CROSS64)g++ -std=c++17 -O2 -s -Wall -Wno-unused-variable \
		-DUMVP_BACKEND_$(4) -I$(3) -I$(3)/backends \
		tests/targets/target_menu.cpp $(3)/backends/imgui_impl_win32.cpp \
		$(3)/backends/imgui_impl_$(1).cpp $(addprefix $(3)/,$(IMGUI_CORE)) \
		-o $$@ -static -municode -lgdi32 -limm32 -ldwmapi -ldwmapi -lole32 -luuid $(5)
endef

DX11_LIBS := -ld3d11 -ldxgi -ld3dcompiler
DX9_LIBS  := -ld3d9

$(eval $(call build_target,dx11,master,$(IMGUI_MASTER),DX11,$(DX11_LIBS)))
$(eval $(call build_target,dx11,1899,$(IMGUI_1899),DX11,$(DX11_LIBS)))
$(eval $(call build_target,dx9,master,$(IMGUI_MASTER),DX9,$(DX9_LIBS)))

.PHONY: targets
targets: $(BUILD)/target_dx11_master.exe $(BUILD)/target_dx11_1899.exe $(BUILD)/target_dx9_master.exe

# ---------------------------------------------------------------------------
# Bağımsız testler (native)
# ---------------------------------------------------------------------------
NATIVE_CXX ?= g++

.PHONY: test-hash
test-hash:
	@bash tests/run_hash_tests.sh

.PHONY: test-layout
test-layout:
	@$(NATIVE_CXX) -std=c++17 -O1 -w $(SDK_INC) tests/test_layout_db.cpp src/layout.cpp -o $(BUILD)/test_layout && $(BUILD)/test_layout

.PHONY: test-integration
test-integration: cli64 targets
	@bash tests/run_integration.sh

.PHONY: all
all: cli64 cli86 targets

.PHONY: clean
clean:
	rm -rf $(BUILD)
