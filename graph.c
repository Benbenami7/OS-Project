#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "graph.h"

Graph* create_graph(void) {
    Graph* g = (Graph*)malloc(sizeof(Graph));
    if (!g) {
        perror("Memory allocation failed for Graph");
        return NULL;
    }
    g->num_nodes = 0;
    g->num_edges = 0;
    g->query_src = 0;
    g->query_dst = 0;

    // Initialize adjacency matrix with -1 (no edge)
    for (int i = 0; i < MAX_NODES; i++) {
        for (int j = 0; j < MAX_NODES; j++) {
            g->adj_matrix[i][j] = -1;
        }
    }
    return g;
}

void free_graph(Graph* g) {
    if (g) {
        free(g);
    }
}

bool load_graph_from_file(Graph* g, const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening input file");
        return false;
    }

    // Read number of nodes and edges
    if (fscanf(file, "%d %d", &g->num_nodes, &g->num_edges) != 2) {
        fprintf(stderr, "Error: Failed to read nodes and edges count.\n");
        fclose(file);
        return false;
    }

    if (g->num_nodes <= 0 || g->num_nodes > MAX_NODES) {
        fprintf(stderr, "Error: Invalid number of nodes. Must be between 1 and %d.\n", MAX_NODES);
        fclose(file);
        return false;
    }

    // Read all edges
    for (int i = 0; i < g->num_edges; i++) {
        int u, v, w;
        if (fscanf(file, "%d %d %d", &u, &v, &w) != 3) {
            fprintf(stderr, "Error: Failed to read edge details at line %d.\n", i + 2);
            fclose(file);
            return false;
        }

        if (u < 0 || u >= g->num_nodes || v < 0 || v >= g->num_nodes) {
            fprintf(stderr, "Error: Node indices out of bounds (%d -> %d).\n", u, v);
            fclose(file);
            return false;
        }

        // Requirement: Negative weights are invalid input
        if (w < 0) {
            fprintf(stderr, "Error: Negative edge weight detected (%d). Invalid input.\n", w);
            fclose(file);
            return false;
        }

        g->adj_matrix[u][v] = w;
    }

    // Read the source and destination query for Dijkstra
    if (fscanf(file, "%d %d", &g->query_src, &g->query_dst) != 2) {
        fprintf(stderr, "Error: Failed to read Dijkstra query nodes.\n");
        fclose(file);
        return false;
    }

    if (g->query_src < 0 || g->query_src >= g->num_nodes ||
        g->query_dst < 0 || g->query_dst >= g->num_nodes) {
        fprintf(stderr, "Error: Query nodes out of bounds.\n");
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
}

void run_dijkstra(const Graph* g) {
    int src = g->query_src;
    int dst = g->query_dst;
    int n = g->num_nodes;

    int dist[MAX_NODES];
    bool visited[MAX_NODES];
    int parent[MAX_NODES];

    for (int i = 0; i < n; i++) {
        dist[i] = INT_MAX;
        visited[i] = false;
        parent[i] = -1;
    }

    dist[src] = 0;

    for (int count = 0; count < n; count++) {
        int min_dist = INT_MAX;
        int u = -1;

        // Find the unvisited node with the smallest distance
        for (int i = 0; i < n; i++) {
            if (!visited[i] && dist[i] < min_dist) {
                min_dist = dist[i];
                u = i;
            }
        }

        // If remaining nodes are unreachable
        if (u == -1) break;

        visited[u] = true;

        // Update distance of adjacent nodes
        for (int v = 0; v < n; v++) {
            if (g->adj_matrix[u][v] != -1 && !visited[v]) {
                if (dist[u] != INT_MAX && dist[u] + g->adj_matrix[u][v] < dist[v]) {
                    dist[v] = dist[u] + g->adj_matrix[u][v];
                    parent[v] = u;
                }
            }
        }
    }

    // Requirement output formatting
    if (dist[dst] == INT_MAX) {
        printf("No path found\n");
    } else {
        // Reconstruct path using the parent array
        int path[MAX_NODES];
        int path_len = 0;
        int curr = dst;

        while (curr != -1) {
            path[path_len++] = curr;
            curr = parent[curr];
        }

        // Print path in correct start-to-end order
        for (int i = path_len - 1; i >= 0; i--) {
            printf("%d", path[i]);
            if (i > 0) printf(" -> ");
        }
        printf("\n%d\n", dist[dst]);
    }
}