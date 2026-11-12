.DEFAULT_GOAL := all

CONFIG ?= Debug
PYTHON ?= python3

ifeq ($(filter $(CONFIG),Debug Release),)
$(error CONFIG must be Debug or Release)
endif

# The assignment uses v120 (VS2013): target its C++11-era language subset.
# MSVC and GCC switches differ; the Windows solution remains authoritative.
override PROJECT_FLAGS := -std=c++11 -Wall -Wextra -Wpedantic -Werror
override PROJECT_FLAGS += -D_CONSOLE -D_LIB -DUNICODE -D_UNICODE
ifeq ($(CONFIG),Debug)
override PROJECT_FLAGS += -O0 -g -D_DEBUG
else
override PROJECT_FLAGS += -O2 -g -DNDEBUG -ffunction-sections -finline-functions -flto
endif
override PROJECT_LINK_FLAGS := -Wl,--fatal-warnings
ifeq ($(CONFIG),Release)
override PROJECT_LINK_FLAGS += -Wl,--gc-sections
endif

EXECUTABLE := build/linux/$(CONFIG)/cs3241_assgt5
SOURCES := main.cpp vector3D.cpp

.PHONY: all build check run smoke verify-project clean help install-hooks
all: build

# Always compile, even without source changes, so checks cannot skip diagnostics.
build: verify-project
	@mkdir -p "$(dir $(EXECUTABLE))"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(PROJECT_FLAGS) $(SOURCES) $(LDFLAGS) $(PROJECT_LINK_FLAGS) -lglut -lGLU -lGL -o "$(EXECUTABLE)"

verify-project:
	$(PYTHON) scripts/verify-project.py

check: build

run: build
	"$(EXECUTABLE)"

smoke: check
	$(PYTHON) scripts/smoke-test.py "$(EXECUTABLE)"

clean:
	rm -rf build/linux

install-hooks:
	bash scripts/install-pre-push-hook.sh

help:
	@echo "make [check|run|smoke] CONFIG=Debug|Release [CXX=g++|clang++]"
	@echo "Both configurations reject compiler and linker warnings."
	@echo "Headless startup check: xvfb-run -a make smoke"
