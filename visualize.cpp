#include <ogdf/basic/Graph.h>
#include <ogdf/basic/GraphAttributes.h>
#include <ogdf/fileformats/GraphIO.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using namespace ogdf;
using json = nlohmann::json;

int main() {
    // 1. Load the exact input file
    std::ifstream i("../sample.json");
    if (!i.is_open()) {
        std::cerr << "Error: Could not open sample.json" << std::endl;
        return 1;
    }
    json j;
    i >> j;

    Graph G;
    GraphAttributes GA(G,
        GraphAttributes::nodeGraphics |
        GraphAttributes::edgeGraphics |
        GraphAttributes::nodeStyle |   
        GraphAttributes::nodeLabel      
    );  
    
    std::map<int, node> idToNode;

    // 2. Set our "Viewing Window" scale
    // This strictly stretches the canvas for the SVG so the default font size fits.
    double visual_scale = 100.0; 

    // 3. Map JSON nodes exactly as they appear in the file
    for (auto& n : j["nodes"]) {
        node v = G.newNode();
        idToNode[n["id"]] = v;
        
        // Multiply original coordinates by the visual scale for rendering
        GA.x(v) = (double)n["x"] * visual_scale; 
        GA.y(v) = (double)n["y"] * visual_scale; 

        // Give the nodes a fixed, reasonable size for the screen
        GA.width(v) = 30.0;
        GA.height(v) = 30.0;
        
        // Default styling
        GA.shape(v) = ogdf::Shape::Rect;
        GA.fillColor(v) = Color::Name::Lightblue;
        GA.label(v) = std::to_string((int)n["id"]);
    }

    // 4. Map the edges exactly as they appear
    for (auto& e : j["edges"]) {
        G.newEdge(idToNode[e["source"]], idToNode[e["target"]]);
    }

    // 5. Generate the SVG immediately
    GraphIO::drawSVG(GA, "input_visualization.svg");
    std::cout << "Successfully wrote input_visualization.svg" << std::endl;

    // Export the full graph topology and visual attributes to a GML file
    GraphIO::write(GA, "input_visualization.gml");
    std::cout << "Successfully wrote input_visualization.gml" << std::endl;

    return 0;
}