import argparse
import subprocess
import os
from datetime import datetime
from concurrent.futures import ThreadPoolExecutor
import csv
import statistics

# Expected (input) file structure to run benchmarks:
# - The `{base_dir}/wasmtime` directory should contain all wasmtime variants (e.g. `{base_dir}/wasmtime/wasmtime-mte-naive`)
# - The `{base_dir}/{benchmark_suite}/build` directory should contain all input wasm files in subdirectories named after the wasmtime variant (e.g. `{base_dir}/{benchmark_suite}/build/wasmtime-mte-naive`)

# Global state
base_dir = None
benchmark_suite = None
wasm_tools_binary = None
results_dir = None
wasm_dir = None
cwasm_dir = None
all_processed_results_dir = None
all_final_results_dir = None
chart_results_dir = None
wasmtime_binaries_dir = None

# CLI INTERFACE
def main():
    parser = argparse.ArgumentParser(
        description="Execute and evaluate benchmarks for the SafeWASM project")

    all_wasmtime_binaries = ["wasmtime-none", "wasmtime-mte-infra-only", "wasmtime-mte-no-tagging", "wasmtime-mte-naive", "wasmtime-mte-opt", "wasmtime-mte-opt-static", "wasmtime-mte-async", "wasmtime-pac"]
    default_wasmtime_binaries = [x for x in all_wasmtime_binaries if x == "wasmtime-pac"]

    parser.add_argument('--base-dir', required=True,
                        help='The base/working directory, in which input files are expected and output files will be written')

    parser.add_argument('--benchmark-suite', choices=['polybench', 'sorting', 'pac', 'memory-tagging'], required=True,
                        help='The benchmark suite to run')

    parser.add_argument('--wasmtime-binaries', nargs='+', choices=all_wasmtime_binaries, required=True,
                        help='All wasmtime executables/binaries for which the benchmarks should be executed')

    parser.add_argument('--wasm-tools-binary',
                        help='A path to the wasm tools binary required for counting new instructions in .wasm')

    parser.add_argument('--compile', action='store_true',
                        help='Compile all .wasm files to .cwasm using the specified wasmtime binaries')

    parser.add_argument('--performance', action='store_true',
                        help='Execute all performance benchmarks using the specified wasmtime binaries')

    parser.add_argument('--binary-size', action='store_true',
                        help='Execute all binary size benchmarks using the specified wasmtime binaries')

    parser.add_argument('--count-insts', action='store_true',
                        help='Execute all instruction counting benchmarks using the specified wasmtime binaries')

    parser.add_argument('--evaluate', action='store_true',
                        help='Evaluate the raw benchmark data, i.e. process it and generate a chart')

    args = parser.parse_args()

    # Initialize global state
    global base_dir, benchmark_suite, wasm_tools_binary, results_dir, wasm_dir, cwasm_dir, all_processed_results_dir, all_final_results_dir, chart_results_dir, wasmtime_binaries_dir

    base_dir = args.base_dir
    benchmark_suite = args.benchmark_suite
    wasm_tools_binary = args.wasm_tools_binary

    # Directories where input files are read and output files are written to
    results_dir = os.path.join(base_dir, f'{benchmark_suite}/results')
    wasm_dir = os.path.join(base_dir, f'{benchmark_suite}/build')
    cwasm_dir = os.path.join(base_dir, f'{benchmark_suite}/compiled')
    all_processed_results_dir = os.path.join(base_dir, f'{benchmark_suite}/all-processed-results')
    all_final_results_dir = os.path.join(base_dir, f'{benchmark_suite}/all-final-results')
    chart_results_dir = os.path.join(base_dir, f'{benchmark_suite}/chart-results')
    wasmtime_binaries_dir = os.path.join(base_dir, f'wasmtime')

    # Create all directories so they be filled later
    [os.makedirs(dir, exist_ok=True) for dir in [results_dir, wasm_dir, cwasm_dir,
                                                all_processed_results_dir, all_final_results_dir, chart_results_dir]]


    for wasmtime_executable in args.wasmtime_binaries:
        if args.compile:
            compile_all(wasmtime_executable)

        if args.performance:
            match benchmark_suite:
                case "sorting":
                    complete_perf_sorting_run(wasmtime_executable)
                case _:
                    complete_perf_run(wasmtime_executable)
        
        if args.binary_size:
            binary_size_complete_run(wasmtime_executable)

        if args.count_insts:
            count_new_instructions_complete_run(wasmtime_executable)

    if args.evaluate:
        generate_all_processed_results()
        generate_all_final_results()
        generate_all_charts()


# Unfortunately, branch-misses, branches, cache-misses are all not supported inside qemu
perf_metrics = "cycles,task-clock,page-faults,context-switches"

# Number of times to run the command
num_runs = 5


def get_wasm_files(wasmtime_executable):
    """Helper to return a list of all input wasm files belonging to a wasmtime variant"""

    sub_dir = os.path.join(wasm_dir, wasmtime_executable)
    return [os.path.join(sub_dir, file) for file in os.listdir(sub_dir)]


def get_cwasm_files(wasmtime_executable):
    """Helper to return a list of all input (pre-compiled) cwasm files belonging to a wasmtime variant"""

    sub_dir = os.path.join(cwasm_dir, wasmtime_executable)
    return [os.path.join(sub_dir, file) for file in os.listdir(sub_dir)]


def get_wasmtime_command(wasmtime_executable, command, extra_options, file):
    """Helper to add all necessary flags to a wasmtime invocation (command can be 'run' or 'compile')"""

    if wasmtime_executable == 'wasmtime-none':
        wasmtime_command = [os.path.join(wasmtime_binaries_dir, wasmtime_executable),
                            command,
                            "--wasm-features=memory64"] + extra_options + [file]
    else:
        wasmtime_command = [os.path.join(wasmtime_binaries_dir, wasmtime_executable),
                            command, "--cranelift-enable", "use_mte", 
                            "--wasm-features=memory64,mem-safety"] + extra_options + [file]
    return wasmtime_command


# == EXECUTING BENCHMARKS

# === COMPILE WASM TO CWASM
def compile_file(wasmtime_executable, wasm_file):
    """Compiles the input wasm file to a cwasm file using the defined wasmtime binary"""

    wasmtime_compile_command = get_wasmtime_command(
        wasmtime_executable, 'compile', [], wasm_file)

    print(f"Executing: {wasmtime_compile_command}")

    subprocess.run(wasmtime_compile_command)


def compile_all(wasmtime_executable):
    """Compiles all wasm files, which belong to a certain wasmtime executable, to cwasm files"""

    wasm_files = get_wasm_files(wasmtime_executable)

    dst_dir = os.path.join(cwasm_dir, wasmtime_executable)
    os.makedirs(dst_dir, exist_ok=True)
    os.chdir(dst_dir)

    with ThreadPoolExecutor(max_workers=len(wasm_files)) as executor:
        for wasm_file in wasm_files:
            executor.submit(compile_file, wasmtime_executable, wasm_file)


# === PERFORMANCE (CPU CYCLES) BENCHMARKING
def single_perf_run(iteration, wasmtime_executable, cwasm_file):
    """Perform a single perf benchmark run. This function simply executes the cwasm binary, without any extra stdin/arguments provided to it"""

    perf_results_dir = os.path.join(results_dir, 'perf')
    dst_dir = os.path.join(perf_results_dir, wasmtime_executable)
    os.makedirs(dst_dir, exist_ok=True)

    dst_file = f'perf__{wasmtime_executable}__{os.path.basename(cwasm_file)}__iter_{str(iteration)}.txt'
    perf_results_file = os.path.join(dst_dir, dst_file)

    # Construct the perf command
    wasmtime_run_command = get_wasmtime_command(
        wasmtime_executable, 'run', ['--allow-precompiled'], cwasm_file)
    perf_command = ["perf", "stat", "-e", perf_metrics,
                    "-o", perf_results_file, "--"] + wasmtime_run_command

    current_time = datetime.now()
    print(f'Time     : {current_time}')
    print(f"Executing: {perf_command}")

    subprocess.run(perf_command)

    print(f"Results  :")
    with open(perf_results_file, 'r') as file:
        print(file.read())


def complete_perf_run(wasmtime_executable):
    """Use a specified wasmtime binary to run all benchmarks"""

    cwasm_files = get_cwasm_files(wasmtime_executable)

    for cwasm_file in cwasm_files:
        print(f"=== Combination: {wasmtime_executable} {cwasm_file}")

        for i in range(num_runs):
            single_perf_run(i, wasmtime_executable, cwasm_file)


# === BINARY SIZE BENCHMARKING
def binary_size_complete_run(wasmtime_executable):
    """Find the binary sizes of .wasm and .cwasm files for a certain wasmtime binary"""

    dst_dir_binary_sizes = os.path.join(results_dir, 'binary-sizes')
    os.makedirs(dst_dir_binary_sizes, exist_ok=True)

    # Get WASM (.wasm) binary sizes
    wasm_dir = os.path.join(dst_dir_binary_sizes, 'wasm', wasmtime_executable)
    os.makedirs(wasm_dir, exist_ok=True)

    for wasm_file in get_wasm_files(wasmtime_executable):
        file_size = os.path.getsize(wasm_file)

        output_filename = f"binary_size__wasm__{wasmtime_executable}__{os.path.basename(wasm_file)}.txt"
        output_path = os.path.join(wasm_dir, output_filename)

        print(f'Calculating wasm binary size: {output_path}')

        with open(output_path, 'w') as f:
            f.write(str(file_size))

    # Get AArch64 binary (.cwasm) sizes
    cwasm_dir = os.path.join(dst_dir_binary_sizes,
                             'cwasm', wasmtime_executable)
    os.makedirs(cwasm_dir, exist_ok=True)

    for cwasm_file in get_cwasm_files(wasmtime_executable):
        file_size = os.path.getsize(cwasm_file)

        output_filename = f"binary_size__cwasm__{wasmtime_executable}__{os.path.basename(cwasm_file)}.txt"
        output_path = os.path.join(cwasm_dir, output_filename)

        print(f'Calculating cwasm binary size: {output_path}')

        with open(output_path, 'w') as f:
            f.write(str(file_size))


# === NUMBER OF NEW WASM INSTRUCTIONS
def count_new_instructions_complete_run(wasmtime_executable):
    """Count the number of new instructions in the .wasm and .cwasm files. 'New' instructions are those from our extended wasm (e.g. segment.new, segment.free etc.)"""

    dst_dir = os.path.join(results_dir, 'count-insts')
    os.makedirs(dst_dir, exist_ok=True)

    # WASM (.wasm)
    wasm_dir = os.path.join(dst_dir, 'wasm', wasmtime_executable)
    os.makedirs(wasm_dir, exist_ok=True)

    for wasm_file in get_wasm_files(wasmtime_executable):
        # Convert .wasm to .wast
        wasm_output = subprocess.check_output(
            ['{wasm_tools_binary}', 'print', wasm_file], text=True)

        # Define a dictionary to store instruction counts
        instruction_counts = {
            "segment.new": wasm_output.count("segment.stack_new"),
            "segment.free": wasm_output.count("segment.free") + wasm_output.count("segment.stack_free"),
            "i64.pointer_sign": wasm_output.count("i64.pointer_sign"),
            "i64.pointer_auth": wasm_output.count("i64.pointer_auth")
        }

        # Save results to file
        output_filename = f"count_wasm__{wasmtime_executable}__{os.path.basename(wasm_file)}.txt"
        output_path = os.path.join(wasm_dir, output_filename)

        print(f'Counting wasm insts: {output_path}')

        with open(output_path, 'w') as f:
            for instruction, count in instruction_counts.items():
                f.write(f"{instruction}: {count}\n")

    # AArch64 binary (.cwasm)
    cwasm_dir = os.path.join(dst_dir, 'cwasm', wasmtime_executable)
    os.makedirs(cwasm_dir, exist_ok=True)

    for cwasm_file in get_cwasm_files(wasmtime_executable):
        # Convert .wasm to .wast
        objdump_output = subprocess.check_output(
            ['objdump', '-D', cwasm_file], text=True)

        # Define a dictionary to store instruction counts
        instruction_counts = {
            "irg": objdump_output.count("irg\t"),
            "stg": objdump_output.count("stg\t"),
            "st2g": objdump_output.count("st2g\t"),
            "pacdza": objdump_output.count("pacdza\t"),
            "autdza": objdump_output.count("autdza\t")
        }

        # Save results to file
        output_filename = f"count_cwasm__{wasmtime_executable}__{os.path.basename(cwasm_file)}.txt"
        output_path = os.path.join(cwasm_dir, output_filename)

        print(f'Counting cwasm insts: {output_path}')

        with open(output_path, 'w') as f:
            for instruction, count in instruction_counts.items():
                f.write(f"{instruction}: {count}\n")


# === SORTING BENCHMARKING
# Unsorted input array size
unsorted_input_array_size = 40000
data = list(range(unsorted_input_array_size, 0, -1))

# TODO: we could abstract this (by passing the stdin and argumens as parameters), since most of this code is dupliated
def single_perf_sorting_run(iteration, wasmtime_executable, cwasm_file):
    """This is a special version of the performance benchmark, because it provides some arguments and stdin to the wasmtime call"""

    perf_results_dir = os.path.join(results_dir, 'perf')
    dst_dir = os.path.join(perf_results_dir, wasmtime_executable)
    os.makedirs(dst_dir, exist_ok=True)

    dst_file = f'perf__{wasmtime_executable}__{os.path.basename(cwasm_file)}__iter_{str(iteration)}.txt'
    perf_results_file = os.path.join(dst_dir, dst_file)

    # Convert the list of integers to a string and then encode it to bytes
    input_data = "\n".join(map(str, data)) + "\n"

    # Construct the perf command
    wasmtime_command = get_wasmtime_command(wasmtime_executable, 'run', [
                                            '--allow-precompiled'], cwasm_file) + [str(len(data))]
    perf_command = ["perf", "stat", "-e", perf_metrics,
                    "-o", perf_results_file, "--"] + wasmtime_command

    current_time = datetime.now()
    print(f'Time     : {current_time}')
    print(f"Executing: {perf_command}")

    # Run the sorter binary with array length as an argument and input_data as stdin
    result = subprocess.run(perf_command, input=input_data,
                            text=True, capture_output=True)

    # Check for errors
    if result.returncode != 0:
        print(f"Error: {result.stderr}")

    print(f"Results  :")
    with open(perf_results_file, 'r') as file:
        print(file.read())


def complete_perf_sorting_run(wasmtime_executable):
    """Performan all sorting benchmarks"""

    cwasm_files = get_cwasm_files(wasmtime_executable)

    for cwasm_file in cwasm_files:
        print(f"Performance benchmarking: {wasmtime_executable} {cwasm_file}")

        for i in range(num_runs):
            single_perf_sorting_run(i, wasmtime_executable, cwasm_file)


# NOTE: in tests, this didn't lead to expected results (all memory usage was basically equal, even though we'd expect MTE to use more. Maybe this is a problem with QEMU+MTE)
# == PEAK MEMORY USED
def single_memory_sorting_run(iteration, wasmtime_executable, cwasm_file):
    """Measure the peak memory used by the input program"""

    memory_results_dir = os.path.join(results_dir, 'memory')
    dst_dir = os.path.join(memory_results_dir, wasmtime_executable)
    os.makedirs(dst_dir, exist_ok=True)

    dst_file = f'memory__{wasmtime_executable}__{os.path.basename(cwasm_file)}__iter_{str(iteration)}.txt'
    memory_results_file = os.path.join(dst_dir, dst_file)

    # Convert the list of integers to a string and then encode it to bytes
    input_data = "\n".join(map(str, data)) + "\n"

    # Construct the time command
    wasmtime_command = get_wasmtime_command(wasmtime_executable, 'run', [
                                            '--allow-precompiled'], cwasm_file) + [str(len(data))]
    time_command = ["/usr/bin/time", "-v", "-o",
                    memory_results_file] + wasmtime_command

    current_time = datetime.now()
    print(f'Time     : {current_time}')
    print(f"Executing: {time_command}")

    # Run the sorter binary with array length as an argument and input_data as stdin
    result = subprocess.run(time_command, input=input_data,
                            text=True, capture_output=True)

    # Check for errors
    if result.returncode != 0:
        print(f"Error: {result.stderr}")

    print(f"Results  :")
    with open(memory_results_file, 'r') as file:
        print(file.read())


def complete_memory_sorting_run(wasmtime_executable):
    """Measure the peak memory for all wasm programs belonging to a wasmtime binary"""

    cwasm_files = get_cwasm_files(wasmtime_executable)

    for cwasm_file in cwasm_files:
        print(f"Memory benchmarking: {wasmtime_executable} {cwasm_file}")

        for i in range(num_runs):
            single_memory_sorting_run(i, wasmtime_executable, cwasm_file)


# == PROCESSING BENCHMARKS

def get_input_programs():
    match benchmark_suite:
        case "sorting":
            return ["bubble_sort",
                              "merge_sort", "modified_merge_sort"]
        case "pac":
            return ["pac-1", "pac-2", "pac-3", "pac-4", "pac-5"]
        case "memory-tagging":
            return ["tagging-few-loops-large-segments", "tagging-few-loops-small-segments",
                              "tagging-many-loops-small-segments", "tagging-many-loops-large-segments"]
        case "polybench":
            return [
                "correlation-san",
                "2mm-san",
                "deriche-san",
                "adi-san",
            ]


def parse_perf_file(content):
    """Parse a perf file to get the cpu cycles count and the elapsed time"""
    lines = content.split("\n")

    cycles = None
    elapsed_time = None

    for line in lines:
        stripped_line = line.strip()

        if "cycles" in stripped_line:
            # Splitting the line by spaces and getting the first item
            cycles = int(stripped_line.split()[0].replace(',', ''))

        if "seconds time elapsed" in stripped_line:
            # Splitting the line by spaces and getting the first item
            elapsed_time = float(stripped_line.split()[0])

    return cycles, elapsed_time


def generate_cpu_cycles_processed_results(input_programs):
    # cpu-cycles.csv:
    # wasmtime-variant,bubble_sort_0,...,bubble_sort_4,merge_sort_0,...,merge_sort_4,modified_merge_sort_0,...,modified_merge_sort_4
    # ...data...
    # ...

    print("Generating processed cpu cycles results")

    perf_dir = os.path.join(results_dir, 'perf')
    # get all subdirectories in perf, which are `wasmtime-*` variants
    variants = [d for d in os.listdir(
        perf_dir) if os.path.isdir(os.path.join(perf_dir, d))]

    header = ["wasmtime-variant"]
    for input_program in input_programs:
        for i in range(5):
            header.append(f"{input_program}_{i}")

    results = []

    # Iterate over subdirectories, i.e. wasmtime-variants
    for variant in variants:
        variant_path = os.path.join(perf_dir, variant)
        variant_results = [variant]  # Start with the variant name
        for input_program in input_programs:
            for i in range(5):
                file_name = f"perf__{variant}__{input_program}.cwasm__iter_{i}.txt"
                with open(os.path.join(variant_path, file_name), 'r') as f:
                    content = f.read()
                    # TODO: for some benchmarks suites/qemu environments, we'll have to fall back to the elapsed time, if the cycles aren't available
                    cycles, elapsed_time = parse_perf_file(content)
                    variant_results.append(cycles)
        results.append(variant_results)

    # Write results to CSV
    output_file = os.path.join(all_processed_results_dir, 'cpu-cycles.csv')
    with open(output_file, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(header)
        for row in results:
            writer.writerow(row)


def generate_all_processed_results():
    input_programs = get_input_programs()
    generate_cpu_cycles_processed_results(input_programs)


def generate_cpu_cycles_final_result(input_programs):
    # wasmtime-variant,bubble_sort_mean,bubble_sort_stdev,merge_sort_mean,merge_sort_stdev,modified_merge_sort_mean,modified_merge_sort_stdev
    # ...data...
    # ...

    print("Generating final cpu cycles results")

    # Read the data
    processsed_result_file = os.path.join(
        all_processed_results_dir, 'cpu-cycles.csv')
    data = []
    with open(processsed_result_file, 'r') as csvfile:
        reader = csv.reader(csvfile)
        for row in reader:
            data.append(row)

    header = data[0][1:]
    variants = [row[0] for row in data[1:]]

    # Calculate mean and standard deviation
    final_data = [['wasmtime-variant']]
    for input_program in input_programs:
        final_data[0].extend(
            [f"{input_program}_mean", f"{input_program}_stdev"])

    for i, variant in enumerate(variants):
        row_data = [variant]
        for j in range(1, len(header), 5):  # assuming 5 iterations for each algorithm
            # values = [int(data[i + 1][j + k]) for k in range(5)]
            values = [float(data[i + 1][j + k]) for k in range(5)]
            mean = statistics.mean(values)
            std_dev = statistics.stdev(values)
            row_data.extend([mean, std_dev])
        final_data.append(row_data)

    # Write to final results file
    output_file = os.path.join(all_final_results_dir, 'cpu-cycles.csv')
    with open(output_file, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerows(final_data)


def generate_all_final_results():
    # <metric>.txt, where metric can be one of cpu-cycles, binary-size and count-instructions. Each file should roughly look like this (csv format):
    # wasmtime-variant,bubble_sort_mean,bubble_sort_stdev,merge_sort_mean,merge_sort_stdev,modified_merge_sort_mean,modified_merge_sort_stdev
    # ...data...
    # ...

    input_programs = get_input_programs()
    generate_cpu_cycles_final_result(input_programs)


def generate_cpu_cycles_chart(input_file, output_image_path):
    import matplotlib.pyplot as plt
    import pandas as pd
    import numpy as np

    print("Generating chart")

    # TUM Colors
    TUMBlue = "#0065BD"
    TUMSecondaryBlue = "#005293"
    TUMSecondaryBlue2 = "#003359"
    TUMBlack = "#000000"
    TUMWhite = "#FFFFFF"
    TUMDarkGray = "#333333"
    TUMGray = "#808080"
    TUMLightGray = "#CCCCC6"
    TUMAccentGray = "#DAD7CB"
    TUMAccentOrange = "#E37222"
    TUMAccentGreen = "#A2AD00"
    TUMAccentLightBlue = "#98C6EA"
    TUMAccentBlue = "#64A0C8"
    colors = [TUMBlue, TUMAccentOrange,
              TUMAccentGreen, TUMSecondaryBlue2, TUMDarkGray]

    def darken_color(color_code, factor=0.6):
        """Return a darkened version of the given color code."""
        r, g, b = [int(color_code[i:i+2], 16) for i in (1, 3, 5)]
        r, g, b = [int(c * factor) for c in (r, g, b)]
        return "#{:02x}{:02x}{:02x}".format(r, g, b)

    darkened_colors = [darken_color(color) for color in colors]

    from pathlib import Path
    import matplotlib as mpl

    # NOTE: for the same font to be used that is also used in the latex template I used for my thesis, I downloaded the font manually (there didn't seem any better way)
    font_file = '{base_dir}/palatino.ttf'
    if os.path.exists(font_file):
        fpath = Path(mpl.get_data_path(), "/home/fritz/palatino.ttf")
        font_properties = mpl.font_manager.FontProperties(fname=fpath)
    else:
        font_properties = mpl.font_manager.FontProperties(family='sans-serif')

    plt.rcParams['lines.linewidth'] = 0.1

    # Read data
    df = pd.read_csv(input_file)

    # Filtering and renaming Wasmtime Variants
    match benchmark_suite:
        case "sorting":
            keep_variants = {
                "wasmtime-none": "No MTE",
                "wasmtime-mte-infra-only": "MTE Infrastructure Only",
                "wasmtime-mte-no-tagging": "No Tagging Instructions",
                "wasmtime-mte-naive": "MTE Naive STG",
                "wasmtime-mte-opt-static": "MTE Optimizations",
                "wasmtime-mte-async": "MTE Async Mode",
            }
        case "pac":
            keep_variants = {
                "wasmtime-none": "No PAC",
                "wasmtime-pac": "PAC Enabled"
            }
        case "memory-tagging":
            keep_variants = {
                "wasmtime-mte-naive": "Naive STG",
                "wasmtime-mte-opt": "Optimized ST2G",
                "wasmtime-mte-opt-static": "Optimized ST2G + Loop Unrolling Threshold",
            }
        case "polybench":
            keep_variants = {
                "wasmtime-none": "No MTE",
                "wasmtime-mte-infra-only": "MTE Infrastructure Only",
                "wasmtime-mte-no-tagging": "No Tagging Instructions",
                "wasmtime-mte-naive": "MTE Naive STG",
                "wasmtime-mte-opt-static": "MTE Optimizations",
                "wasmtime-mte-async": "MTE Async Mode",
            }


    df = df[df["wasmtime-variant"].isin(keep_variants.keys())]
    df["wasmtime-variant"] = df["wasmtime-variant"].map(keep_variants)


    # Format with the name as the first element of the tuple
    match benchmark_suite:
        case "sorting":
            algorithms = [
                ("Bubble Sort", "bubble_sort_mean", "bubble_sort_stdev"),
                ("Merge Sort", "merge_sort_mean", "merge_sort_stdev"),
                ("Modified Merge Sort", "modified_merge_sort_mean", "modified_merge_sort_stdev")
            ]
        case "pac":
            algorithms = [
                ("10000", "pac-1_mean", "pac-1_stdev"),
                ("100000", "pac-2_mean", "pac-2_stdev"),
                ("1000000", "pac-3_mean", "pac-3_stdev"),
                ("10000000", "pac-4_mean", "pac-4_stdev"),
                ("100000000", "pac-5_mean", "pac-5_stdev"),
            ]
        case "memory-tagging":
            algorithms = [
                ("1600, 1600", "tagging-few-loops-small-segments_mean", "tagging-few-loops-small-segments_stdev"),
                ("40000, 40000", "tagging-many-loops-large-segments_mean", "tagging-many-loops-large-segments_stdev"),
                ("1600, 4000000", "tagging-few-loops-large-segments_mean", "tagging-few-loops-large-segments_stdev"),
                ("4000000, 1600", "tagging-many-loops-small-segments_mean", "tagging-many-loops-small-segments_stdev"),
            ]
        case "polybench":
            algorithms = [
                ("correlation", "correlation-san_mean", "correlation-san_stdev"),
                ("2mm", "2mm-san_mean", "2mm-san_stdev"),
                ("deriche", "deriche-san_mean", "deriche-san_stdev"),
                ("adi", "adi-san_mean", "adi-san_stdev")
            ]


    bar_width = 0.1
    r = np.arange(len(algorithms))
    gap = 0.02

    fig_width_in_inches = 6
    fig_height_in_inches = 4

    fig, ax = plt.subplots(figsize=(fig_width_in_inches, fig_height_in_inches))


    match benchmark_suite:
        case "sorting":
            normalized_value = df.loc[df["wasmtime-variant"] == "No MTE", mean].values[0]
        case "pac":
            normalized_value = df.loc[df["wasmtime-variant"] == "No PAC", mean].values[0]
        case "memory-tagging":
            normalized_value = df.loc[df["wasmtime-variant"] == "Naive STG", mean].values[0]
        case "polybench":
            normalized_value = df.loc[df["wasmtime-variant"] == "No MTE", mean].values[0]


    for idx, (alg_name, mean, stdev) in enumerate(algorithms):
        offset = -(len(keep_variants) - 1) * (bar_width + gap) / 2
        for v_idx, variant in enumerate(keep_variants.values()):
            y = df.loc[df["wasmtime-variant"] ==
                       variant, mean].values[0] / normalized_value
            y_err = (df.loc[df["wasmtime-variant"] == variant,
                     stdev].values[0] / normalized_value) if normalized_value != 0 else 0
            position = r[idx] + offset + v_idx * (bar_width + gap)
            color_idx = v_idx % len(colors)
            plt.bar(position, y, yerr=y_err, capsize=4, alpha=1, width=bar_width,
                    edgecolor=darkened_colors[color_idx], color=colors[color_idx],
                    label=variant if idx == 0 else "", error_kw=dict(lw=1, capthick=0.5))


    match benchmark_suite:
        case "sorting":
            x_axis_label = 'Sorting Algorithms'
        case "pac":
            x_axis_label = 'Size of Array'
        case "memory-tagging":
            x_axis_label = 'Loop Iterations, Size of Tagged Segments (in Bytes)'
        case "polybench":
            x_axis_label = None

    if benchmark_suite is not None:
        ax.set_xlabel(x_axis_label, fontweight='bold', fontproperties=font_properties)

    ax.set_ylabel('Normalized Runtime', fontweight='bold',
                  fontproperties=font_properties)

    # NOTE: Whether the y axis should be log scale depends heavily on the results, but for some for now it makes more sense
    match benchmark_suite:
        case "sorting" | "polybench":
            ax.set_yscale('log')

    ax.set_xticks(r)
    ax.set_xticklabels(
        [alg_name for alg_name, _, _ in algorithms], fontproperties=font_properties)
    for label in ax.get_yticklabels():
        label.set_fontproperties(font_properties)

    ax.grid(which='both', axis='y', linestyle='-', linewidth=0.7, alpha=0.6)
    ax.set_axisbelow(True)

    # NOTE: you can adjust the number of horizontal columns here manually (if there are too many variants to fit on one line)
    plt.legend(loc='upper center', bbox_to_anchor=(0.5, -0.15), fancybox=False,
               shadow=False, prop=font_properties)
    # plt.legend(loc='upper center', bbox_to_anchor=(0.5, -0.15), fancybox=False,
    #            shadow=False, prop=font_properties, ncol=len(keep_variants))

    plt.tight_layout()
    plt.savefig(output_image_path, format='pdf')
    plt.show()


def generate_all_charts():
    final_data_csv = os.path.join(all_final_results_dir, 'cpu-cycles.csv')
    output_image = os.path.join(chart_results_dir, 'cpu-cycles.pdf')
    generate_cpu_cycles_chart(final_data_csv, output_image)


if __name__ == "__main__":
    main()
