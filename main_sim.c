#include <stdio.h>
#include <stdlib.h>
#include "graph.h"
#include "gui.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./sim <file_name>\n");
        return EXIT_FAILURE;
    }

    Graph* g = create_graph();
    if (!g) {
        return EXIT_FAILURE;
    }

    if (!load_graph_from_file(g, argv[1])) {
        free_graph(g);
        return EXIT_FAILURE;
    }

    // Prints the same Dijkstra result in the terminal and then opens the GUI simulation.
    run_dijkstra(g);
    run_gui(g);

    free_graph(g);
    return EXIT_SUCCESS;
}
