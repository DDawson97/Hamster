#include <ogdf/basic/Graph.h>
#include <ogdf/basic/GraphAttributes.h>
#include <ogdf/misclayout/CircularLayout.h>
#include <ogdf/fileformats/GraphIO.h>
#include <iostream>

using namespace ogdf;

int main() {
    // 1. Create an empty graph
    Graph G;

    // 2. Enable attributes for graphics (so we can draw it)
    GraphAttributes GA(G,
        GraphAttributes::nodeGraphics |
        GraphAttributes::edgeGraphics);

    // 3. Add nodes
    node n1 = G.newNode();
    node n2 = G.newNode();
    node n3 = G.newNode();
    node n4 = G.newNode();

    // 4. Add edges to create a triangle
    G.newEdge(n1, n2);
    G.newEdge(n2, n3);
    G.newEdge(n3, n1);

    // Set a default size for our nodes so they show up clearly
    for (node v : G.nodes) {
        GA.width(v) = 2.0;
        GA.height(v) = 2.0;
    }

    // 5. Apply a layout algorithm (Circular Layout)
    CircularLayout layout;
    layout.call(GA);

    // 6. Export the graph to an SVG file
    if (GraphIO::write(GA, "output_graph.svg", GraphIO::drawSVG)) {
        std::cout << "Success! Check output_graph.svg in your folder." << std::endl;
    } else {
        std::cerr << "Failed to write SVG file." << std::endl;
    }

    return 0;
}