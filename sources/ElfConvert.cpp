#include "ElfConvert.hpp"
#include <dynalo.hpp>
#include <cstring>
#include <iostream>
#include <algorithm>

#define die(msg) {throw runtime_error(msg);}
#define safe_call(a) do {int rc = a; if(rc != 0) return rc;} while(0)

using namespace std;

extern string g_enclibpath;
static const u8 defaultFunc[] = {0xC0, 0x40, 0x2D, 0xE9, 0x00, 0x70, 0xA0, 0xE3, 0x04, 0x60, 0x90, 0xE4, 0x06, 0x70, 0x87, 0xE0,
                                0x01, 0x00, 0x50, 0xE1, 0xFB, 0xFF, 0xFF, 0x1A, 0x07, 0x00, 0xA0, 0xE1, 0xC0, 0x80, 0xBD, 0xE8,
                                0x00, 0xF0, 0x20, 0xE3};

static u32 defaultFuncExec(void *data, u32 sizeBytes, u32 params[4]) {
    u32 ret = 0;
    u32 *d = (u32*)data;
    u32 *e = d + (sizeBytes / sizeof(u32));
    while (d < e) ret += *d++;
    return ret;
}

// Cannot be placed in the hpp file as dynalo can only be used on a single file
dynalo::library *_encLib{nullptr};

ElfConvert::ElfConvert(const string &elfPath) {
    u32 fileSize;
    Elf32_Ehdr *elfHdr;
    Elf32_Phdr *pHdr;
    ifstream ifile;
    ifile.open(elfPath, ios::in | ios::binary);

    if (!ifile.is_open())
        die("Couldn't open the file!");

    // Get elf file size
    ifile.seekg(0, ios::end);
    fileSize = (u32)ifile.tellg();
    ifile.seekg(0, ios::beg);

    // Read file
    _img = new char[fileSize];
    ifile.read(_img, fileSize);
    ifile.close();

    // Check file is ELF
    elfHdr = reinterpret_cast<Elf32_Ehdr *>(_img);

    if (memcmp(elfHdr->e_ident, ELF_MAGIC, 4) != 0)
        die("Invalid ELF file!");

    if (le_hword(elfHdr->e_type) != ET_EXEC)
        die("ELF file must be executable! (hdr->e_type should be ET_EXEC)");

    _elfSects = reinterpret_cast<Elf32_Shdr *>(_img + le_word(elfHdr->e_shoff));
    _elfSectCount = static_cast<int>(le_hword(elfHdr->e_shnum));
    _elfSectNames = reinterpret_cast<const char *>(_img + le_word(_elfSects[le_hword(elfHdr->e_shstrndx)].sh_offset));

    pHdr = reinterpret_cast<Elf32_Phdr *>(_img + le_word(elfHdr->e_phoff));
    _baseAddr = 1, _topAddr = 0;

    if (le_hword(elfHdr->e_phnum) > 3)
        die("Too many segments!");

    for (u32 i = 0; i < le_hword(elfHdr->e_phnum); ++i) {
        Elf32_Phdr *cur = pHdr + i;
        SegConv s;

        s.fileOff = le_word(cur->p_offset);
        s.flags = le_word(cur->p_flags);
        s.memSize = le_word(cur->p_memsz);
        s.fileSize = le_word(cur->p_filesz);
        s.memPos = le_word(cur->p_vaddr);

        if (!s.memSize)
            continue;

#ifdef DEBUG
        printf("PHDR[%d]: fOff(%X) memPos(%08X) memSize(%08X) fileSize(%08X) flags(%08X)\n",
            i, s.fileOff, s.memPos, s.memSize, s.fileSize, s.flags);
#endif

        if (i == 0)
            _baseAddr = s.memPos;

        else if (s.memPos != _topAddr)
            die("Non-contiguous segments!");

        if (s.memSize & 3)
            die("The segments is not word-aligned!");

        if (s.flags != 6 && s.memSize != s.fileSize)
            die("Only the data segment can have a BSS!");

        if (s.fileSize & 3)
            die("The loadable part of the segment is not word-aligned!");

        switch (s.flags) {
            case 5: // Code
                if (_codeSeg)
                    die("Too many code segments");

                if (_rodataSeg || _dataSeg)
                    die("Code segment must be the first");

                _codeSeg = _img + s.fileOff;
                _codeSegSize = s.memSize;
                break;

            case 4: // rodata
                if (_rodataSeg)
                    die("Too many rodata segments");

                if (_dataSeg)
                    die("Data segment must be before the code segment");

                _rodataSeg = _img + s.fileOff;
                _rodataSegSize = s.memSize;
                break;

            case 6: // data + bss
                if (_dataSeg)
                    die("Too many data segments");

                _dataSeg = _img + s.fileOff;
                _dataSegSize = s.fileSize;
                _bssSize = s.memSize - s.fileSize;
                break;

            default:
                die("Invalid segment!");
        }

        _topAddr = s.memPos + s.memSize + ((s.memSize & 3) ? 4 - (s.memSize & 3) : 0);
    }

    if ((_topAddr - _baseAddr) >= 0x200000)
        cout << "WARNING: The executable is bigger than 2 MiB! Only 3 MiB are left for heap memory, plugin may be unstable!" << endl;

    if (le_word(elfHdr->e_entry) != _baseAddr)
        die("Entrypoint should be zero!");

    _GetSymbols();
}

ElfConvert::~ElfConvert(void) {
    if (_img)
        delete[] _img;

    if (_binaryBuff)
        delete[] _binaryBuff;

    if (_encLib) {
        delete _encLib;
        _encLib = nullptr;
    }
}

void ElfConvert::WriteToFile(_3gx_Header &header, ofstream &outFile, bool writeSymbols) {
    _3gx_Infos &infos = header.infos;
    _3gx_Executable &exec = header.executable;
    _3gx_Symtable &symb = header.symtable;

    // Update header infos
    exec.codeSize = _codeSegSize;
    exec.rodataSize = _rodataSegSize;
    exec.dataSize = _dataSegSize;
    exec.bssSize = _bssSize;

    if (!g_enclibpath.empty())
        _encLib = new dynalo::library(g_enclibpath);

    // I could use _img directly, but I prefer not messing up something that I didn't make nor understand :P
    _binaryBuff = new uint8_t[_codeSegSize + _rodataSegSize + _dataSegSize];
    memcpy(_binaryBuff, _codeSeg, _codeSegSize);
    memcpy(_binaryBuff + _codeSegSize, _rodataSeg, _rodataSegSize);
    memcpy(_binaryBuff + _codeSegSize + _rodataSegSize, _dataSeg, _dataSegSize);

    u32 exeparams[4] = {0}, swapparams[4] = {0};
    u32 decExePayload[32] = {0}, decSwapPayload[32] = {0}, encSwapPayload[32] = {0};

    if (_encLib) { // Load the encrypt/decript lib
        auto encfunc = _encLib->get_function<uint32_t(void*, uint32_t, uint32_t[4])>("encrypt");
        auto embeddedDecFunc = _encLib->get_function<bool(uint32_t[32], uint32_t[4])>("decryptPayload");
        auto embeddedSwapEncDecFunc = _encLib->get_function<bool(uint32_t[32], uint32_t[32], uint32_t[4])>("encryptDecryptSwapPayload");

        infos.embeddedExeDecryptFunc = embeddedDecFunc(decExePayload, exeparams);
        infos.exeDecChecksum = encfunc(_binaryBuff, _codeSegSize + _rodataSegSize + _dataSegSize, exeparams);
        infos.embeddedSwapEncDecFunc = embeddedSwapEncDecFunc(encSwapPayload, decSwapPayload, swapparams);
    }

    else { // Use the default lib to get the checksum
        memcpy(decExePayload, defaultFunc, sizeof(defaultFunc));
        memcpy(decSwapPayload, defaultFunc, sizeof(defaultFunc));
        memcpy(encSwapPayload, defaultFunc, sizeof(defaultFunc));
        infos.embeddedExeDecryptFunc = infos.embeddedSwapEncDecFunc = 1;
        infos.exeDecChecksum = defaultFuncExec(_binaryBuff, _codeSegSize + _rodataSegSize + _dataSegSize, exeparams);
    }

    if (infos.embeddedExeDecryptFunc) {
        memcpy(infos.builtInDecExeArgs, exeparams, sizeof(infos.builtInDecExeArgs));
        exec.exeDecOffset = static_cast<u32>(outFile.tellp());
        u32 payloadSize = 1;

        for (; payloadSize <= (sizeof(decExePayload) / sizeof(u32)); payloadSize++) {
            if (decExePayload[payloadSize - 1] == 0xE320F000)
                break;
        }

        if (payloadSize > (sizeof(decExePayload) / sizeof(u32)))
            die("Decryption payload is too long or not \"NOP\" terminated.");

        outFile.write((char*)decExePayload, payloadSize * sizeof(u32));
        outFile.flush();
    }

    if (infos.embeddedSwapEncDecFunc) {
        memcpy(infos.builtInSwapEncDecArgs, swapparams, sizeof(infos.builtInSwapEncDecArgs));
        exec.swapEncOffset = static_cast<u32>(outFile.tellp());
        u32 payloadSize = 1;

        for (; payloadSize <= (sizeof(encSwapPayload) / sizeof(u32)); payloadSize++) {
            if (encSwapPayload[payloadSize - 1] == 0xE320F000)
                break;
        }

        if (payloadSize > (sizeof(encSwapPayload) / sizeof(u32)))
            die("Swap ecryption payload is too long or not \"NOP\" terminated.");

        outFile.write((char*)encSwapPayload, payloadSize*sizeof(u32));
        outFile.flush();
        exec.swapDecOffset = static_cast<u32>(outFile.tellp());
        payloadSize = 1;

        for (; payloadSize <= (sizeof(decSwapPayload) / sizeof(u32)); payloadSize++) {
            if (decSwapPayload[payloadSize - 1] == 0xE320F000)
                break;
        }

        if (payloadSize > (sizeof(decSwapPayload) / sizeof(u32)))
            die("Swap decryption payload is too long or not \"NOP\" terminated.");

        outFile.write((char*)decSwapPayload, payloadSize*sizeof(u32));
        outFile.flush();
    }

    // Write code to file
    exec.codeOffset = static_cast<u32>(outFile.tellp()); { // Make the offset in file 16 bytes aligned
        u32 padding = 16 - (exec.codeOffset & 0xF);
        char zeroes[16] = {0};
        outFile.write(zeroes, padding);
        exec.codeOffset += padding;
    }

    outFile.write((char*)_binaryBuff, _codeSegSize);
    outFile.flush();

    // Write rodata to file
    exec.rodataOffset = static_cast<u32>(outFile.tellp());
    outFile.write((char*)_binaryBuff + _codeSegSize, _rodataSegSize);
    outFile.flush();

    // Write data to file
    exec.dataOffset = static_cast<u32>(outFile.tellp());
    outFile.write((char*)_binaryBuff + _codeSegSize + _rodataSegSize, _dataSegSize);
    outFile.flush();

    if (!writeSymbols) {
        symb.nbSymbols = 0;
        symb.symbolsOffset = 0;
        symb.nameTableOffset = 0;
        return;
    }

    // Write symbols to file
    symb.nbSymbols = _symbols.size();
    symb.symbolsOffset = static_cast<u32>(outFile.tellp());
    outFile.write(reinterpret_cast<char *>(_symbols.data()), sizeof(_3gx_Symbol) * _symbols.size());
    outFile.flush();

    // Write symbols name to file
    symb.nameTableOffset = static_cast<u32>(outFile.tellp());
    const char *name = _symbolsNames.data();

    for (const _3gx_Symbol &sym : _symbols)
        outFile << string(name + le_word(sym.nameOffset)) << '\0';

    outFile.flush();
}

void ElfConvert::_GetSymbols(void) {
    for (u32 i = 0; i < _elfSectCount; ++i) {
        Elf32_Shdr *sect = _elfSects + i;

        switch (le_word(sect->sh_type)) {
            case SHT_SYMTAB:
                _elfSyms = reinterpret_cast<Elf32_Sym *>(_img + le_word(sect->sh_offset));
                _elfSymCount = le_word(sect->sh_size) / sizeof(Elf32_Sym);
                _elfSymNames = reinterpret_cast<const char *>(_img + le_word(_elfSects[le_word(sect->sh_link)].sh_offset));

                Elf32_Sym *sym = _elfSyms;
                vector<Elf32_Sym *> symbols;

                for (u32 i = 0; i < _elfSymCount; ++i, ++sym) {
                    // Skip FILE symbols
                    if (ELF32_ST_TYPE(sym->st_info) == STT_FILE)
                        continue;

                    // Skip SECTION symbols
                    if (ELF32_ST_TYPE(sym->st_info) == STT_SECTION)
                        continue;

                    symbols.push_back(sym);
                }

                // Sort symbols by VA
                sort(symbols.begin(), symbols.end(), [this](Elf32_Sym *left, Elf32_Sym *right) {
                    // Make sure for the descriptor to be before the actual symbol
                    // eg: $a then _myFunc
                    return le_word(left->st_value) < le_word(right->st_value);
                });

                // Ensure there's no duplicate (should probably not be necessary, but just in case and it doesn't take that much execution time)
                symbols.erase(unique(symbols.begin(), symbols.end(), [this](Elf32_Sym *left, Elf32_Sym *right) {
                    return left->st_value == right->st_value
                        && left->st_size == right->st_size
                        && left->st_info == right->st_info
                        && left->st_other == right->st_other
                        && !strcmp(_elfSymNames + le_word(left->st_name), _elfSymNames + le_word(right->st_name));
                }), symbols.end());

                // Convert symbols to 3GX symbol types
                u32 type = 0;
                u32 lastAddr = 0;

                for (auto it = symbols.begin(); it != symbols.end();) {
                    Elf32_Sym *symbol = *it++;
                    const char *name = _elfSymNames + le_word(symbol->st_name);

                    if (name[0] == '$' && name[2] == '\0') {
                        // Skip specifier when it's not relevant
                        if (le_word(symbol->st_value) != le_word((*it)->st_value))
                            continue;

                        switch (name[1]) {
                            case 'a': ///< label a list of instructions
                            case 'p': ///< label
                                type = _3GX_SYM__FUNC;
                                break;
                            case 'b': ///< label a bl Thumb
                            case 't': ///< label a list of Thumb instructions
                                type = _3GX_SYM__FUNC | _3GX_SYM__THUMB;
                                break;
                            case 'd':
                                type = _3GX_SYM__DATA;
                                break;
                        }

                        continue;
                    }

                    _AddSymbol(symbol, type | (lastAddr == le_word(symbol->st_value) ? _3GX_SYM__ALTNAME : 0));
                    lastAddr = le_word(symbol->st_value);
                    type = 0;
                }

                break;
        }
    }

    if (!_elfSyms)
        die("ELF has no symbol table!");
}

void ElfConvert::_AddSymbol(Elf32_Sym *symbol, u16 flags) {
    _symbols.emplace_back(le_word(symbol->st_value),
                          le_word(symbol->st_size),
                          le_hword(flags),
                          le_word(_symbolsNames.size()));

    const char *name = _elfSymNames + le_word(symbol->st_name);

    while (*name)
        _symbolsNames.push_back(*name++);
    _symbolsNames.push_back(0);
}