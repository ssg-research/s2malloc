cmake_minimum_required(VERSION 3.16)
project(unialloc)

set(CMAKE_C_STANDARD 11)

include_directories(../include/)


option(GLIBC_OVERRIDE OFF)
message("override: ${GLIBC_OVERRIDE}")

option(DEBUG OFF)
message("debug: ${DEBUG}")

option(WORD_ALIGN OFF)
message("align: ${WORD_ALIGN}")

option(LOG_TO_FILE OFF)
message("log_to_file: ${LOG_TO_FILE}")

option(WRAPPER_INFO OFF)
message("wrapper_info: ${WRAPPER_INFO}")

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    message("x86")
    set(CFLAGS  "-Wl,--no-as-needed -O3 -ldl -msse2 -msse -march=native -maes")
else()
    message("arm")
    set(CFLAGS  "-Wl,--no-as-needed -O3 -ldl -march=armv8-a+fp+simd+crypto+crc")
endif()

if (GLIBC_OVERRIDE)
    message("Enable override")
    set(CFLAGS "${CFLAGS} -DGLIBC_OVERRIDE")
endif(GLIBC_OVERRIDE)

if (DEBUG)
    message("Enable debug")
    set(CFLAGS "${CFLAGS} -DDEBUG")
endif(DEBUG)

if (LOG_TO_FILE)
    message("Enable log_to_file")
    set(CFLAGS "${CFLAGS} -DLOG_TO_FILE")
endif(LOG_TO_FILE)

if (WORD_ALIGN)
    message("Enable word align")
    set(CFLAGS "${CFLAGS} -DWORD_ALIGN")
endif(WORD_ALIGN)

if (WRAPPER_INFO)
    message("Enable wrapper info")
    set(CFLAGS "${CFLAGS} -DWRAPPER_INFO")
endif(WRAPPER_INFO)



if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    set(fl-src
            ../include/metadata_pool.h
            ../include/tlb.h
            ../include/flmalloc.h
            ../include/debug.h
            ../include/atomic.h
            ../include/threads.h
            ../include/pcg.h
            ../include/bibop.h
            ../include/helpers.h
            wrapper.c
            wrapper.cc
            flmalloc.c
            ../include/haesni.h)
else()
    set(fl-src
            ../include/metadata_pool.h
            ../include/tlb.h
            ../include/flmalloc.h
            ../include/debug.h
            ../include/atomic.h
            ../include/threads.h
            ../include/pcg.h
            ../include/bibop.h
            ../include/helpers.h
            wrapper.c
            wrapper.cc
            flmalloc.c
            ../include/haesni.h
            ../include/sse2neon.h)
endif()

unset(GLIBC_OVERRIDE CACHE)
unset(DEBUG CACHE)
unset(LOG_TO_FILE CACHE)

set(CMAKE_C_FLAGS "${CFLAGS}")
set(CMAKE_CXX_FLAGS "${CFLAGS}")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_library(flmalloc SHARED ${fl-src})


