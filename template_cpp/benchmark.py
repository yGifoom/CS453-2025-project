#!/usr/bin/env python3

import argparse
import subprocess
import statistics
import sys
import os
import re
from pathlib import Path

def run_test(script_path, hosts_file, config_file, output_dir, use_valgrind=False):
    """Run a single test and extract execution time."""
    cmd = [script_path, hosts_file, config_file, output_dir]
    if use_valgrind:
        cmd.append("--valgrind")
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
        
        # Extract execution time from output
        # Look for "Execution time: X.XXXs (XXXms)" format
        match = re.search(r'Execution time: ([\d.]+)s \((\d+)ms\)', result.stdout)
        if match:
            return int(match.group(2))
        else:
            # Try alternative patterns
            match_alt = re.search(r'(\d+)ms', result.stdout)
            if match_alt:
                print(f"Warning: Using alternative pattern", file=sys.stderr)
                return int(match_alt.group(1))
            
            print(f"Warning: Could not extract execution time from output", file=sys.stderr)
            print(f"Looking for pattern: 'Execution time: X.XXXs (XXXms)'", file=sys.stderr)
            print(f"Last 800 chars of stdout:", file=sys.stderr)
            print(result.stdout[-800:], file=sys.stderr)
            print(f"\nLast 300 chars of stderr:", file=sys.stderr)
            print(result.stderr[-300:], file=sys.stderr)
            return None
    except subprocess.TimeoutExpired:
        print("Error: Test timed out after 5 minutes", file=sys.stderr)
        return None
    except Exception as e:
        print(f"Error running test: {e}", file=sys.stderr)
        return None

def main():
    parser = argparse.ArgumentParser(
        description='Run tests multiple times and compute execution time statistics'
    )
    parser.add_argument('hosts_file', help='Path to hosts file')
    parser.add_argument('config_file', help='Path to config file')
    parser.add_argument('-n', '--iterations', type=int, default=5,
                        help='Number of iterations to run (default: 5)')
    parser.add_argument('-o', '--output-dir', default=None,
                        help='Base output directory (default: auto-generated)')
    parser.add_argument('--valgrind', action='store_true',
                        help='Run with valgrind (will be much slower)')
    parser.add_argument('--script', default=None,
                        help='Path to test.sh script (default: auto-detect)')
    
    args = parser.parse_args()
    
    # Find test.sh script
    if args.script:
        script_path = args.script
    else:
        script_dir = Path(__file__).parent
        script_path = script_dir / "test.sh"
    
    script_path = str(Path(script_path).resolve())
    
    if not os.path.isfile(script_path):
        print(f"Error: test.sh not found at {script_path}", file=sys.stderr)
        return 1
    
    # Convert paths to absolute
    hosts_file = str(Path(args.hosts_file).resolve())
    config_file = str(Path(args.config_file).resolve())
    
    # Verify input files
    if not os.path.isfile(hosts_file):
        print(f"Error: Hosts file not found: {hosts_file}", file=sys.stderr)
        return 1
    
    if not os.path.isfile(config_file):
        print(f"Error: Config file not found: {config_file}", file=sys.stderr)
        return 1
    
    print(f"Running {args.iterations} iterations...")
    print(f"Hosts file: {hosts_file}")
    print(f"Config file: {config_file}")
    if args.valgrind:
        print("Valgrind mode: ENABLED (execution will be much slower)")
    print()
    
    execution_times = []
    
    for i in range(1, args.iterations + 1):
        # Create unique output directory for each iteration
        if args.output_dir:
            output_dir = f"{args.output_dir}/run_{i}"
        else:
            output_dir = str(Path(script_path).parent / "output" / f"run_{i}")
        
        print(f"=== Iteration {i}/{args.iterations} ===")
        print(f"Output directory: {output_dir}")
        
        exec_time = run_test(script_path, hosts_file, config_file, output_dir, args.valgrind)
        
        if exec_time is not None:
            execution_times.append(exec_time)
            print(f"Execution time: {exec_time}ms ({exec_time/1000:.3f}s)")
        else:
            print("Failed to get execution time")
        
        print()
    
    # Compute statistics
    if len(execution_times) == 0:
        print("Error: No successful test runs", file=sys.stderr)
        return 1
    
    print("=" * 50)
    print("RESULTS")
    print("=" * 50)
    print(f"Successful runs: {len(execution_times)}/{args.iterations}")
    print(f"Execution times (ms): {execution_times}")
    print()
    print(f"Mean:               {statistics.mean(execution_times):.2f}ms ({statistics.mean(execution_times)/1000:.3f}s)")
    print(f"Median:             {statistics.median(execution_times):.2f}ms ({statistics.median(execution_times)/1000:.3f}s)")
    
    if len(execution_times) > 1:
        print(f"Standard deviation: {statistics.stdev(execution_times):.2f}ms ({statistics.stdev(execution_times)/1000:.3f}s)")
        print(f"Min:                {min(execution_times)}ms ({min(execution_times)/1000:.3f}s)")
        print(f"Max:                {max(execution_times)}ms ({max(execution_times)/1000:.3f}s)")
    else:
        print("Standard deviation: N/A (need at least 2 runs)")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
