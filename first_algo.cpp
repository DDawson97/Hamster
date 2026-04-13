#include <ogdf/basic/Graph.h>
#include <ogdf/basic/GraphAttributes.h>
#include <ogdf/fileformats/GraphIO.h>
#include <ogdf/energybased/FMMMLayout.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <map>
#include <random>
#include <limits>

using namespace ogdf;
using json = nlohmann::json;

// --- GEOMETRY HELPER FUNCTIONS ---
int orientation(double px, double py, double qx, double qy, double rx, double ry) {
    double val = (qy - py) * (rx - qx) - (qx - px) * (ry - qy);
    if (std::abs(val) < 1e-9) return 0; 
    return (val > 0) ? 1 : 2; 
}

bool doIntersect(double p1x, double p1y, double q1x, double q1y, 
                 double p2x, double p2y, double q2x, double q2y) {
    int o1 = orientation(p1x, p1y, q1x, q1y, p2x, p2y);
    int o2 = orientation(p1x, p1y, q1x, q1y, q2x, q2y);
    int o3 = orientation(p2x, p2y, q2x, q2y, p1x, p1y);
    int o4 = orientation(p2x, p2y, q2x, q2y, q1x, q1y);
    if (o1 != o2 && o3 != o4) return true;
    return false;
}

// Returns true if point (px, py) lies on the segment (a, b)
bool isPointOnSegment(double px, double py, double ax, double ay, double bx, double by) {
    // 1. Check collinearity using orientation
    if (orientation(ax, ay, bx, by, px, py) != 0) return false;
    
    // 2. Check if point is within the bounding box of the segment
    return px >= std::min(ax, bx) && px <= std::max(ax, bx) &&
           py >= std::min(ay, by) && py <= std::max(ay, by);
}

// Returns true if move is invalid due to overlaps
bool isMoveInvalid(const Graph& G, const GraphAttributes& GA, node v, double next_x, double next_y) {
    // 1. Vertex-Vertex Overlap
    for (node u : G.nodes) {
        if (u != v && GA.x(u) == next_x && GA.y(u) == next_y) return true;
    }

    // 2. Vertex-Edge Overlap: Does the moved node land on an existing edge?
    for (edge e : G.edges) {
        if (e->source() == v || e->target() == v) continue; // Skip incident edges
        if (isPointOnSegment(next_x, next_y, GA.x(e->source()), GA.y(e->source()), 
                             GA.x(e->target()), GA.y(e->target()))) return true;
    }

    // 3. Edge-Vertex Overlap: Do edges connected to V now hit another vertex?
    for (adjEntry adj : v->adjEntries) {
        edge e = adj->theEdge();
        node other = e->opposite(v);
        for (node u : G.nodes) {
            if (u == v || u == other) continue;
            if (isPointOnSegment(GA.x(u), GA.y(u), next_x, next_y, GA.x(other), GA.y(other))) return true;
        }
    }
    
    // 4. Edge-Edge Overlap (Collinear)
    // For simplicity, orientation logic handles most of this. If you get 
    // orientation=0 for endpoints, you must check for segment overlap.
    return false; 
}

void evaluateCrossings(const Graph& G, const GraphAttributes& GA, int& max_k, int& total_crossings) {
    total_crossings = 0;
    max_k = 0;
    std::vector<edge> edges;
    for (edge e : G.edges) edges.push_back(e);
    std::vector<int> edge_crossings(edges.size(), 0);

    for (size_t a = 0; a < edges.size(); ++a) {
        for (size_t b = a + 1; b < edges.size(); ++b) {
            edge e1 = edges[a];
            edge e2 = edges[b];
            if (e1->source() == e2->source() || e1->source() == e2->target() ||
                e1->target() == e2->source() || e1->target() == e2->target()) {
                continue; 
            }
            if (doIntersect(GA.x(e1->source()), GA.y(e1->source()), GA.x(e1->target()), GA.y(e1->target()),
                            GA.x(e2->source()), GA.y(e2->source()), GA.x(e2->target()), GA.y(e2->target()))) {
                edge_crossings[a]++;
                edge_crossings[b]++;
                total_crossings++;
            }
        }
    }
    for (int k : edge_crossings) {
        if (k > max_k) max_k = k;
    }
}

// Check if a grid coordinate is already taken
bool isOccupied(const Graph& G, const GraphAttributes& GA, node v, double x, double y) {
    for (node u : G.nodes) {
        if (u != v && GA.x(u) == x && GA.y(u) == y) return true;
    }
    return false;
}

int main(int argc, char* argv[]) {
    // ==========================================
    // SETUP: LOAD DATA
    // ==========================================
    string filepath = "tests/150-nodes.json";
    if (argc > 1) filepath = argv[1]; // Accept file from Python

    std::filesystem::path inputPath(filepath);
    if (!std::filesystem::exists(inputPath)) {
        std::filesystem::path fallbackPath = std::filesystem::path("..") / filepath;
        if (std::filesystem::exists(fallbackPath)) {
            inputPath = fallbackPath;
        }
    }

    std::ifstream i(inputPath);
    if (!i.is_open()) {
        std::cerr << "Error: Could not open " << inputPath.string() << std::endl;
        std::cerr << "Current working directory: " << std::filesystem::current_path() << std::endl;
        return 1;
    }
    json j; i >> j;

    Graph G;
    GraphAttributes GA(G, GraphAttributes::nodeGraphics | GraphAttributes::edgeGraphics | GraphAttributes::nodeLabel);  
    std::map<int, node> idToNode;
    std::vector<node> node_list;

    int grid_width = j.value("width", 100);
    int grid_height = j.value("height", 100);

    for (auto& n : j["nodes"]) {
        node v = G.newNode();
        idToNode[n["id"]] = v;
        node_list.push_back(v);
        GA.label(v) = std::to_string((int)n["id"]);

        // Explicitly load the starting coordinates
        GA.x(v) = n.value("x", 0.0);
        GA.y(v) = n.value("y", 0.0);
    }
    for (auto& e : j["edges"]) {
        G.newEdge(idToNode[e["source"]], idToNode[e["target"]]);
    }

    std::cout << "--- STARTING UNIFIED SOLVER ---" << std::endl;
    std::cout << "Graph: " << G.numberOfNodes() << " nodes, " << G.numberOfEdges() << " edges." << std::endl;
    std::cout << "Grid Limit: " << grid_width << "x" << grid_height << std::endl;

    int current_k, current_total;
    evaluateCrossings(G, GA, current_k, current_total);
    std::cout << "Initial k: " << current_k << " | Total: " << current_total << std::endl;

    // ==========================================
    // PHASE 1: GLOBAL UNTANGLING (FMMM)
    // ==========================================
    std::cout << "\n[Phase 1] Running Force-Directed Layout (FMMM)..." << std::endl;
    FMMMLayout fmmm;
    fmmm.useHighLevelOptions(true);
    fmmm.unitEdgeLength(100.0); 
    fmmm.call(GA);

    // ==========================================
    // PHASE 2: GRID SNAPPING & DISCRETIZATION
    // ==========================================
    std::cout << "[Phase 2] Snapping continuous layout to integer grid..." << std::endl;
    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();
    double cur_max_x = std::numeric_limits<double>::lowest();
    double cur_max_y = std::numeric_limits<double>::lowest();

    for (node v : G.nodes) {
        min_x = std::min(min_x, GA.x(v));
        min_y = std::min(min_y, GA.y(v));
        cur_max_x = std::max(cur_max_x, GA.x(v));
        cur_max_y = std::max(cur_max_y, GA.y(v));
    }

    double current_w = cur_max_x - min_x;
    double current_h = cur_max_y - min_y;
    double scale = 1.0;
    if (current_w > 0 && current_h > 0) {
        double scaleX = grid_width / current_w;
        double scaleY = grid_height / current_h;
        scale = std::min(scaleX, scaleY) * 0.95; // 5% padding
    }

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist_x(0, grid_width);
    std::uniform_int_distribution<int> dist_y(0, grid_height);

    for (node v : G.nodes) {
        int nx = std::round((GA.x(v) - min_x) * scale);
        int ny = std::round((GA.y(v) - min_y) * scale);
        
        // Clamp to ensure it doesn't break boundaries
        nx = std::max(0, std::min(grid_width, nx));
        ny = std::max(0, std::min(grid_height, ny));

        // Improved Overlap Resolution:
        // We check isMoveInvalid to ensure we aren't landing on a node OR an edge.
        int attempts = 0;
        while (isMoveInvalid(G, GA, v, nx, ny) && attempts < 100) {
            nx = dist_x(rng);
            ny = dist_y(rng);
            attempts++;
        }

        GA.x(v) = nx;
        GA.y(v) = ny;
    }

    // ==========================================
    // PHASE 3: LOCAL OPTIMIZATION (Hill Climbing)
    // ==========================================
    evaluateCrossings(G, GA, current_k, current_total);
    std::cout << "[Phase 3] Starting Local Search. Initial k: " << current_k << " | Total: " << current_total << std::endl;

    std::uniform_int_distribution<int> dist_node(0, node_list.size() - 1);
    
    // NEW: Setup the nudge distribution (-3 to +3 grid units)
    std::uniform_int_distribution<int> dist_nudge(-3, 3);
    // NEW: Setup a probability distribution to decide between nudging and teleporting
    std::uniform_real_distribution<double> dist_choice(0.0, 1.0);

    int iterations = 40000;

    for (int step = 0; step < iterations; ++step) {
        if (current_k == 0) break; // Perfect score achieved

        node v = node_list[dist_node(rng)];
        double old_x = GA.x(v);
        double old_y = GA.y(v);
        
        double new_x, new_y;

        // --- THE LOCAL SEARCH NUDGE ---
        // 10% chance to gently nudge the node
        if (dist_choice(rng) < 0.1) {
            new_x = old_x + dist_nudge(rng);
            new_y = old_y + dist_nudge(rng);
            
            // Clamp to ensure the nudge doesn't push the node off the grid
            new_x = std::max(0.0, std::min((double)grid_width, new_x));
            new_y = std::max(0.0, std::min((double)grid_height, new_y));
        } 
        // 90% chance to completely randomize (teleport) to escape getting stuck
        else {
            new_x = dist_x(rng);
            new_y = dist_y(rng);
        }
        // ------------------------------
        
        // Use the strict validity check instead of just isOccupied
        if (isMoveInvalid(G, GA, v, new_x, new_y)) continue;
        
        // Temporarily apply the move
        GA.x(v) = new_x;
        GA.y(v) = new_y;
        
        int new_k, new_total;
        evaluateCrossings(G, GA, new_k, new_total);
        
        // Accept strictly better k, OR same k but fewer overall crossings
        if (new_k < current_k || (new_k == current_k && new_total < current_total)) {
            current_k = new_k;
            current_total = new_total;
            if (step % 100 == 0 || current_k == 0) {
                std::cout << "  -> Step " << step << " | Improvement! k: " << current_k << " | Total: " << current_total << "\n";
            }
        } else {
            // Undo if it wasn't an improvement
            GA.x(v) = old_x;
            GA.y(v) = old_y;
        }
        if (step % 5000 == 0 || current_k == 0) {
            std::cout << "  -> Step " << step << " | Update! k: " << current_k << " | Total: " << current_total << "\n";
        }
    }

    // for (int step = 0; step < iterations; ++step) {
    //     if (current_k == 0) break; // Perfect score achieved

    //     node v = node_list[dist_node(rng)];
    //     double old_x = GA.x(v);
    //     double old_y = GA.y(v);
        
    //     double new_x = dist_x(rng);
    //     double new_y = dist_y(rng);
        
    //     if (isOccupied(G, GA, v, new_x, new_y)) continue;
        
    //     GA.x(v) = new_x;
    //     GA.y(v) = new_y;
        
    //     int new_k, new_total;
    //     evaluateCrossings(G, GA, new_k, new_total);
        
    //     // Accept strictly better k, OR same k but fewer overall crossings
    //     if (new_k < current_k || (new_k == current_k && new_total < current_total)) {
    //         current_k = new_k;
    //         current_total = new_total;
    //         // Only print occasionally to not flood the console
    //         if (step % 100 == 0 || current_k == 0) {
    //             std::cout << "  -> Step " << step << " | Improvement! k: " << current_k << " | Total: " << current_total << "\n";
    //         }
    //     } else {
    //         // Undo
    //         GA.x(v) = old_x;
    //         GA.y(v) = old_y;
    //     }
    // }

    std::cout << "\n--- FINAL RESULTS ---" << std::endl;
    std::cout << "Graph: " << G.numberOfNodes() << " nodes, " << G.numberOfEdges() << " edges." << std::endl;
    std::cout << "Max Crossings (k): " << current_k << " | Total: " << current_total << std::endl;

    // ==========================================
    // EXPORT
    // ==========================================
    nlohmann::ordered_json out;
    out["nodes"] = json::array();
    for (node v : G.nodes) {
        out["nodes"].push_back({
            {"id", std::stoi(GA.label(v))}, 
            {"x", (int)GA.x(v)}, 
            {"y", (int)GA.y(v)}
        });
    }
    out["edges"] = j["edges"];
    out["width"] = grid_width;
    out["height"] = grid_height;

    std::ofstream o("submission-" + filepath);
    o << out.dump(4);
    std::cout << "Saved valid contest entry to submission-" + filepath << std::endl;

    return 0;
}