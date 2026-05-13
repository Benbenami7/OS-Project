#include <stdio.h>
#include <stdlib.h>
#include "graph.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./dijkstra <file_name>\n");
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

    run_dijkstra(g);
    free_graph(g);
    return EXIT_SUCCESS;
}
