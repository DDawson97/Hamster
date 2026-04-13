import os
import subprocess
import time
import pandas as pd
import re

# --- CONFIGURATION ---
EXECUTABLE_PATH = "../build/solver" 
CONTEST_JSON_DIR = "../tests"

def run_benchmarks(executable_path, test_folder):
    """Runs the C++ solver on every JSON file and records the results."""
    if not os.path.exists(executable_path):
        print(f"Error: Executable not found at {executable_path}. Did you compile it?")
        return

    print(f"Starting benchmarks on {test_folder}...")
    results = []
    
    # Optional: limit the number of files if you just want a quick test
    file_list = [f for f in os.listdir(test_folder) if f.endswith(".json")]
    
    for file in file_list:
        file_path = os.path.join(test_folder, file)
        
        start_time = time.time()
        # Call the C++ executable
        process = subprocess.run([executable_path, file_path], capture_output=True, text=True)
        exec_time = time.time() - start_time
        
        # Parse the output string using Regex to find the final scores
        output = process.stdout
        k_matches = re.findall(r"Max Crossings \(k\):\s*(\d+)", output)
        total_matches = re.findall(r"Total:\s*(\d+)", output)
        
        if k_matches and total_matches:
            final_k = int(k_matches[-1])
            final_total = int(total_matches[-1])
            print(f"Solved {file:<20} | k: {final_k:<3} | Total: {final_total:<5} | Time: {exec_time:.3f}s")
            results.append({
                "Graph": file, 
                "Final_k": final_k, 
                "Total_Crossings": final_total, 
                "Time_Seconds": round(exec_time, 3)
            })
        else:
            print(f"Failed to parse output for {file}. Output was:\n{output.strip()}")

    if not results:
        print("No benchmarks were successfully completed.")
        return

    # Save to CSV
    df = pd.DataFrame(results)
    csv_filename = "solver_benchmarks.csv"
    df.to_csv(csv_filename, index=False)
    
    print(f"\nBenchmark saved to {csv_filename}")
    print("\n--- Summary Statistics ---")
    # Print a statistical summary of your solver's performance
    print(df.describe().round(2)) 

if __name__ == "__main__":
    if os.path.exists(CONTEST_JSON_DIR):
        run_benchmarks(EXECUTABLE_PATH, CONTEST_JSON_DIR)
    else:
        print(f"Error: The directory {CONTEST_JSON_DIR} does not exist.")