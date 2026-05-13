#include <stdio.h>
#include <stdlib.h>
#include "graph.h"
#include "gui.h"

int main(int argc, char* argv[]) {
    // Default file name as requested or accept via terminal arguments
    const char* filename = "graph.txt";
    if (argc > 1) {
        filename = argv[1];
    }

    Graph* g = create_graph();
    if (!g) {
        return EXIT_FAILURE;
    }

    // Execute Step 1: Read input file cleanly
    if (!load_graph_from_file(g, filename)) {
        free_graph(g);
        return EXIT_FAILURE;
    }

    // Execute Step 1: Run Dijkstra algorithm and output strictly formatted results
    run_dijkstra(g);

    // Execute Step 2: Open graphical visualization window
    run_gui(g);

    // Clean up allocated memory securely
    free_graph(g);
    return EXIT_SUCCESS;
}