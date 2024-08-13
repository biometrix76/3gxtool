/**
 * @file types.h
 * @brief Various system types.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint8_t u8; ///<  8-bit unsigned integer
typedef uint16_t u16; ///< 16-bit unsigned integer
typedef uint32_t u32; ///< 32-bit unsigned integer
typedef uint64_t u64; ///< 64-bit unsigned integer

typedef int8_t s8; ///<  8-bit signed integer
typedef int16_t s16; ///< 16-bit signed integer
typedef int32_t s32; ///< 32-bit signed integer
typedef int64_t s64; ///< 64-bit signed integer

typedef volatile u8 vu8; ///<  8-bit volatile unsigned integer.
typedef volatile u16 vu16; ///< 16-bit volatile unsigned integer.
typedef volatile u32 vu32; ///< 32-bit volatile unsigned integer.
typedef volatile u64 vu64; ///< 64-bit volatile unsigned integer.

typedef volatile s8 vs8; ///<  8-bit volatile signed integer.
typedef volatile s16 vs16; ///< 16-bit volatile signed integer.
typedef volatile s32 vs32; ///< 32-bit volatile signed integer.
typedef volatile s64 vs64; ///< 64-bit volatile signed integer.

#define BIT(n) (1U<<(n)) /// Creates a bitmask from a bit number.
#define ALIGN(m) __attribute__((aligned(m))) /// Aligns a struct (and other types?) to m, making sure that the size of the struct is a multiple of m.
#define PACKED __attribute__((packed)) /// Packs a struct (and other types?) so it won't include padding bytes.

static inline uint16_t __local_bswap16(uint16_t x) {
    return ((x << 8) & 0xff00) | ((x >> 8) & 0x00ff);
}

static inline uint32_t __local_bswap32(uint32_t x) {
    return  ((x << 24) & 0xff000000 ) |
            ((x <<  8) & 0x00ff0000 ) |
            ((x >>  8) & 0x0000ff00 ) |
            ((x >> 24) & 0x000000ff );
}

static inline uint64_t __local_bswap64(uint64_t x) {
    return (uint64_t)__local_bswap32(x >> 32) |
          ((uint64_t)__local_bswap32(x & 0xFFFFFFFF) << 32);
}

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define be_dword(a) __local_bswap64(a)
#define be_word(a) __local_bswap32(a)
#define be_hword(a) __local_bswap16(a)
#define le_dword(a) (a)
#define le_word(a) (a)
#define le_hword(a) (a)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define be_dword(a) (a)
#define be_word(a) (a)
#define be_hword(a) (a)
#define le_dword(a) __local_bswap64(a)
#define le_word(a) __local_bswap32(a)
#define le_hword(a) __local_bswap16(a)
#else
#error "What's the endianness of the platform you're targeting?"
#endif
