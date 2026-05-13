#include <raylib.h>
#include <math.h>
#include <stdio.h>
#include "gui.h"
#include "graph.h"

#define RADIUS 32.0f
#define FONT_SIZE 20

// Internal vector math helpers to avoid specific raymath.h dependencies
static Vector2 v_sub(Vector2 a, Vector2 b) {
    return (Vector2){ a.x - b.x, a.y - b.y };
}

static Vector2 v_add(Vector2 a, Vector2 b) {
    return (Vector2){ a.x + b.x, a.y + b.y };
}

static Vector2 v_scale(Vector2 a, float s) {
    return (Vector2){ a.x * s, a.y * s };
}

static float v_len(Vector2 a) {
    return sqrtf(a.x * a.x + a.y * a.y);
}

static Vector2 v_norm(Vector2 a) {
    float len = v_len(a);
    if (len == 0.0f) return (Vector2){ 0.0f, 0.0f };
    return v_scale(a, 1.0f / len);
}

// Draws a directed edge with arrowhead and weight text cleanly
static void draw_directed_edge(Vector2 start_pos, Vector2 end_pos, int weight) {
    Vector2 dir = v_sub(end_pos, start_pos);
    float length = v_len(dir);

    // Do not draw if nodes overlap or are extremely close
    if (length <= 2.0f * RADIUS) return;

    dir = v_norm(dir);

    // Calculate perpendicular vector to offset dual-directional edges
    Vector2 perp = { -dir.y, dir.x };
    float offset = 8.0f;

    // Adjust start and end positions to touch the outer border of the nodes
    Vector2 start = v_add(start_pos, v_scale(dir, RADIUS));
    start = v_add(start, v_scale(perp, offset));

    Vector2 end = v_sub(end_pos, v_scale(dir, RADIUS));
    end = v_add(end, v_scale(perp, offset));

    // Draw main connecting line
    DrawLineEx(start, end, 2.5f, DARKGRAY);

    // Calculate arrowhead triangle coordinates
    float arrow_size = 14.0f;
    Vector2 base = v_sub(end, v_scale(dir, arrow_size));
    Vector2 p1 = v_add(base, v_scale(perp, arrow_size * 0.5f));
    Vector2 p2 = v_sub(base, v_scale(perp, arrow_size * 0.5f));

    DrawTriangle(end, p1, p2, MAROON);

    // Position the weight text near the midpoint, slightly shifted to stay readable
    Vector2 mid = v_scale(v_add(start, end), 0.5f);
    Vector2 text_pos = v_add(mid, v_scale(perp, 14.0f));

    char weight_str[16];
    sprintf(weight_str, "%d", weight);
    DrawText(weight_str, (int)text_pos.x - 5, (int)text_pos.y - 8, 18, DARKBLUE);
}

void run_gui(Graph* g) {
    const int screenWidth = 900;
    const int screenHeight = 750;

    // Enable Multi-Sample Anti-Aliasing for smooth UI lines
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "OS Project - Directed Graph GUI");

    // Automatically distribute nodes in a highly readable circular layout
    Vector2 positions[MAX_NODES];
    Vector2 center = { screenWidth / 2.0f, screenHeight / 2.0f };
    float layout_radius = 260.0f;

    for (int i = 0; i < g->num_nodes; i++) {
        float angle = i * (2.0f * PI / g->num_nodes);
        positions[i].x = center.x + cosf(angle) * layout_radius;
        positions[i].y = center.y + sinf(angle) * layout_radius;
    }

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Milestone 2: Graph GUI Display", 20, 20, 22, DARKGRAY);
        DrawText("Close window or press ESC to exit", 20, 50, 14, GRAY);

        // Render all edges first so they sit cleanly behind nodes
        for (int u = 0; u < g->num_nodes; u++) {
            for (int v = 0; v < g->num_nodes; v++) {
                if (g->adj_matrix[u][v] != -1) {
                    draw_directed_edge(positions[u], positions[v], g->adj_matrix[u][v]);
                }
            }
        }

        // Render nodes with border and centered ID text
        for (int i = 0; i < g->num_nodes; i++) {
            DrawCircleV(positions[i], RADIUS, SKYBLUE);
            DrawCircleLines((int)positions[i].x, (int)positions[i].y, RADIUS, BLUE);

            char id_str[8];
            sprintf(id_str, "%d", i);
            int text_width = MeasureText(id_str, FONT_SIZE);
            DrawText(id_str, (int)positions[i].x - text_width / 2, (int)positions[i].y - FONT_SIZE / 2, FONT_SIZE, BLACK);
        }

        EndDrawing();
    }

    CloseWindow();
}