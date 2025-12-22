/* expat_config.h - Manually configured for Bazel */
#ifndef EXPAT_CONFIG_H
#define EXPAT_CONFIG_H 1

/* 1234 = LIL_ENDIAN, 4321 = BIGENDIAN */
/* Zig toolchains / Modern systems are predominantly Little Endian (x86, ARM) */
#define BYTEORDER 1234

/* System headers */
#define HAVE_DLFCN_H 1
#define HAVE_FCNTL_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_MEMMOVE 1

/* Build configuration */
#define XML_CONTEXT_BYTES 1024
#define XML_DTD 1
#define XML_NS 1
#define XML_STATIC 1
#define XML_GE 1

#endif /* EXPAT_CONFIG_H */
