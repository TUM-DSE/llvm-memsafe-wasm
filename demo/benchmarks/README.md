# Benchmarking

For creating some benchmarks for my B.Sc., I have used this python script.
You can find more information on the input code, and why I chose these benchmarks, by reading the `Evaluation` of my thesis at: https://github.com/TUM-DSE/research-work-archive/blob/main/archive/2023/summer/docs/bsc_rehde_hardware_assisted_memory_safety_for_webassembly.pdf

# Convert Input C/LLVM IR Code to WASM

To generate the input .wasm code files that are later provided to the python script, you will first have to compile the C or LLVM IR files with clang to wasm64.
Note that you should change some of the paths in the Makefiles, since they need to point to e.g. our custom clang or wasm_memsafety_rtlib.c.
Also note that the PolybenchC .wasm files should be generated elsewhere, this only contains some smaller custom tests.

# Performing Benchmarks with Python Script

This script can execute benchmarks, and also evaluate them (i.e. process data and create a matplotlib graph).

The way this script functions is that it expects you to first create a `base_dir`, a working dir in which you place input (.wasm) files and where output files will be saved.

Expected (input) file structure to run benchmarks:
- The `{base_dir}/wasmtime` directory should contain all wasmtime variants (e.g. `{base_dir}/wasmtime/wasmtime-mte-naive`) that you later specify on the command-line.
- The `{base_dir}/{benchmark_suite}/build` directory should contain all input wasm files in subdirectories named after the wasmtime variant (e.g. `{base_dir}/{benchmark_suite}/build/wasmtime-mte-naive`).

Note that this python script will have to be executed in QEMU with MTE enabled, since it is meant to run MTE enabled wasmtime binaries.

To see all possible and required command-line eoptions, just run the script with no options like `python3 benchmarks-script.py`.
