#include <ogdf/basic/Graph.h>
#include <ogdf/basic/GraphAttributes.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <map>

using namespace ogdf;
using json = nlohmann::json;

// --- GEOMETRY HELPER FUNCTIONS ---

// 0 = Collinear, 1 = Clockwise, 2 = Counter-Clockwise
int orientation(double px, double py, double qx, double qy, double rx, double ry) {
    double val = (qy - py) * (rx - qx) - (qx - px) * (ry - qy);
    if (std::abs(val) < 1e-9) return 0; // Collinear (handling floating point limits)
    return (val > 0) ? 1 : 2; 
}

// Returns true if segment (p1, q1) and segment (p2, q2) intersect
bool doIntersect(double p1x, double p1y, double q1x, double q1y, 
                 double p2x, double p2y, double q2x, double q2y) {
    
    int o1 = orientation(p1x, p1y, q1x, q1y, p2x, p2y);
    int o2 = orientation(p1x, p1y, q1x, q1y, q2x, q2y);
    int o3 = orientation(p2x, p2y, q2x, q2y, p1x, p1y);
    int o4 = orientation(p2x, p2y, q2x, q2y, q1x, q1y);

    // General case: segments strictly cross each other
    if (o1 != o2 && o3 != o4) {
        return true;
    }
    
    // (Ignoring collinear overlaps for this specific contest metric, 
    // as nodes overlapping edges are usually penalized separately)
    return false;
}

// --- MAIN PROGRAM ---

int main() {
    // 1. Load the exact input file
    std::ifstream i("../test-2.json");
    if (!i.is_open()) {
        std::cerr << "Error: Could not open test-2.json" << std::endl;
        return 1;
    }
    json j;
    i >> j;

    Graph G;
    GraphAttributes GA(G, GraphAttributes::nodeGraphics | GraphAttributes::nodeLabel);  
    std::map<int, node> idToNode;

    // 2. Map JSON nodes exactly as they appear (NO visual scaling here!)
    for (auto& n : j["nodes"]) {
        node v = G.newNode();
        idToNode[n["id"]] = v;
        GA.x(v) = n.value("x", 0.0); 
        GA.y(v) = n.value("y", 0.0); 
        GA.label(v) = std::to_string((int)n["id"]);
    }

    // Map the edges
    for (auto& e : j["edges"]) {
        G.newEdge(idToNode[e["source"]], idToNode[e["target"]]);
    }

    // 3. SCORE THE GRAPH
    int total_crossings = 0;
    int max_crossings_per_edge = 0;
    std::map<edge, int> crossings_per_edge; // Tracks counts for individual edges

    // Put all edges in a vector so we can easily check every unique pair
    std::vector<edge> edge_list;
    for (edge e : G.edges) {
        edge_list.push_back(e);
        crossings_per_edge[e] = 0; // Initialize counts to zero
    }

    // Nested loop to check every unique pair of edges exactly once
    for (size_t a = 0; a < edge_list.size(); ++a) {
        for (size_t b = a + 1; b < edge_list.size(); ++b) {
            edge e1 = edge_list[a];
            edge e2 = edge_list[b];

            // CRITICAL: Skip edges that share a node. 
            // Edges that touch at a vertex do not "cross" mathematically.
            if (e1->source() == e2->source() || e1->source() == e2->target() ||
                e1->target() == e2->source() || e1->target() == e2->target()) {
                continue;
            }

            node p1 = e1->source(), q1 = e1->target();
            node p2 = e2->source(), q2 = e2->target();

            // Check for intersection using actual node coordinates
            if (doIntersect(GA.x(p1), GA.y(p1), GA.x(q1), GA.y(q1),
                            GA.x(p2), GA.y(p2), GA.x(q2), GA.y(q2))) {
                
                total_crossings++;
                crossings_per_edge[e1]++;
                crossings_per_edge[e2]++;
            }
        }
    }

    // Find the maximum crossings on any single edge
    for (const auto& pair : crossings_per_edge) {
        max_crossings_per_edge = std::max(max_crossings_per_edge, pair.second);
    }

    // 4. PRINT THE SCORES
    std::cout << "--- CONTEST SCORER ---" << std::endl;
    std::cout << "Total Nodes: " << G.numberOfNodes() << std::endl;
    std::cout << "Total Edges: " << G.numberOfEdges() << std::endl;
    std::cout << "----------------------" << std::endl;
    std::cout << "Total Edge Crossings: " << total_crossings << std::endl;
    std::cout << "Max Crossings on a Single Edge: " << max_crossings_per_edge << std::endl;

    return 0;
}