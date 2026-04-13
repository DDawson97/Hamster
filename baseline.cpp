#include <ogdf/basic/Graph.h>
#include <ogdf/basic/GraphAttributes.h>
#include <ogdf/fileformats/GraphIO.h>
#include <ogdf/energybased/FMMMLayout.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using namespace ogdf;
using json = nlohmann::json;

int main() {
    // 1. Load Sample Input
    std::ifstream i("../sample.json");
    json j;
    i >> j;

    Graph G;
    GraphAttributes GA(G,
    GraphAttributes::nodeGraphics |
    GraphAttributes::edgeGraphics |
    GraphAttributes::nodeStyle |   // Enables colors/borders
    GraphAttributes::nodeLabel      // Enables text labels
    );  
    
    double max_w = j.value("width", 1000.0);
    double max_h = j.value("height", 1000.0);

    // Map JSON IDs to OGDF nodes
    std::map<int, node> idToNode;

    // Seed the random number generator so we can scatter the nodes
    srand(time(0));

    for (auto& n : j["nodes"]) {
        node v = G.newNode();
        idToNode[n["id"]] = v;
        GA.x(v) = n.value("x", 0); // initial x (if exists)
        GA.y(v) = n.value("y", 0); // initial y

        // Set explicit node dimensions (e.g., 2% of the board size)
        GA.width(v) = max_w * 0.02;
        GA.height(v) = max_h * 0.02;
        // Default styling on nodes
        GA.shape(v) = ogdf::Shape::Rect;
        GA.fillColor(v) = Color::Name::Lightblue;
        GA.label(v) = std::to_string((int)n["id"]);
    }

    for (auto& e : j["edges"]) {
        G.newEdge(idToNode[e["source"]], idToNode[e["target"]]);
    }

    // 2. Initial Layout (using FMMM - Fast Multipole Multilevel Method)
    // FMMMLayout fmmm;
    // fmmm.useHighLevelOptions(true);
    // fmmm.unitEdgeLength(100); 
    // fmmm.call(GA);

    // 1. Find the current minimums and maximums manually (safest method)
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

    // 2. Calculate scaling factor (using the smaller ratio to maintain aspect ratio)
    if (current_w > 0 && current_h > 0) {
        double scaleX = max_w / current_w;
        double scaleY = max_h / current_h;
        double scale = std::min(scaleX, scaleY) * 0.95; // 0.95 gives a 5% margin

        // 3. Apply scaling, shift to positive coordinates, and snap to integer grid
        for (node v : G.nodes) {
            // Shift to 0, scale, then add a small offset for the margin
            double new_x = (GA.x(v) - min_x) * scale + (max_w * 0.025);
            double new_y = (GA.y(v) - min_y) * scale + (max_h * 0.025);

            GA.x(v) = std::round(new_x);
            GA.y(v) = std::round(new_y);
        }
    }

    // 4. EXPORT FOR SUBMISSION FIRST
    nlohmann::ordered_json out;

    // Export the newly calculated node positions
    out["nodes"] = json::array(); // Initialize as an empty JSON array
    for (node v : G.nodes) {
        out["nodes"].push_back({
            {"id", std::stoi(GA.label(v))}, 
            {"x", (int)GA.x(v)}, 
            {"y", (int)GA.y(v)}
        });
    }

    // Copy the edges exactly as they came in
    out["edges"] = j["edges"];

    // Copy the board dimensions
    out["width"] = j["width"];
    out["height"] = j["height"];

    std::ofstream o("submission.json");
    o << out.dump(4);
    std::cout << "Successfully wrote submission.json" << std::endl;

    // ---------------------------------------------------------
    // 5. VISUALIZE FOR DEBUGGING
    // (Scale everything up x100 so the default font size looks normal)
    // ---------------------------------------------------------
    double visual_scale = 100.0;
    
    for (node v : G.nodes) {
        GA.x(v) *= visual_scale;
        GA.y(v) *= visual_scale;
        
        // Give them a nice readable physical size for the SVG
        GA.width(v) = 30.0;
        GA.height(v) = 30.0;
    }

    GraphIO::drawSVG(GA, "output_visualization.svg");
    std::cout << "Successfully wrote output_visualization.svg" << std::endl;

    return 0;
}