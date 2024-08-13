#pragma once
#include "types.hpp"
#define _3GX_MAGIC (0x3230303024584733) /* "3GX$0002" */

struct _3gx_Infos {
    enum class Compatibility {
        CONSOLE = 0,
        CITRA = 1,
        CONSOLE_CITRA = 2
    };

    enum class MemorySize {
        _5MiB = 0,
        _2MiB = 1,
        _10MiB = 2,
        RESERVED = 3
    };

    u32 authorLen{0};
    u32 authorMsg{0};
    u32 titleLen{0};
    u32 titleMsg{0};
    u32 summaryLen{0};
    u32 summaryMsg{0};
    u32 descriptionLen{0};
    u32 descriptionMsg{0};

    union {
        u32 flags{0};

        struct {
            u32 embeddedExeDecryptFunc : 1;
            u32 embeddedSwapEncDecFunc : 1;
            u32 memoryRegionSize : 2;
            u32 compatibility : 2;
            u32 eventsSelfManaged : 1;
            u32 swapNotNeeded : 1;
            u32 unused : 24;
        };
    };

    u32 exeDecChecksum{0};
    u32 builtInDecExeArgs[4]{0};
    u32 builtInSwapEncDecArgs[4]{0};
} PACKED;

struct _3gx_Targets {
    u32 count{0};
    u32 titles{0};
} PACKED;

#ifndef BIT
#define BIT(x) (1u << x)
#endif

enum _3gx_SymFlags
{
    _3GX_SYM__DATA = BIT(0),
    _3GX_SYM__FUNC = BIT(1),
    _3GX_SYM__THUMB = BIT(2),
    _3GX_SYM__ALTNAME = BIT(3),
};

struct _3gx_Symbol {
    u32 address{0};
    u16 size{0};
    u16 flags{0};
    u32 nameOffset{0}; // Offset from start of name table
    _3gx_Symbol(u32 addr, u16 s, u16 f, u32 n) : address{addr}, size{s}, flags{f}, nameOffset{n} {}
} PACKED;

struct _3gx_Symtable
{
    u32  nbSymbols{0};
    u32  symbolsOffset{0};
    u32  nameTableOffset{0};
} PACKED;

struct _3gx_Executable {
    u32 codeOffset{0};
    u32 rodataOffset{0};
    u32 dataOffset{0};
    u32 codeSize{0};
    u32 rodataSize{0};
    u32 dataSize{0};
    u32 bssSize{0};
    u32 exeDecOffset{0}; // NOP terminated
    u32 swapEncOffset{0}; // NOP terminated
    u32 swapDecOffset{0}; // NOP terminated
} PACKED;

struct _3gx_Header {
    u64 magic{_3GX_MAGIC};
    u32 version{0};
    u32 reserved{0};
    _3gx_Infos infos{};
    _3gx_Executable executable{};
    _3gx_Targets targets{};
    _3gx_Symtable symtable{};
} PACKED;