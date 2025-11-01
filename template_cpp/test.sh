#!/bin/bash

set -e  # Exit on error

# Get the directory where the script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Check if required arguments areprovided
if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <hosts_file> <config_file> [output_dir]"
    exit 1
fi

# Convert to absolute paths
HOSTS_FILE="$(realpath "$1")"
CONFIG_FILE="$(realpath "$2")"
OUTPUT_DIR="${3:-$SCRIPT_DIR/output}"
# Also make output dir absolute
OUTPUT_DIR="$(realpath -m "$OUTPUT_DIR")"
# Call the binary directly instead of through run.sh
BINARY="$SCRIPT_DIR/bin/da_proc"

# Verify that the binary exists and is executable
if [ ! -f "$BINARY" ]; then
    echo "Error: Binary $BINARY not found"
    exit 1
fi

if [ ! -x "$BINARY" ]; then
    echo "Error: Binary $BINARY is not executable"
    echo "Run: chmod +x $BINARY"
    exit 1
fi

# Verify input files exist
if [ ! -f "$HOSTS_FILE" ]; then
    echo "Error: Hosts file $HOSTS_FILE not found"
    exit 1
fi

if [ ! -f "$CONFIG_FILE" ]; then
    echo "Error: Config file $CONFIG_FILE not found"
    exit 1
fi

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

# Array to store PIDs
declare -a PIDS

# Read the second number to get number of processes (from first non-empty, non-comment line)
NUM_PROCESSES=$(awk 'NF && $1 !~ /^#/ {print $2; exit}' "$CONFIG_FILE")
if ! [[ "$NUM_PROCESSES" =~ ^[0-9]+$ ]] || [ "$NUM_PROCESSES" -eq 0 ]; then
    echo "Error: could not read valid process count (2nd number) from $CONFIG_FILE"
    exit 1
fi

echo "Starting $NUM_PROCESSES processes..."
echo "Output directory: $OUTPUT_DIR"
echo "Hosts file: $HOSTS_FILE"
echo "Config file: $CONFIG_FILE"
echo ""

# Test that files are readable
if [ ! -r "$HOSTS_FILE" ]; then
    echo "Error: Cannot read hosts file: $HOSTS_FILE"
    exit 1
fi

if [ ! -r "$CONFIG_FILE" ]; then
    echo "Error: Cannot read config file: $CONFIG_FILE"
    exit 1
fi

# Print first few lines of config for verification
echo "Config file contents:"
head -n 3 "$CONFIG_FILE"
echo ""

# Start each process
for ((i=1; i<=NUM_PROCESSES; i++)); do
    OUTPUT_FILE="$OUTPUT_DIR/proc_$i.output"
    
    echo "Starting process $i (output: $OUTPUT_FILE)..."
    # Pass config as positional argument (no --config flag)
    "$BINARY" --id "$i" --hosts "$HOSTS_FILE" --output "$OUTPUT_FILE" "$CONFIG_FILE" > "$OUTPUT_DIR/proc_$i.stderr" 2>&1 &
    
    PID=$!
    PIDS+=($PID)
    echo "  Process $i started with PID $PID"
    
    # Small delay to avoid race conditions
    sleep 0.1
done

echo ""
echo "All processes started. PIDs: ${PIDS[@]}"
echo "Press Ctrl+C to stop all processes gracefully..."
echo ""

# Function to handle cleanup
cleanup() {
    echo ""
    echo "Stopping all processes gracefully..."
    
    for PID in "${PIDS[@]}"; do
        if ps -p $PID > /dev/null 2>&1; then
            echo "  Sending SIGTERM to PID $PID"
            kill -TERM $PID 2>/dev/null || true
        fi
    done
    
    # Wait for all processes to terminate (with timeout)
    echo "Waiting for processes to terminate..."
    TIMEOUT=5
    for PID in "${PIDS[@]}"; do
        COUNT=0
        while ps -p $PID > /dev/null 2>&1 && [ $COUNT -lt $TIMEOUT ]; do
            sleep 1
            COUNT=$((COUNT + 1))
        done
        
        # Force kill if still running
        if ps -p $PID > /dev/null 2>&1; then
            echo "  Force killing PID $PID"
            kill -9 $PID 2>/dev/null || true
        fi
    done
    
    echo "All processes stopped."
    echo "Output files are in: $OUTPUT_DIR"
    exit 0
}

# Trap SIGINT and SIGTERM
trap cleanup SIGINT SIGTERM

# Wait for all background processes
wait

# If we get here, all processes exited normally
echo ""
echo "All processes completed."
echo "Output files are in: $OUTPUT_DIR"