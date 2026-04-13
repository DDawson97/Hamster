import os
import json
import networkx as nx

# --- CONFIGURATION ---
ROME_GRAPHS_DIR = "../tests/rome-gml"
CONTEST_JSON_DIR = "../tests"

def convert_rome_to_json(input_dir, output_dir, grid_size=100):
    """Converts Rome GML/GraphML files into the GD Contest JSON format."""
    print(f"Converting Rome graphs from {input_dir} to JSON format in {output_dir}...")
    os.makedirs(output_dir, exist_ok=True)
    
    converted_count = 0
    for file in os.listdir(input_dir):
        if not (file.endswith(".graphml") or file.endswith(".gml")): 
            continue
        
        filepath = os.path.join(input_dir, file)
        try:
            G = nx.read_graphml(filepath) if file.endswith(".graphml") else nx.read_gml(filepath)
        except Exception as e:
            print(f"Skipping {file} due to parsing error: {e}")
            continue
        
        data = {"nodes": [], "edges": [], "width": grid_size, "height": grid_size}
        mapping = {node: i for i, node in enumerate(G.nodes())} # Standardize IDs to 0-N
        
        for node, idx in mapping.items():
            data["nodes"].append({"id": idx, "x": 0, "y": 0})
            
        for u, v in G.edges():
            data["edges"].append({"source": mapping[u], "target": mapping[v]})
            
        out_name = file.rsplit('.', 1)[0] + ".json"
        with open(os.path.join(output_dir, out_name), 'w') as f:
            json.dump(data, f)
            
        converted_count += 1

    print(f"Conversion complete! Successfully converted {converted_count} graphs.")

if __name__ == "__main__":
    if os.path.exists(ROME_GRAPHS_DIR):
        convert_rome_to_json(ROME_GRAPHS_DIR, CONTEST_JSON_DIR)
    else:
        print(f"Error: The directory {ROME_GRAPHS_DIR} does not exist.")