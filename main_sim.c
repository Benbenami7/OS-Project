#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "graph.h"
#include "gui.h"

#ifndef SIM_MILESTONE
#define SIM_MILESTONE 3
#endif

int main(int argc, char* argv[]) {
// =================================================================
// MILESTONE 7: ARGUMENT PARSING (Scheduler type + Filename)
// =================================================================
#if SIM_MILESTONE == 7
    if (argc != 3) {
        fprintf(stderr, "Usage: ./sim-schd <fcfs|sjf> <file_name>\n");
        return EXIT_FAILURE;
    }
    const char* algo = argv[1];
    const char* filename = argv[2];
    
    // Validate the chosen scheduling algorithm
    if (strcmp(algo, "fcfs") != 0 && strcmp(algo, "sjf") != 0) {
        fprintf(stderr, "Error: Invalid scheduling algorithm. Use 'fcfs' or 'sjf'.\n");
        return EXIT_FAILURE;
    }
#else
    if (argc != 2) {
        fprintf(stderr, "Usage: ./sim <file_name>\n");
        return EXIT_FAILURE;
    }
    const char* filename = argv[1];
#endif

    Graph* g = create_graph();
    if (!g) {
        return EXIT_FAILURE;
    }

// =================================================================
// MILESTONES 4, 5, 6, 7: Extended data loading (Graph + Travelers)
// =================================================================
#if SIM_MILESTONE >= 4
    TravelerRequest* travelers = NULL;
    int traveler_count = 0;

    if (!load_graph_and_travelers(filename, g, &travelers, &traveler_count)) {
        free_graph(g);
        return EXIT_FAILURE;
    }
#else
// =================================================================
// MILESTONES 1, 2, 3: Basic data loading (Graph and Source-Destination query only)
// =================================================================
    if (!load_graph_from_file(g, filename)) {
        free_graph(g);
        return EXIT_FAILURE;
    }
#endif

// =================================================================
// Milestone execution routing (The core logic of the project is triggered here)
// =================================================================
#if SIM_MILESTONE == 4
    // MILESTONE 4: Fork, Processes & Signals
    run_gui_milestone4(g, travelers, traveler_count);

#elif SIM_MILESTONE == 5
    // MILESTONE 5: IPC (Pipes/Shared Memory) & Terminal Logging
    run_gui_milestone5(g, travelers, traveler_count);

#elif SIM_MILESTONE == 6
    // MILESTONE 6: Synchronization (Semaphores/Mutex) & Critical Sections
    run_gui_milestone6(g, travelers, traveler_count);

#elif SIM_MILESTONE == 7
    // MILESTONE 7: Parent-managed Scheduling (FCFS/SJF)
    run_gui_milestone7(g, travelers, traveler_count, algo);

#else
    // MILESTONES 1, 2, 3: Dijkstra Algorithm, Static GUI & Animation Timer
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
