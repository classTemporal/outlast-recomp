#!/usr/bin/env python3
"""
Orchestration script for Outlast Static Recompilation Lifter (Phase 2).
Prepares symbols, runs recomp_lift in batched or full mode, and tracks lifted statistics.
"""

import os
import sys
import subprocess
import time

def run_lifter(exe_path, out_dir="gen", sym_dir="work/symbols"):
    print("=" * 60)
    print("Outlast Recompilation Pipeline: Phase 2 Static Lifting")
    print("=" * 60)
    
    lifter_bin = "tools/lifter/bin/recomp_lift"
    if not os.path.exists(lifter_bin):
        print(f"[!] Error: {lifter_bin} not found. Please build it first.")
        sys.exit(1)
        
    os.makedirs(out_dir, exist_ok=True)
    os.makedirs(sym_dir, exist_ok=True)
    
    # 1. Ensure symbols exist
    if not os.path.exists(f"{sym_dir}/functions.tsv"):
        print("[*] Generating symbol and function tables from PE32+...")
        subprocess.run(["/home/agy2/.venv/bin/python3", "tools/lifter/generate_symbols.py", exe_path], check=True)
        
    if not os.path.exists(f"{sym_dir}/jumptables.tsv"):
        open(f"{sym_dir}/jumptables.tsv", "a").close()
    if not os.path.exists("lifter.manifest"):
        open("lifter.manifest", "a").close()

    # 2. Run recomp_lift
    print(f"[*] Invoking x86_64 lifter on: {exe_path}")
    start_time = time.time()
    
    cmd = [
        lifter_bin,
        "--exe", exe_path,
        "--symbols", sym_dir,
        "--out", out_dir
    ]
    
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    elapsed = time.time() - start_time
    
    print(proc.stdout)
    if proc.returncode != 0:
        print(f"[!] Lifter failed with code {proc.returncode}")
        sys.exit(proc.returncode)
        
    print(f"[*] Static Lifting Completed successfully in {elapsed:.2f} seconds.")
    print(f"[*] Lifted C++ files are located in: {out_dir}/ (gitignored)")

if __name__ == "__main__":
    exe = sys.argv[1] if len(sys.argv) > 1 else "/home/agy2/nextcloud/backup/OLGame.exe"
    run_lifter(exe)
