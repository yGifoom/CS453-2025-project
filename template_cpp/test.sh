#!/bin/bash

# Check if required arguments are provided
if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <hosts_file> <config_file> [output_dir]"
    exit 1
fi

HOSTS_FILE="$1"
CONFIG_FILE="$2"
OUTPUT_DIR="${3:-output}"
BINARY="./run.sh"

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

# Array to store PIDs
declare -a PIDS

# Read the second number to get number of processes (from first non-empty, non-comment line)
NUM_PROCESSES=$(awk 'NF && $1 !~ /^#/ {print $2; exit}' "$CONFIG_FILE")
if ! [[ "$NUM_PROCESSES" =~ ^[0-9]+$ ]]; then
    echo "Error: could not read process count (2nd number) from $CONFIG_FILE"
    exit 1
fi

echo "Starting $NUM_PROCESSES processes..."

# Start each process
for ((i=1; i<=NUM_PROCESSES; i++)); do
    OUTPUT_FILE="$OUTPUT_DIR/proc_$i.output"
    
    echo "Starting process $i..."
    $BINARY --id $i --hosts "$HOSTS_FILE" --output "$OUTPUT_FILE" --config "$CONFIG_FILE" &
    
    PID=$!
    PIDS+=($PID)
    echo "Process $i started with PID $PID"
done

echo ""
echo "All processes started. PIDs: ${PIDS[@]}"
echo "Press Ctrl+C to stop all processes gracefully..."

# Function to handle cleanup
cleanup() {
    echo ""
    echo "Stopping all processes gracefully..."
    
    for PID in "${PIDS[@]}"; do
        if kill -0 $PID 2>/dev/null; then
            echo "Sending SIGTERM to PID $PID"
            kill -SIGTERM $PID
        fi
    done
    
    # Wait for all processes to terminate
    echo "Waiting for processes to terminate..."
    for PID in "${PIDS[@]}"; do
        if kill -0 $PID 2>/dev/null; then
            wait $PID 2>/dev/null
        fi
    done
    
    echo "All processes stopped."
    exit 0
}

# Trap SIGINT and SIGTERM
trap cleanup SIGINT SIGTERM

# Wait for all background processes
wait