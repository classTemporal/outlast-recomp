#!/usr/bin/env python3
"""
PE Analysis Script for Outlast (OLGame.exe)
Extracts headers, sections, imports, exports, and generates Phase 1 artifacts.
"""

import os
import sys
import json
import pefile

def analyze_pe(exe_path, output_dir):
    print(f"[*] Loading PE file: {exe_path}")
    pe = pefile.PE(exe_path)
    
    os.makedirs(output_dir, exist_ok=True)
    
    is_64bit = (pe.FILE_HEADER.Machine == pefile.MACHINE_TYPE['IMAGE_FILE_MACHINE_AMD64'])
    print(f"[*] Architecture: {'64-bit (x86_64)' if is_64bit else '32-bit (x86)'}")
    print(f"[*] Image Base: 0x{pe.OPTIONAL_HEADER.ImageBase:016X}")
    print(f"[*] Entry Point: 0x{pe.OPTIONAL_HEADER.AddressOfEntryPoint:08X}")
    print(f"[*] Number of Sections: {pe.FILE_HEADER.NumberOfSections}")
    
    # 1. Sections
    sections = []
    for s in pe.sections:
        sec_name = s.Name.decode('utf-8', errors='ignore').strip('\x00')
        sections.append({
            "name": sec_name,
            "virtual_address": f"0x{s.VirtualAddress:08X}",
            "virtual_size": f"0x{s.Misc_VirtualSize:08X}",
            "raw_size": f"0x{s.SizeOfRawData:08X}",
            "raw_pointer": f"0x{s.PointerToRawData:08X}",
            "characteristics": f"0x{s.Characteristics:08X}"
        })
        print(f"    Section: {sec_name:<8} VA: 0x{s.VirtualAddress:08X} VSize: 0x{s.Misc_VirtualSize:08X} RawSize: 0x{s.SizeOfRawData:08X}")

    # 2. Imports
    imports = {}
    total_imports = 0
    if hasattr(pe, 'DIRECTORY_ENTRY_IMPORT'):
        for entry in pe.DIRECTORY_ENTRY_IMPORT:
            dll_name = entry.dll.decode('utf-8', errors='ignore').lower()
            imports[dll_name] = []
            for imp in entry.imports:
                total_imports += 1
                func_name = imp.name.decode('utf-8', errors='ignore') if imp.name else f"Ordinal_{imp.ordinal}"
                imports[dll_name].append({
                    "name": func_name,
                    "address": f"0x{imp.address:016X}" if is_64bit else f"0x{imp.address:08X}",
                    "ordinal": imp.ordinal
                })
            print(f"    DLL: {dll_name:<25} ({len(imports[dll_name])} functions)")

    # Save imports.json
    imports_path = os.path.join(output_dir, "imports.json")
    with open(imports_path, "w") as f:
        json.dump(imports, f, indent=2)
    print(f"[*] Saved imports to: {imports_path} (Total imports: {total_imports})")
    
    # Save sections and metadata
    metadata = {
        "file": os.path.basename(exe_path),
        "is_64bit": is_64bit,
        "machine": pe.FILE_HEADER.Machine,
        "image_base": f"0x{pe.OPTIONAL_HEADER.ImageBase:016X}",
        "entry_point": f"0x{pe.OPTIONAL_HEADER.AddressOfEntryPoint:08X}",
        "subsystem": pe.OPTIONAL_HEADER.Subsystem,
        "sections": sections,
        "dll_count": len(imports),
        "total_import_count": total_imports
    }
    with open(os.path.join(output_dir, "metadata.json"), "w") as f:
        json.dump(metadata, f, indent=2)
        
    return metadata

if __name__ == "__main__":
    exe = sys.argv[1] if len(sys.argv) > 1 else "/home/agy2/nextcloud/backup/OLGame.exe"
    out = sys.argv[2] if len(sys.argv) > 2 else "tools/analyzer/output"
    analyze_pe(exe, out)
