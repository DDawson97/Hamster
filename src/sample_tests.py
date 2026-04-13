import os
import json
import random
import shutil

# --- CONFIGURATION ---
SOURCE_DIR = "../tests"
MINI_DIR = "../tests-mini"
GRAPHS_PER_BUCKET = 10

def create_stratified_subset():
    print(f"Creating stratified Mini-Rome dataset in {MINI_DIR}...")
    os.makedirs(MINI_DIR, exist_ok=True)
    
    # Create buckets for node ranges (e.g., 10-19, 20-29... 90-100)
    buckets = {i: [] for i in range(10, 101, 10)}
    
    # Read and categorize existing JSON files
    for file in os.listdir(SOURCE_DIR):
        if not file.endswith(".json"): continue
        
        filepath = os.path.join(SOURCE_DIR, file)
        try:
            with open(filepath, 'r') as f:
                data = json.load(f)
                num_nodes = len(data.get("nodes", []))
                
                # Find the right bucket (round down to nearest 10)
                bucket_key = (num_nodes // 10) * 10
                if bucket_key < 10: bucket_key = 10
                if bucket_key > 100: bucket_key = 100
                
                if bucket_key in buckets:
                    buckets[bucket_key].append(filepath)
        except Exception as e:
            continue

    # Sample and copy files
    total_copied = 0
    for size, files in buckets.items():
        # Take a random sample, or all of them if there are fewer than GRAPHS_PER_BUCKET
        sample = random.sample(files, min(GRAPHS_PER_BUCKET, len(files)))
        for filepath in sample:
            filename = os.path.basename(filepath)
            dest_path = os.path.join(MINI_DIR, f"size_{size}_{filename}")
            shutil.copy(filepath, dest_path)
            total_copied += 1
            
    print(f"Done! Copied {total_copied} perfectly distributed graphs to {MINI_DIR}.")

if __name__ == "__main__":
    create_stratified_subset()