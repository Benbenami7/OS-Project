#ifndef GRAPH_H
#define GRAPH_H

#include <stdbool.h>

// Requirement: Assume at most 15 nodes
#define MAX_NODES 15

typedef struct {
    int num_nodes;
    int num_edges;
    // Adjacency matrix: -1 represents no edge
    int adj_matrix[MAX_NODES][MAX_NODES];
    int query_src;
    int query_dst;
} Graph;

// Function prototypes
Graph* create_graph(void);
void free_graph(Graph* g);
bool load_graph_from_file(Graph* g, const char* filename);
void run_dijkstra(const Graph* g);

#endif // GRAPH_H