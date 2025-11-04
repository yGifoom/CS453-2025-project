#!/usr/bin/env python3
# filepath: /home/ygifoom/epfl/da/CS451-2025-project/template_cpp/check_correctness.py

import argparse
import sys
from pathlib import Path
from collections import defaultdict

def read_config(config_file):
    """Parse config file to extract parameters."""
    with open(config_file, 'r') as f:
        lines = [line.strip() for line in f if line.strip()]
    
    # Perfect Links format: num_messages num_processes (on one line)
    # num_processes is the total number of processes including the receiver
    parts = lines[0].split()
    num_messages = int(parts[0])
    num_processes = int(parts[1]) if len(parts) > 1 else None
    
    return {
        'num_messages': num_messages,
        'num_processes': num_processes
    }

def read_hosts(hosts_file):
    """Parse hosts file to get list of process IDs."""
    process_ids = []
    with open(hosts_file, 'r') as f:
        for line in f:
            line = line.strip()
            if line:
                # Format: id ip port
                parts = line.split()
                process_ids.append(int(parts[0]))
    return sorted(process_ids)

def check_perfect_links_receiver(output_file, num_messages, process_ids, receiver_id):
    """Check receiver output for Perfect Links."""
    errors = []
    
    if not output_file.exists():
        return [f"Receiver output file not found: {output_file}"]
    
    # Track delivered messages: (sender_id, seq_num) -> count
    delivered = defaultdict(int)
    
    with open(output_file, 'r') as f:
        line_num = 0
        for line in f:
            line_num += 1
            line = line.strip()
            if not line:
                continue
            
            parts = line.split()
            if len(parts) != 3:
                errors.append(f"Line {line_num}: Invalid format '{line}' (expected 'd sender_id seq_num')")
                continue
            
            if parts[0] != 'd':
                errors.append(f"Line {line_num}: Expected 'd' but got '{parts[0]}'")
                continue
            
            try:
                sender_id = int(parts[1])
                seq_num = int(parts[2])
            except ValueError:
                errors.append(f"Line {line_num}: Invalid integers in '{line}'")
                continue
            
            # Check for duplicate delivery
            key = (sender_id, seq_num)
            delivered[key] += 1
            if delivered[key] > 1:
                errors.append(f"Message ({sender_id}, {seq_num}) delivered {delivered[key]} times (duplicate on line {line_num})")
    
    # Check all messages were delivered from all senders (all processes except receiver)
    for sender_id in process_ids:
        if sender_id == receiver_id:
            continue  # Receiver doesn't send to itself
        
        for seq_num in range(1, num_messages + 1):
            key = (sender_id, seq_num)
            if key not in delivered:
                errors.append(f"Message ({sender_id}, {seq_num}) was never delivered")
            elif delivered[key] == 0:
                errors.append(f"Message ({sender_id}, {seq_num}) has 0 deliveries")
    
    return errors

def check_perfect_links_sender(output_file, process_id, num_messages):
    """Check sender output for Perfect Links."""
    errors = []
    
    if not output_file.exists():
        return [f"Sender output file not found: {output_file}"]
    
    broadcasts = []
    
    with open(output_file, 'r') as f:
        line_num = 0
        for line in f:
            line_num += 1
            line = line.strip()
            if not line:
                continue
            
            parts = line.split()
            if len(parts) != 2:
                errors.append(f"Line {line_num}: Invalid format '{line}' (expected 'b seq_num')")
                continue
            
            if parts[0] != 'b':
                errors.append(f"Line {line_num}: Expected 'b' but got '{parts[0]}'")
                continue
            
            try:
                seq_num = int(parts[1])
            except ValueError:
                errors.append(f"Line {line_num}: Invalid integer in '{line}'")
                continue
            
            broadcasts.append(seq_num)
    
    # Check for correct sequence and no duplicates
    expected = list(range(1, num_messages + 1))
    
    if len(broadcasts) != num_messages:
        errors.append(f"Expected {num_messages} broadcasts, got {len(broadcasts)}")
    
    # Check for duplicates
    seen = set()
    for i, seq_num in enumerate(broadcasts, 1):
        if seq_num in seen:
            errors.append(f"Duplicate broadcast: message {seq_num} appears multiple times")
        seen.add(seq_num)
    
    # Check all messages were broadcast
    for seq_num in expected:
        if seq_num not in seen:
            errors.append(f"Message {seq_num} was never broadcast")
    
    # Check order
    if broadcasts != expected and len(broadcasts) == num_messages:
        errors.append(f"Broadcasts not in sequential order: {broadcasts[:10]}... (showing first 10)")
    
    return errors

def check_perfect_links(config, hosts_file, output_dir):
    """Check Perfect Links correctness."""
    num_messages = config['num_messages']
    num_processes_config = config['num_processes']
    
    process_ids = read_hosts(hosts_file)
    num_processes_actual = len(process_ids)
    
    # Validate that config and hosts file match
    if num_processes_config != num_processes_actual:
        return {
            "Configuration Mismatch": [
                f"Config file specifies {num_processes_config} processes, "
                f"but hosts file contains {num_processes_actual} processes",
                f"Processes in hosts file: {process_ids}"
            ]
        }
    
    # Receiver is the process with highest ID
    receiver_id = max(process_ids)
    
    all_errors = {}
    
    # Check receiver - using proc_{id}.output format
    receiver_output = Path(output_dir) / f"proc_{receiver_id}.output"
    receiver_errors = check_perfect_links_receiver(
        receiver_output, num_messages, process_ids, receiver_id
    )
    if receiver_errors:
        all_errors[f"Process {receiver_id} (receiver)"] = receiver_errors
    
    # Check senders - using proc_{id}.output format
    for process_id in process_ids:
        if process_id == receiver_id:
            continue
        
        sender_output = Path(output_dir) / f"proc_{process_id}.output"
        sender_errors = check_perfect_links_sender(sender_output, process_id, num_messages)
        if sender_errors:
            all_errors[f"Process {process_id} (sender)"] = sender_errors
    
    return all_errors

def check_fifo_broadcast(config, hosts_file, output_dir):
    """Check FIFO Broadcast correctness (not implemented)."""
    return {"FIFO Broadcast": ["Not implemented yet"]}

def check_lattice_agreement(config, hosts_file, output_dir):
    """Check Lattice Agreement correctness (not implemented)."""
    return {"Lattice Agreement": ["Not implemented yet"]}

def check_correctness(config_file, hosts_file, output_dir, algorithm='pl'):
    """
    Check correctness of outputs. Can be called from other scripts.
    Returns (success: bool, errors: dict)
    """
    try:
        config = read_config(config_file)
    except Exception as e:
        return False, {"Config Error": [str(e)]}
    
    if algorithm == 'pl':
        errors = check_perfect_links(config, hosts_file, output_dir)
    elif algorithm == 'fifo':
        errors = check_fifo_broadcast(config, hosts_file, output_dir)
    elif algorithm == 'lattice':
        errors = check_lattice_agreement(config, hosts_file, output_dir)
    else:
        return False, {"Error": [f"Unknown algorithm: {algorithm}"]}
    
    return len(errors) == 0, errors

def main():
    parser = argparse.ArgumentParser(
        description='Check correctness of distributed algorithm outputs'
    )
    parser.add_argument('config_file', help='Path to config file')
    parser.add_argument('hosts_file', help='Path to hosts file')
    parser.add_argument('output_dir', help='Directory containing output files')
    parser.add_argument('algorithm', choices=['pl', 'fifo', 'lattice'],
                        help='Algorithm to check: pl (Perfect Links), fifo (FIFO Broadcast), lattice (Lattice Agreement)')
    
    args = parser.parse_args()
    
    success, errors = check_correctness(
        args.config_file, 
        args.hosts_file, 
        args.output_dir, 
        args.algorithm
    )
    
    # Report results
    if success:
        print("✓ All correctness checks passed!")
        return 0
    else:
        print("✗ Correctness check FAILED")
        print("=" * 60)
        for component, error_list in errors.items():
            print(f"\n{component}:")
            for error in error_list[:20]:  # Limit to first 20 errors per component
                print(f"  - {error}")
            if len(error_list) > 20:
                print(f"  ... and {len(error_list) - 20} more errors")
        return 1

if __name__ == "__main__":
    sys.exit(main())