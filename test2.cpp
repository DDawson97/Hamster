#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <ogdf/basic/Graph.h>
#include <ogdf/basic/GraphAttributes.h>
#include <ogdf/fileformats/GraphIO.h>

using json = nlohmann::json;
using namespace ogdf;

int main() {
    // Mocking the contest input format
    std::string raw_json = R"(
    {
        "nodes": [{"id": 0}, {"id": 1}, {"id": 2}, {"id": 3}],
        "edges": [{"source": 0, "target": 1}, {"source": 1, "target": 2}, {"source": 2, "target": 3}, {"source": 3, "target": 0}]
    }
    )";

    json data = json::parse(raw_json);
    Graph G;

    GraphAttributes GA(G,
        GraphAttributes::nodeGraphics |
        GraphAttributes::edgeGraphics);

    // Map to keep track of OGDF nodes by their JSON IDs
    std::map<int, node> node_map;

    for (auto& n : data["nodes"]) {
        node_map[n["id"]] = G.newNode();
        std::cout << "Created OGDF node for JSON ID: " << n["id"] << std::endl;
    }

    for (auto& e : data["edges"]) {
        G.newEdge(node_map[e["source"]], node_map[e["target"]]);
        std::cout << "Created Edge: " << e["source"] << " -> " << e["target"] << std::endl;
    }

    std::cout << "Success! Graph has " << G.numberOfNodes() << " nodes." << std::endl;

    for (node v : G.nodes) {
        GA.width(v) = 2.0;
        GA.height(v) = 2.0;
    }

    if (GraphIO::write(GA, "json_to_graph.svg", GraphIO::drawSVG)) {
        std::cout << "Success! Check json_to_graph.svg in your folder." << std::endl;
    } else {
        std::cerr << "Failed to write SVG file." << std::endl;
    }


    return 0;
}