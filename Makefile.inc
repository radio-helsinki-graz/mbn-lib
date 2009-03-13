# Run this Makefile using: make PLATFORM=x
# Where x can be:
#   mingw
#     Mingw environment, static link to pthreadsGC2.a
#     Requires the WinPcap developer pack to be unpacked in /WdpPack
#     Interface modules: Pcap & TCP
#   linux
#     Linux, dynamic link to libpthreads.so
#     Interface modules: Ethernet & TCP

ifndef PLATFORM
	PLATFORM=linux
endif

CFLAGS = -Wall -Wextra
LFLAGS =

ifeq (${PLATFORM}, mingw)
	CFLAGS += -DPTW32_STATIC_LIB -I/WpdPack/Include
	LFLAGS = -lpthreadGC2 -lws2_32 -L/WpdPack/Lib -lwpcap
else
	LFLAGS = -lpthread
	CFLAGS += -std=c89 -pedantic
endif

