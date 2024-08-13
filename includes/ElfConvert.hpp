// Based on code from https://github.com/devkitPro/3dstools
#pragma once
#include "types.hpp"
#include "elf.hpp"
#include "3gx.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

struct SegConv {
    u32 fileOff;
    u32 flags;
    u32 memSize;
    u32 fileSize;
    u32 memPos;
};

class ElfConvert {
public:
    ElfConvert(const string &elfPath);
    ~ElfConvert(void);
    void WriteToFile(_3gx_Header &header, ofstream &outFile, bool writeSymbols);

private:
    char *_img{nullptr};
    uint8_t *_binaryBuff{nullptr};
    int _platFlags{0};

    Elf32_Shdr *_elfSects{nullptr};
    int _elfSectCount{0};
    const char *_elfSectNames{nullptr};

    Elf32_Sym *_elfSyms{nullptr};
    int _elfSymCount{0};
    const char *_elfSymNames{nullptr};

    u32 _baseAddr{0};
    u32 _topAddr{0};

    const char *_codeSeg{nullptr};
    const char *_rodataSeg{nullptr};
    const char *_dataSeg{nullptr};
    u32 _codeSegSize{0};
    u32 _rodataSegSize{0};
    u32 _dataSegSize{0};
    u32 _bssSize{0};

    vector<_3gx_Symbol> _symbols;
    vector<char> _symbolsNames;

    void _GetSymbols(void);
    void _AddSymbol(Elf32_Sym *symbol, u16 flags);
};