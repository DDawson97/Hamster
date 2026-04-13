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
#include <unordered_map>
#include <algorithm>
#include <random>
#include <limits>
#include <filesystem>

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

bool isPointOnSegment(double px, double py, double ax, double ay, double bx, double by) {
    if (orientation(ax, ay, bx, by, px, py) != 0) return false;
    return px >= std::min(ax, bx) && px <= std::max(ax, bx) &&
           py >= std::min(ay, by) && py <= std::max(ay, by);
}

bool isMoveInvalid(const Graph& G, const GraphAttributes& GA, node v, double next_x, double next_y) {
    for (node u : G.nodes) {
        if (u != v && GA.x(u) == next_x && GA.y(u) == next_y) return true;
    }
    for (edge e : G.edges) {
        if (e->source() == v || e->target() == v) continue;
        if (isPointOnSegment(next_x, next_y, GA.x(e->source()), GA.y(e->source()), 
                             GA.x(e->target()), GA.y(e->target()))) return true;
    }
    for (adjEntry adj : v->adjEntries) {
        edge e = adj->theEdge();
        node other = e->opposite(v);
        for (node u : G.nodes) {
            if (u == v || u == other) continue;
            if (isPointOnSegment(GA.x(u), GA.y(u), next_x, next_y, GA.x(other), GA.y(other))) return true;
        }
    }
    return false; 
}

// --- OPTIMIZATION 1: SWEEP AND PRUNE EVALUATOR ---
struct SweepEdge {
    edge e;
    int idx;
    double minX, maxX, minY, maxY;
};

void fastEvaluateCrossings(const Graph& G, const GraphAttributes& GA, 
                           const std::vector<edge>& edges, 
                           const std::unordered_map<edge, int>& edge_idx,
                           std::vector<int>& edge_crossings, int& max_k, int& total_crossings) {
    total_crossings = 0;
    max_k = 0;
    std::fill(edge_crossings.begin(), edge_crossings.end(), 0);

    std::vector<SweepEdge> sweep_edges;
    sweep_edges.reserve(edges.size());
    for (edge e : edges) {
        double x1 = GA.x(e->source()), y1 = GA.y(e->source());
        double x2 = GA.x(e->target()), y2 = GA.y(e->target());
        sweep_edges.push_back({e, edge_idx.at(e), std::min(x1, x2), std::max(x1, x2), std::min(y1, y2), std::max(y1, y2)});
    }

    // Sort by left-most X coordinate
    std::sort(sweep_edges.begin(), sweep_edges.end(), [](const SweepEdge& a, const SweepEdge& b) {
        return a.minX < b.minX;
    });

    for (size_t i = 0; i < sweep_edges.size(); ++i) {
        for (size_t j = i + 1; j < sweep_edges.size(); ++j) {
            // THE PRUNE: If edge J starts after edge I completely finishes, break!
            if (sweep_edges[j].minX > sweep_edges[i].maxX) break; 
            
            // Fast Y bounding box check
            if (sweep_edges[i].maxY < sweep_edges[j].minY || sweep_edges[i].minY > sweep_edges[j].maxY) continue; 

            edge e1 = sweep_edges[i].e;
            edge e2 = sweep_edges[j].e;

            if (e1->source() == e2->source() || e1->source() == e2->target() ||
                e1->target() == e2->source() || e1->target() == e2->target()) {
                continue; 
            }

            if (doIntersect(GA.x(e1->source()), GA.y(e1->source()), GA.x(e1->target()), GA.y(e1->target()),
                            GA.x(e2->source()), GA.y(e2->source()), GA.x(e2->target()), GA.y(e2->target()))) {
                edge_crossings[sweep_edges[i].idx]++;
                edge_crossings[sweep_edges[j].idx]++;
                total_crossings++;
            }
        }
    }
    for (int k : edge_crossings) {
        if (k > max_k) max_k = k;
    }
}


int main(int argc, char* argv[]) {
    string fn = "225-nodes.json";
    string filepath = "tests/" + fn;
    if (argc > 1) filepath = argv[1], fn = filepath; // Accept file from Python

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
        GA.x(v) = n.value("x", 0.0);
        GA.y(v) = n.value("y", 0.0);
    }
    for (auto& e : j["edges"]) {
        G.newEdge(idToNode[e["source"]], idToNode[e["target"]]);
    }

    std::cout << "--- STARTING UNIFIED SOLVER ---" << std::endl;
    std::cout << "Graph: " << G.numberOfNodes() << " nodes, " << G.numberOfEdges() << " edges." << std::endl;
    std::cout << "Grid Limit: " << grid_width << "x" << grid_height << std::endl;

    // --- SETUP PRECOMPUTED EDGE ARRAYS FOR SPEED ---
    std::vector<edge> all_edges;
    std::unordered_map<edge, int> edge_idx;
    int e_counter = 0;
    for (edge e : G.edges) {
        all_edges.push_back(e);
        edge_idx[e] = e_counter++;
    }
    std::vector<int> current_crossings(all_edges.size(), 0);
    int current_k, current_total;

    // Run Initial Fast Eval
    fastEvaluateCrossings(G, GA, all_edges, edge_idx, current_crossings, current_k, current_total);
    std::cout << "Initial k: " << current_k << " | Total: " << current_total << std::endl;

    // ==========================================
    // PHASE 1: FMMM & PHASE 2: SNAPPING
    // ==========================================
    std::cout << "\n[Phase 1] Running Force-Directed Layout (FMMM)..." << std::endl;
    FMMMLayout fmmm; fmmm.useHighLevelOptions(true); fmmm.unitEdgeLength(100.0); fmmm.call(GA);

    std::cout << "[Phase 2] Snapping continuous layout to integer grid..." << std::endl;
    double min_x = std::numeric_limits<double>::max(), min_y = std::numeric_limits<double>::max();
    double cur_max_x = std::numeric_limits<double>::lowest(), cur_max_y = std::numeric_limits<double>::lowest();
    for (node v : G.nodes) {
        min_x = std::min(min_x, GA.x(v)); min_y = std::min(min_y, GA.y(v));
        cur_max_x = std::max(cur_max_x, GA.x(v)); cur_max_y = std::max(cur_max_y, GA.y(v));
    }

    double scale = 1.0;
    if (cur_max_x - min_x > 0 && cur_max_y - min_y > 0) {
        scale = std::min(grid_width / (cur_max_x - min_x), grid_height / (cur_max_y - min_y)) * 0.95;
    }

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist_x(0, grid_width);
    std::uniform_int_distribution<int> dist_y(0, grid_height);

    for (node v : G.nodes) {
        int nx = std::max(0, std::min(grid_width, (int)std::round((GA.x(v) - min_x) * scale)));
        int ny = std::max(0, std::min(grid_height, (int)std::round((GA.y(v) - min_y) * scale)));
        int attempts = 0;
        while (isMoveInvalid(G, GA, v, nx, ny) && attempts < 100) {
            nx = dist_x(rng); ny = dist_y(rng); attempts++;
        }
        GA.x(v) = nx; GA.y(v) = ny;
    }

    // ==========================================
    // PHASE 3: LOCAL OPTIMIZATION (Simulated Annealing)
    // ==========================================
    fastEvaluateCrossings(G, GA, all_edges, edge_idx, current_crossings, current_k, current_total);
    std::cout << "[Phase 3] Starting SA. Initial k: " << current_k << " | Total: " << current_total << std::endl;

    std::uniform_int_distribution<int> dist_node(0, node_list.size() - 1);
    std::uniform_int_distribution<int> dist_nudge(-3, 3);
    std::uniform_real_distribution<double> dist_choice(0.0, 1.0);

    // --- SA Parameters ---
    double initial_temp = 1000.0;
    double cooling_rate = 0.9998; // Cools down over 40,000 steps
    double temp = initial_temp;

    for (int step = 0; step < 40000; ++step) {
        if (current_k == 0) break; 

        if (step % 5000 == 0 || current_k == 0) {
            std::cout << "  -> Step " << step << " | Update! k: " << current_k << " | Total: " << current_total << "\n";
        }

        node v = node_list[dist_node(rng)];
        double old_x = GA.x(v); double old_y = GA.y(v);
        double new_x, new_y;

        if (dist_choice(rng) < 0.1) {
            new_x = std::max(0.0, std::min((double)grid_width, old_x + dist_nudge(rng)));
            new_y = std::max(0.0, std::min((double)grid_height, old_y + dist_nudge(rng)));
        } else {
            new_x = dist_x(rng); new_y = dist_y(rng);
        }
        
        if (isMoveInvalid(G, GA, v, new_x, new_y)) continue;

        // --- DELTA UPDATE (Keep your existing fast delta update code here) ---
        std::vector<int> new_crossings = current_crossings; 
        int new_total = current_total;
        
        for (adjEntry adj : v->adjEntries) {
            edge e_v = adj->theEdge();
            int ev_idx = edge_idx[e_v];
            for (edge e_other : all_edges) {
                if (e_other->source() == v || e_other->target() == v) continue;
                if (e_other->source() == e_v->opposite(v) || e_other->target() == e_v->opposite(v)) continue;
                if (doIntersect(old_x, old_y, GA.x(e_v->opposite(v)), GA.y(e_v->opposite(v)),
                                GA.x(e_other->source()), GA.y(e_other->source()), GA.x(e_other->target()), GA.y(e_other->target()))) {
                    new_crossings[ev_idx]--; new_crossings[edge_idx[e_other]]--; new_total--;
                }
            }
        }

        GA.x(v) = new_x; GA.y(v) = new_y;

        for (adjEntry adj : v->adjEntries) {
            edge e_v = adj->theEdge();
            int ev_idx = edge_idx[e_v];
            for (edge e_other : all_edges) {
                if (e_other->source() == v || e_other->target() == v) continue;
                if (e_other->source() == e_v->opposite(v) || e_other->target() == e_v->opposite(v)) continue;
                if (doIntersect(new_x, new_y, GA.x(e_v->opposite(v)), GA.y(e_v->opposite(v)),
                                GA.x(e_other->source()), GA.y(e_other->source()), GA.x(e_other->target()), GA.y(e_other->target()))) {
                    new_crossings[ev_idx]++; new_crossings[edge_idx[e_other]]++; new_total++;
                }
            }
        }

        int new_k = 0;
        for (int k_val : new_crossings) if (k_val > new_k) new_k = k_val;
        // ---------------------------------------------------------------------

        // --- NEW: SIMULATED ANNEALING ACCEPTANCE LOGIC ---
        double current_score = (current_k * 10000) + current_total;
        double new_score = (new_k * 10000) + new_total;
        double delta = new_score - current_score;

        bool accept = false;
        if (delta < 0) {
            accept = true; // Strictly better move
        } else {
            // It's a worse move. Calculate probability of accepting it anyway.
            double acceptance_probability = std::exp(-delta / temp);
            if (dist_choice(rng) < acceptance_probability) {
                accept = true;
            }
        }

        if (accept) {
            current_k = new_k; current_total = new_total;
            current_crossings = new_crossings; 
        } else {
            GA.x(v) = old_x; GA.y(v) = old_y; // Revert
        }

        temp *= cooling_rate; // Cool down the system
    }

    std::cout << "\n--- FINAL RESULTS ---\nGraph: " << G.numberOfNodes() << " nodes, " << G.numberOfEdges() << " edges.\nMax Crossings (k): " << current_k << " | Total: " << current_total << std::endl;

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

    std::ofstream o("submission-" + fn);
    o << out.dump(4);
    std::cout << "Saved valid contest entry to submission-" + fn << std::endl;

    return 0;
}