#!/usr/bin/env python3
"""
Dump PE32+ (x86_64) facts, exports, imports, and functions.tsv (from .pdata) for recomp_lift.
"""

import os
import sys
import struct
import pefile

def dump_symbols_pe64(exe_path, out_dir="work/symbols"):
    os.makedirs(out_dir, exist_ok=True)
    print(f"[*] Parsing PE64 for symbols: {exe_path}")
    pe = pefile.PE(exe_path)
    image_base = pe.OPTIONAL_HEADER.ImageBase
    entry_point_va = image_base + pe.OPTIONAL_HEADER.AddressOfEntryPoint
    
    # 1. pe_info.txt
    with open(f"{out_dir}/pe_info.txt", "w") as f:
        f.write(f"image_base: 0x{image_base:016X}\n")
        f.write(f"entry_point: 0x{entry_point_va:016X}\n")
        f.write(f"subsystem: {pe.OPTIONAL_HEADER.Subsystem}\n")
        f.write("sections:\n")
        for s in pe.sections:
            name = s.Name.decode('utf-8', errors='ignore').strip('\x00')
            f.write(f"  {name} va: 0x{image_base + s.VirtualAddress:016X} size: 0x{s.Misc_VirtualSize:08X} raw: 0x{s.SizeOfRawData:08X}\n")
            
    # 2. imports.tsv
    with open(f"{out_dir}/imports.tsv", "w") as f:
        f.write("iat_va\tdll\tname_or_ordinal\n")
        if hasattr(pe, 'DIRECTORY_ENTRY_IMPORT'):
            for entry in pe.DIRECTORY_ENTRY_IMPORT:
                dll = entry.dll.decode('utf-8', errors='ignore').lower()
                for imp in entry.imports:
                    name = imp.name.decode('utf-8', errors='ignore') if imp.name else f"#{imp.ordinal}"
                    f.write(f"0x{imp.address:016X}\t{dll}\t{name}\n")
                    
    # 3. exports.tsv
    with open(f"{out_dir}/exports.tsv", "w") as f:
        f.write("va\tkind\tmangled\tdemangled\n")
        f.write(f"0x{entry_point_va:016X}\tfunc\tentry_point\tentry_point\n")
        if hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
            for exp in pe.DIRECTORY_ENTRY_EXPORT.symbols:
                name = exp.name.decode('utf-8', errors='ignore') if exp.name else f"Ordinal_{exp.ordinal}"
                va = image_base + exp.address
                f.write(f"0x{va:016X}\tfunc\t{name}\t{name}\n")

    # 4. functions.tsv from .pdata
    pdata_dir = pe.OPTIONAL_HEADER.DATA_DIRECTORY[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXCEPTION']]
    pdata_data = pe.get_data(pdata_dir.VirtualAddress, pdata_dir.Size)
    num_entries = len(pdata_data) // 12
    print(f"[*] Exporting {num_entries} functions from .pdata to {out_dir}/functions.tsv...")
    
    with open(f"{out_dir}/functions.tsv", "w") as f:
        # Format expected by recomp_lift:
        # col 0: entry VA (hex)
        # col 1: size
        # col 2: ...
        # col 5: is_noreturn ("0" or "1")
        # col 7: symbol name
        for i in range(num_entries):
            offset = i * 12
            begin_rva, end_rva, unwind_rva = struct.unpack_from("<III", pdata_data, offset)
            entry_va = image_base + begin_rva
            size = end_rva - begin_rva
            name = "entry_point" if entry_va == entry_point_va else f"sub_{entry_va:X}"
            f.write(f"0x{entry_va:016X}\t{size}\t0\t0\t0\t0\t0\t{name}\n")

    print(f"[*] Generated complete symbol suite in {out_dir}/")

if __name__ == "__main__":
    exe = sys.argv[1] if len(sys.argv) > 1 else "/home/agy2/nextcloud/backup/OLGame.exe"
    dump_symbols_pe64(exe)
