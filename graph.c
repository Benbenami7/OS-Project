#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include "graph.h"

#define INPUT_LINE_SIZE 256

// =================================================================
// MILESTONE 1: GRAPH MEMORY MANAGEMENT
// =================================================================
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

    for (int i = 0; i < MAX_NODES; i++) {
        for (int j = 0; j < MAX_NODES; j++) {
            g->adj_matrix[i][j] = -1;
        }
    }

    return g;
}

void free_graph(Graph* g) {
    free(g);
}

// =================================================================
// MILESTONE 1/4/5: FILE PARSING
// Reads the graph.txt and travelers data line by line
// =================================================================
static bool read_data_line(FILE* file, char line[], size_t line_size) {
    while (fgets(line, (int)line_size, file)) {
        char* p = line;
        while (isspace((unsigned char)*p)) {
            p++;
        }

        if (*p == '\0' || *p == '\n' || *p == '#') {
            continue;
        }

        if (p != line) {
            memmove(line, p, strlen(p) + 1);
        }
        return true;
    }

    return false;
}

static bool validate_graph_shape(const Graph* g) {
    if (g->num_nodes <= 0 || g->num_nodes > MAX_NODES) {
        fprintf(stderr, "Error: Invalid number of nodes. Must be between 1 and %d.\n", MAX_NODES);
        return false;
    }

    if (g->num_edges < 0 || g->num_edges > g->num_nodes * g->num_nodes) {
        fprintf(stderr, "Error: Invalid number of edges.\n");
        return false;
    }

    return true;
}

bool load_graph_and_travelers(const char* filename, Graph* g,
                              TravelerRequest** travelers, int* traveler_count) {
    if (!g || !filename || !travelers || !traveler_count) {
        fprintf(stderr, "Error: Invalid graph, filename, or traveler output.\n");
        return false;
    }

    *travelers = NULL;
    *traveler_count = 0;

    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening input file");
        return false;
    }

    char line[INPUT_LINE_SIZE];
    if (!read_data_line(file, line, sizeof(line)) ||
        sscanf(line, "%d %d", &g->num_nodes, &g->num_edges) != 2) {
        fprintf(stderr, "Error: Failed to read nodes and edges count.\n");
        fclose(file);
        return false;
    }

    if (!validate_graph_shape(g)) {
        fclose(file);
        return false;
    }

    for (int i = 0; i < g->num_edges; i++) {
        int u, v, w;
        if (!read_data_line(file, line, sizeof(line)) ||
            sscanf(line, "%d %d %d", &u, &v, &w) != 3) {
            fprintf(stderr, "Error: Failed to read edge details at line %d.\n", i + 2);
            fclose(file);
            return false;
        }

        if (u < 0 || u >= g->num_nodes || v < 0 || v >= g->num_nodes) {
            fprintf(stderr, "Error: Node indices out of bounds (%d -> %d).\n", u, v);
            fclose(file);
            return false;
        }

        if (w < 0) {
            fprintf(stderr, "Error: Negative edge weight detected (%d). Invalid input.\n", w);
            fclose(file);
            return false;
        }

        g->adj_matrix[u][v] = w;
    }

    if (!read_data_line(file, line, sizeof(line))) {
        fprintf(stderr, "Error: Failed to read traveler count or Dijkstra query nodes.\n");
        fclose(file);
        return false;
    }

    int first = 0;
    int second = 0;
    char extra = '\0';
    int count = sscanf(line, "%d %d %c", &first, &second, &extra);

    if (count >= 2) {
        *traveler_count = 1;
        *travelers = (TravelerRequest*)malloc(sizeof(TravelerRequest));
        if (!*travelers) {
            perror("Memory allocation failed for travelers");
            fclose(file);
            return false;
        }

        (*travelers)[0].source = first;
        (*travelers)[0].destination = second;
        g->query_src = first;
        g->query_dst = second;
    } else if (count == 1) {
        int k = first;
        if (k < 0 || k > MAX_TRAVELERS) {
            fprintf(stderr, "Error: Invalid traveler count. Must be between 0 and %d.\n", MAX_TRAVELERS);
            fclose(file);
            return false;
        }

        *traveler_count = k;
        if (k > 0) {
            *travelers = (TravelerRequest*)calloc((size_t)k, sizeof(TravelerRequest));
            if (!*travelers) {
                perror("Memory allocation failed for travelers");
                fclose(file);
                return false;
            }
        }

        for (int i = 0; i < k; i++) {
            if (!read_data_line(file, line, sizeof(line)) ||
                sscanf(line, "%d %d", &(*travelers)[i].source, &(*travelers)[i].destination) != 2) {
                fprintf(stderr, "Error: Failed to read traveler %d.\n", i);
                free(*travelers);
                *travelers = NULL;
                *traveler_count = 0;
                fclose(file);
                return false;
            }
        }

        if (k > 0) {
            g->query_src = (*travelers)[0].source;
            g->query_dst = (*travelers)[0].destination;
        }
    } else {
        fprintf(stderr, "Error: Invalid traveler count or Dijkstra query nodes.\n");
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
}

bool load_graph_from_file(Graph* g, const char* filename) {
    TravelerRequest* travelers = NULL;
    int traveler_count = 0;

    if (!load_graph_and_travelers(filename, g, &travelers, &traveler_count)) {
        return false;
    }

    if (traveler_count <= 0) {
        fprintf(stderr, "Error: Missing Dijkstra query nodes.\n");
        free(travelers);
        return false;
    }

    if (g->query_src < 0 || g->query_src >= g->num_nodes ||
        g->query_dst < 0 || g->query_dst >= g->num_nodes) {
        fprintf(stderr, "Error: Query nodes out of bounds.\n");
        free(travelers);
        return false;
    }

    free(travelers);
    return true;
}

// =================================================================
// MILESTONE 1: CORE DIJKSTRA ALGORITHM (EXAM HOTSPOT)
// If you need to change how paths are calculated, weights are added,
// or nodes are filtered, do it inside this function!
// =================================================================
bool get_dijkstra_path_between(const Graph* g, int src, int dst,
                               int path[], int* path_len, int* total_weight) {
    if (!g || !path || !path_len || !total_weight) {
        return false;
    }

    int n = g->num_nodes;

    if (src < 0 || src >= n || dst < 0 || dst >= n) {
        *path_len = 0;
        *total_weight = 0;
        return false;
    }

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

        if (u == -1) {
            break;
        }

        visited[u] = true;

        if (u == dst) {
            break;
        }

        // Update distances of adjacent vertices
        for (int v = 0; v < n; v++) {
            int weight = g->adj_matrix[u][v];
            if (weight != -1 && !visited[v] && dist[u] != INT_MAX) {
                if (dist[u] <= INT_MAX - weight && dist[u] + weight < dist[v]) {
                    dist[v] = dist[u] + weight;
                    parent[v] = u;
                }
            }
        }
    }

    if (dist[dst] == INT_MAX) {
        *path_len = 0;
        *total_weight = 0;
        return false;
    }

    // Reconstruct the path backwards
    int reversed[MAX_NODES];
    int len = 0;
    int curr = dst;

    while (curr != -1 && len < MAX_NODES) {
        reversed[len++] = curr;
        curr = parent[curr];
    }

    // Reverse the array to get the correct path order
    for (int i = 0; i < len; i++) {
        path[i] = reversed[len - 1 - i];
    }

    *path_len = len;
    *total_weight = dist[dst];
    return true;
}

bool get_dijkstra_path(const Graph* g, int path[], int* path_len, int* total_weight) {
    if (!g) {
        return false;
    }

    return get_dijkstra_path_between(g, g->query_src, g->query_dst,
                                     path, path_len, total_weight);
}

void run_dijkstra(const Graph* g) {
    int path[MAX_NODES];
    int path_len = 0;
    int total_weight = 0;

    if (!get_dijkstra_path(g, path, &path_len, &total_weight)) {
        printf("No path found\n");
        return;
    }

    for (int i = 0; i < path_len; i++) {
        printf("%d", path[i]);
        if (i < path_len - 1) {
            printf(" -> ");
        }
    }
    printf("\n%d\n", total_weight);
}
