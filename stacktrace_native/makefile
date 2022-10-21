#PLATFORM = x86_64
PLATFORM = arm_64

ifeq ($(PLATFORM),x86_64)
  COMPILER = x86_64-linux-gnu-gcc
  LINKER = x86_64-linux-gnu-g++
  
  BUILD_DIR_PLATFORM_SUFFIX = linux-x86_64
  #TARGET_NAME_PLATFORM_SUFFIX = .cpython-37m-x86_64-linux-gnu.so
  TARGET_NAME_PLATFORM_SUFFIX = .so

else ifeq ($(PLATFORM),arm_64)
  COMPILER = aarch64-linux-gnu-gcc
  LINKER = aarch64-linux-gnu-gcc

  BUILD_DIR_PLATFORM_SUFFIX = linux-aarch64
  TARGET_NAME_PLATFORM_SUFFIX = .cpython-37m-aarch64-linux-gnu.so

endif

TARGET_NAME = stacktrace_native$(TARGET_NAME_PLATFORM_SUFFIX)

BUILD_DIR = build
BUILD_TMP = $(BUILD_DIR)/temp.$(BUILD_DIR_PLATFORM_SUFFIX)-3.7
BUILD_LIB = $(BUILD_DIR)/lib.$(BUILD_DIR_PLATFORM_SUFFIX)-3.7

INSTALL_DIR = dynlibs
#INSTALL_DIR = .


# ---

all: $(INSTALL_DIR)/$(TARGET_NAME)

$(BUILD_TMP)/stacktrace_native.o : stacktrace_native.cpp
	mkdir -p $(BUILD_TMP)
	$(COMPILER) -pthread -DNDEBUG -g -fwrapv -O2 -Wall -g -fstack-protector-strong -Wformat -Werror=format-security -Wdate-time -D_FORTIFY_SOURCE=2 -fPIC -I/usr/include/python3.7m -I./include -c stacktrace_native.cpp -o $(BUILD_TMP)/stacktrace_native.o

$(BUILD_LIB)/$(TARGET_NAME) : $(BUILD_TMP)/stacktrace_native.o
	mkdir -p $(BUILD_LIB)
	$(LINKER) -pthread -shared -Wl,-O1 -Wl,-Bsymbolic-functions -Wl,-Bsymbolic-functions -Wl,-z,relro -Wl,-Bsymbolic-functions -Wl,-z,relro -g -fstack-protector-strong -Wformat -Werror=format-security -Wdate-time -D_FORTIFY_SOURCE=2 $(BUILD_TMP)/stacktrace_native.o -o $(BUILD_LIB)/$(TARGET_NAME)

$(INSTALL_DIR)/$(TARGET_NAME) : $(BUILD_LIB)/$(TARGET_NAME)
	mkdir -p $(INSTALL_DIR)
	cp $(BUILD_LIB)/$(TARGET_NAME) $(INSTALL_DIR)

clean:
	rm -rf $(BUILD_DIR)
	rm $(INSTALL_DIR)/$(TARGET_NAME) || true

test:
	python3.7 test.py

