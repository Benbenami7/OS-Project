#ifndef GRAPH_H
#define GRAPH_H

#include <stdbool.h>

// Requirement: assume at most 15 nodes
#define MAX_NODES 15

// -1 means no edge in the adjacency matrix
typedef struct {
    int num_nodes;
    int num_edges;
    int adj_matrix[MAX_NODES][MAX_NODES];
    int query_src;
    int query_dst;
} Graph;

Graph* create_graph(void);
void free_graph(Graph* g);

bool load_graph_from_file(Graph* g, const char* filename);

// Milestone 1: prints the shortest path and total weight exactly as required.
void run_dijkstra(const Graph* g);

// Milestone 3: calculates the shortest path and returns it for the GUI animation.
// Returns false when no path exists.
bool get_dijkstra_path(const Graph* g, int path[], int* path_len, int* total_weight);

#endif // GRAPH_H
