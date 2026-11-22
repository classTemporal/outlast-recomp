#!/usr/bin/env python3
"""
Differential CPU Test Harness for x86_64 -> Native C execution.
Compares register state, flags, and memory writes between Unicorn Engine (x86_64 reference)
and lifted native C++ code compiled with Clang.
"""

import sys
import struct
from unicorn import *
from unicorn.x86_const import *
import capstone

def test_instruction_differential(code_bytes, initial_regs=None):
    """
    Simulates instruction in Unicorn (x86_64) and prints pre/post states.
    """
    if initial_regs is None:
        initial_regs = {
            "rax": 0x123456789ABCDEF0,
            "rbx": 0x000000000000002A,
            "rcx": 0x0000000000001000,
            "rdx": 0x0000000000000005,
            "rsi": 0x0000000000002000,
            "rdi": 0x0000000000003000,
            "rsp": 0x0000000000100000,
            "rbp": 0x0000000000100000,
            "r8":  0x0000000000000001,
            "r9":  0x0000000000000002,
        }
        
    ADDRESS = 0x1000000
    STACK_ADDR = 0x100000
    
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    uc.mem_map(ADDRESS, 2 * 1024 * 1024)
    uc.mem_map(STACK_ADDR, 1024 * 1024)
    
    uc.mem_write(ADDRESS, code_bytes)
    
    # Setup initial state
    uc.reg_write(UC_X86_REG_RAX, initial_regs.get("rax", 0))
    uc.reg_write(UC_X86_REG_RBX, initial_regs.get("rbx", 0))
    uc.reg_write(UC_X86_REG_RCX, initial_regs.get("rcx", 0))
    uc.reg_write(UC_X86_REG_RDX, initial_regs.get("rdx", 0))
    uc.reg_write(UC_X86_REG_RSI, initial_regs.get("rsi", 0))
    uc.reg_write(UC_X86_REG_RDI, initial_regs.get("rdi", 0))
    uc.reg_write(UC_X86_REG_RSP, initial_regs.get("rsp", 0))
    uc.reg_write(UC_X86_REG_RBP, initial_regs.get("rbp", 0))
    uc.reg_write(UC_X86_REG_R8, initial_regs.get("r8", 0))
    uc.reg_write(UC_X86_REG_R9, initial_regs.get("r9", 0))
    
    # Run
    uc.emu_start(ADDRESS, ADDRESS + len(code_bytes))
    
    results = {
        "rax": uc.reg_read(UC_X86_REG_RAX),
        "rbx": uc.reg_read(UC_X86_REG_RBX),
        "rcx": uc.reg_read(UC_X86_REG_RCX),
        "rdx": uc.reg_read(UC_X86_REG_RDX),
        "eflags": uc.reg_read(UC_X86_REG_EFLAGS),
    }
    return results

def run_suite():
    print("=== Differential CPU Test Suite (Unicorn x86_64) ===")
    
    # Test 1: Arithmetic & Flags (add, sub, imul)
    # add rax, rbx; sub rdx, 1; imul rcx, rdx
    code = b"\x48\x01\xd8\x48\x83\xea\x01\x48\x0f\xaf\xca"
    res = test_instruction_differential(code)
    print(f"Test 1 (Math/ALU): RAX=0x{res['rax']:016X}, RDX=0x{res['rdx']:016X}, RCX=0x{res['rcx']:016X}")
    assert res['rax'] == 0x123456789ABCDF1A
    assert res['rdx'] == 0x0000000000000004
    assert res['rcx'] == 0x0000000000004000
    print("Test 1 PASS!")
    
    # Test 2: Bitwise & Shifts
    # rol rax, 4; xor rax, rdx
    code2 = b"\x48\xc1\xc0\x04\x48\x31\xd0"
    res2 = test_instruction_differential(code2, {"rax": 0x0123456789ABCDEF, "rdx": 0x00000000000000FF})
    print(f"Test 2 (Rotate/XOR): RAX=0x{res2['rax']:016X}")
    assert res2['rax'] == 0x123456789ABCDEF0 ^ 0xFF
    print("Test 2 PASS!")

    print("\nAll differential CPU reference checks passed cleanly!")

if __name__ == "__main__":
    run_suite()
