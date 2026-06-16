#include <stdio.h>
#include <stdlib.h>
#include "graph.h"
#include "gui.h"

#ifndef SIM_MILESTONE
#define SIM_MILESTONE 3
#endif

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./sim <file_name>\n");
        return EXIT_FAILURE;
    }

    Graph* g = create_graph();
    if (!g) {
        return EXIT_FAILURE;
    }

#if SIM_MILESTONE >= 4
    TravelerRequest* travelers = NULL;
    int traveler_count = 0;

    if (!load_graph_and_travelers(argv[1], g, &travelers, &traveler_count)) {
        free_graph(g);
        return EXIT_FAILURE;
    }
#else
    if (!load_graph_from_file(g, argv[1])) {
        free_graph(g);
        return EXIT_FAILURE;
    }
#endif

#if SIM_MILESTONE == 4
    run_gui_milestone4(g, travelers, traveler_count);
#elif SIM_MILESTONE == 5
    run_gui_milestone5(g, travelers, traveler_count);
#elif SIM_MILESTONE == 6
    run_gui_milestone6(g, travelers, traveler_count);
#else
    // Prints the same Dijkstra result in the terminal and then opens the GUI simulation.
    run_dijkstra(g);
    run_gui(g);
#endif

#if SIM_MILESTONE >= 4
    free(travelers);
#endif
    free_graph(g);
    return EXIT_SUCCESS;
}
