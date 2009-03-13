# Run this Makefile using: make PLATFORM=x
# Where x can be:
#   mingw
#     Mingw environment, static link to pthreadsGC2.a
#     No interface modules
#   linux
#     Linux, dynamic link to libpthreads.so
#     Interface modules: Ethernet & TCP

ifndef PLATFORM
	PLATFORM=linux
endif

CFLAGS = -Wall -Wextra -std=c89 -pedantic
LFLAGS =

ifeq (${PLATFORM}, mingw)
	CFLAGS += -DPTW32_STATIC_LIB
	LFLAGS = -lpthreadGC2 -lws2_32
else
	LFLAGS = -lpthread
endif

