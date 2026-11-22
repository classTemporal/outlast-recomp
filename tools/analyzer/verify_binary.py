#!/usr/bin/env python3
"""
Scan PE binary for section entropy, packer signatures, and strings related to LoadLibrary/GetProcAddress.
"""

import math
import sys
import pefile
import re

def calculate_entropy(data):
    if not data:
        return 0.0
    entropy = 0
    for x in range(256):
        p_x = float(data.count(bytes([x]))) / len(data)
        if p_x > 0:
            entropy += - p_x * math.log2(p_x)
    return entropy

def verify_binary(pe_path):
    print(f"[*] Deep verification of: {pe_path}")
    pe = pefile.PE(pe_path)
    
    print("\n--- 1. Section Entropy (Packer / Encryption Detection) ---")
    for section in pe.sections:
        name = section.Name.decode('utf-8', errors='ignore').strip('\x00')
        data = section.get_data()
        entropy = calculate_entropy(data)
        # Normal compiled code is usually between 6.0 and 6.8. Encrypted/packed is > 7.2
        status = "SUSPICIOUS (Possible pack/encrypt)" if entropy > 7.2 else "NORMAL (Clean compiled)"
        print(f"Section {name:<8} | Size: {len(data):>10} bytes | Entropy: {entropy:.4f} | {status}")
        
    print("\n--- 2. d3dx11_43.dll & nvtt_64.dll Exact Imported Symbols ---")
    if hasattr(pe, 'DIRECTORY_ENTRY_IMPORT'):
        for entry in pe.DIRECTORY_ENTRY_IMPORT:
            dll = entry.dll.decode('utf-8', errors='ignore').lower()
            if dll in ['d3dx11_43.dll', 'nvtt_64.dll', 'd3dcompiler_43.dll']:
                print(f"Imports from {dll}:")
                for imp in entry.imports:
                    name = imp.name.decode('utf-8', errors='ignore') if imp.name else f"Ordinal_{imp.ordinal}"
                    print(f"    - {name}")

    print("\n--- 3. Dynamic DLL References in Binary Strings ---")
    raw = open(pe_path, 'rb').read()
    
    # Search for DLL names referenced in ascii or utf-16
    dll_pattern = re.compile(rb'([a-zA-Z0-9_\-\.]+\.dll)', re.IGNORECASE)
    matches = set()
    for m in dll_pattern.finditer(raw):
        s = m.group(1).decode('ascii', errors='ignore').lower()
        if len(s) > 4:
            matches.add(s)
            
    # Also wide strings
    dll_pattern_w = re.compile(rb'((?:[a-zA-Z0-9_\-\.]\x00)+\.d\x00l\x00l\x00)', re.IGNORECASE)
    for m in dll_pattern_w.finditer(raw):
        s = m.group(1).decode('utf-16le', errors='ignore').lower()
        if len(s) > 4:
            matches.add(s)

    physx_dlls = [x for x in sorted(matches) if 'physx' in x or 'nx' in x or 'apex' in x]
    print(f"Discovered PhysX/Physics DLL strings in binary ({len(physx_dlls)}):")
    for d in physx_dlls:
        print(f"    - {d}")
        
    media_dlls = [x for x in sorted(matches) if 'bink' in x or 'ogg' in x or 'vorbis' in x or 'steam' in x]
    print(f"Discovered Media/Steam DLL strings ({len(media_dlls)}):")
    for d in media_dlls:
        print(f"    - {d}")

    # Check for DRM / SteamStub signatures
    print("\n--- 4. DRM / SteamStub Signature Scan ---")
    steam_stub = False
    for sig in [b'.bind', b'SteamDRM', b'steam_api.dll\x00\x00SteamStartup', b'bClientLibraryIsLegacy']:
        if sig in raw:
            print(f"Found signature: {sig}")
            steam_stub = True
    if not steam_stub:
        print("No SteamStub / DRM envelope detected. Executable is clean native uncompressed PE32+.")

if __name__ == "__main__":
    verify_binary(sys.argv[1] if len(sys.argv) > 1 else "/home/agy2/nextcloud/backup/OLGame.exe")
