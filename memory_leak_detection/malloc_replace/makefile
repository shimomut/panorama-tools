COMPILER = gcc
LINKER = g++

BUILD_DIR_PLATFORM_SUFFIX = native
TARGET_NAME_PLATFORM_SUFFIX =

TARGET_NAME = malloc_replace$(TARGET_NAME_PLATFORM_SUFFIX)

BUILD_DIR = build
BUILD_TMP = $(BUILD_DIR)/temp.$(BUILD_DIR_PLATFORM_SUFFIX)
BUILD_LIB = $(BUILD_DIR)/lib.$(BUILD_DIR_PLATFORM_SUFFIX)

INSTALL_DIR = .

#OPTIMIZATION = -O0
OPTIMIZATION = -O2

# ---

all: $(INSTALL_DIR)/$(TARGET_NAME)

$(BUILD_TMP)/malloc_replace.o : malloc_replace.cpp
	mkdir -p $(BUILD_TMP)
	$(COMPILER) -pthread -DNDEBUG -g -fwrapv $(OPTIMIZATION) -Wall -g -fstack-protector-strong -Wformat -Werror=format-security -Wdate-time -D_FORTIFY_SOURCE=2 -fPIC -I/usr/include/python3.7m -c malloc_replace.cpp -o $(BUILD_TMP)/malloc_free_trace.o

$(BUILD_TMP)/dlmalloc.o : dlmalloc.c
	mkdir -p $(BUILD_TMP)
	$(COMPILER) -pthread -DNDEBUG -g -fwrapv $(OPTIMIZATION) -Wall -g -fstack-protector-strong -Wformat -Werror=format-security -Wdate-time -D_FORTIFY_SOURCE=2 -fPIC -I/usr/include/python3.7m -c dlmalloc.c -o $(BUILD_TMP)/dlmalloc.o

$(BUILD_LIB)/$(TARGET_NAME) : $(BUILD_TMP)/malloc_replace.o $(BUILD_TMP)/dlmalloc.o
	mkdir -p $(BUILD_LIB)
	$(LINKER) -pthread -Wl,-O1 -Wl,-Bsymbolic-functions -Wl,-Bsymbolic-functions -Wl,-z,relro -Wl,-Bsymbolic-functions -Wl,-z,relro -g -fstack-protector-strong -Wformat -Werror=format-security -Wdate-time -D_FORTIFY_SOURCE=2 $(BUILD_TMP)/malloc_free_trace.o $(BUILD_TMP)/dlmalloc.o -lpython3.7m -ldl -o $(BUILD_LIB)/$(TARGET_NAME)

$(INSTALL_DIR)/$(TARGET_NAME) : $(BUILD_LIB)/$(TARGET_NAME)
	mkdir -p $(INSTALL_DIR)
	cp $(BUILD_LIB)/$(TARGET_NAME) $(INSTALL_DIR)

clean:
	rm -rf $(BUILD_DIR)
	rm $(INSTALL_DIR)/$(TARGET_NAME) || true

run:
	rm malloc_trace.log || true
	$(INSTALL_DIR)/$(TARGET_NAME) ./test.py
	python3.7 ./parse_malloc_trace_log.py malloc_trace.log