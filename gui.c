#include <raylib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"
#include "graph.h"

#define NODE_RADIUS 32.0f
#define NODE_FONT_SIZE 20
#define ENTITY_RADIUS 13.0f
#define JUMP_DURATION 0.30f
#define NODE_WAIT_SECONDS 1.0f

typedef enum {
    SIM_AT_NODE,
    SIM_MOVING,
    SIM_FINISHED
} SimState;

static Vector2 v_add(Vector2 a, Vector2 b) {
    return (Vector2){ a.x + b.x, a.y + b.y };
}

static Vector2 v_sub(Vector2 a, Vector2 b) {
    return (Vector2){ a.x - b.x, a.y - b.y };
}

static Vector2 v_scale(Vector2 a, float s) {
    return (Vector2){ a.x * s, a.y * s };
}

static float v_len(Vector2 a) {
    return sqrtf(a.x * a.x + a.y * a.y);
}

static Vector2 v_norm(Vector2 a) {
    float len = v_len(a);
    if (len <= 0.0001f) {
        return (Vector2){ 0.0f, 0.0f };
    }
    return v_scale(a, 1.0f / len);
}

static Vector2 v_lerp(Vector2 a, Vector2 b, float t) {
    return (Vector2){ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
}

static void draw_centered_text(const char* text, int center_x, int center_y, int font_size, Color color) {
    int width = MeasureText(text, font_size);
    DrawText(text, center_x - width / 2, center_y - font_size / 2, font_size, color);
}

static void draw_button(Rectangle button, const char* text, bool enabled) {
    Color fill = enabled ? LIGHTGRAY : (Color){ 210, 210, 210, 255 };
    Color border = enabled ? DARKGRAY : GRAY;
    Color text_color = enabled ? BLACK : GRAY;

    DrawRectangleRec(button, fill);
    DrawRectangleLinesEx(button, 2.0f, border);
    draw_centered_text(text,
                       (int)(button.x + button.width / 2.0f),
                       (int)(button.y + button.height / 2.0f),
                       20,
                       text_color);
}

static void draw_directed_edge(Vector2 start_pos, Vector2 end_pos, int weight) {
    Vector2 dir = v_sub(end_pos, start_pos);
    float length = v_len(dir);

    if (length <= 2.0f * NODE_RADIUS) {
        return;
    }

    dir = v_norm(dir);
    Vector2 perp = { -dir.y, dir.x };
    float offset = 8.0f;

    Vector2 start = v_add(start_pos, v_scale(dir, NODE_RADIUS));
    start = v_add(start, v_scale(perp, offset));

    Vector2 end = v_sub(end_pos, v_scale(dir, NODE_RADIUS));
    end = v_add(end, v_scale(perp, offset));

    DrawLineEx(start, end, 2.5f, DARKGRAY);

    float arrow_size = 14.0f;
    Vector2 base = v_sub(end, v_scale(dir, arrow_size));
    Vector2 p1 = v_add(base, v_scale(perp, arrow_size * 0.5f));
    Vector2 p2 = v_sub(base, v_scale(perp, arrow_size * 0.5f));
    DrawTriangle(end, p1, p2, MAROON);

    Vector2 mid = v_scale(v_add(start, end), 0.5f);
    Vector2 text_pos = v_add(mid, v_scale(perp, 16.0f));

    char weight_str[16];
    snprintf(weight_str, sizeof(weight_str), "%d", weight);

    DrawCircle((int)text_pos.x, (int)text_pos.y, 12.0f, RAYWHITE);
    DrawText(weight_str, (int)text_pos.x - MeasureText(weight_str, 18) / 2, (int)text_pos.y - 9, 18, DARKBLUE);
}

static void draw_graph(const Graph* g, const Vector2 positions[]) {
    for (int u = 0; u < g->num_nodes; u++) {
        for (int v = 0; v < g->num_nodes; v++) {
            if (g->adj_matrix[u][v] != -1) {
                draw_directed_edge(positions[u], positions[v], g->adj_matrix[u][v]);
            }
        }
    }

    for (int i = 0; i < g->num_nodes; i++) {
        DrawCircleV(positions[i], NODE_RADIUS, SKYBLUE);
        DrawCircleLines((int)positions[i].x, (int)positions[i].y, NODE_RADIUS, BLUE);

        char id_str[8];
        snprintf(id_str, sizeof(id_str), "%d", i);
        draw_centered_text(id_str, (int)positions[i].x, (int)positions[i].y, NODE_FONT_SIZE, BLACK);
    }
}

static void draw_path_text(const int path[], int path_len, int total_weight, bool has_path) {
    if (!has_path) {
        DrawText("No path found", 20, 140, 22, RED);
        return;
    }

    char path_text[256] = "Path: ";
    char part[24];

    for (int i = 0; i < path_len; i++) {
        snprintf(part, sizeof(part), "%d", path[i]);
        strncat(path_text, part, sizeof(path_text) - strlen(path_text) - 1);
        if (i < path_len - 1) {
            strncat(path_text, " -> ", sizeof(path_text) - strlen(path_text) - 1);
        }
    }

    DrawText(path_text, 20, 140, 20, DARKGRAY);

    char weight_text[64];
    snprintf(weight_text, sizeof(weight_text), "Total weight: %d", total_weight);
    DrawText(weight_text, 20, 168, 20, DARKGRAY);
}

static void reset_simulation(const int path[], const Vector2 positions[], Vector2* entity_pos,
                             SimState* state, int* edge_index, int* jump_index,
                             float* state_timer, bool* finished) {
    *entity_pos = positions[path[0]];
    *state = SIM_AT_NODE;
    *edge_index = 0;
    *jump_index = 0;
    *state_timer = 0.0f;
    *finished = false;
}

void run_gui(Graph* g) {
    const int screen_width = 950;
    const int screen_height = 780;

    int path[MAX_NODES];
    int path_len = 0;
    int total_weight = 0;
    bool has_path = get_dijkstra_path(g, path, &path_len, &total_weight);

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screen_width, screen_height, "OS Project - Milestone 3 Graph Simulation");
    SetTargetFPS(60);

    Vector2 positions[MAX_NODES];
    Vector2 center = { screen_width / 2.0f, screen_height / 2.0f + 25.0f };
    float layout_radius = 265.0f;

    for (int i = 0; i < g->num_nodes; i++) {
        float angle = -PI / 2.0f + i * (2.0f * PI / g->num_nodes);
        positions[i].x = center.x + cosf(angle) * layout_radius;
        positions[i].y = center.y + sinf(angle) * layout_radius;
    }

    Rectangle play_button = { 20.0f, 78.0f, 125.0f, 42.0f };

    bool playing = false;
    bool finished = false;
    SimState state = SIM_AT_NODE;
    int edge_index = 0;
    int jump_index = 0;
    float state_timer = 0.0f;
    Vector2 entity_pos = has_path ? positions[path[0]] : (Vector2){ 0.0f, 0.0f };

    if (has_path && path_len == 1) {
        finished = true;
        state = SIM_FINISHED;
    }

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (has_path && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            CheckCollisionPointRec(GetMousePosition(), play_button)) {
            if (finished) {
                reset_simulation(path, positions, &entity_pos, &state, &edge_index,
                                 &jump_index, &state_timer, &finished);
            }
            playing = !playing;
        }

        if (has_path && playing && !finished) {
            state_timer += dt;

            if (state == SIM_AT_NODE) {
                if (edge_index >= path_len - 1) {
                    state = SIM_FINISHED;
                    finished = true;
                    playing = false;
                } else {
                    float wait_time = (edge_index > 0 && edge_index < path_len - 1) ? NODE_WAIT_SECONDS : 0.0f;
                    if (state_timer >= wait_time) {
                        state = SIM_MOVING;
                        state_timer = 0.0f;
                        jump_index = 0;
                    }
                }
            } else if (state == SIM_MOVING) {
                int u = path[edge_index];
                int v = path[edge_index + 1];
                int edge_weight = g->adj_matrix[u][v];
                int jumps = edge_weight > 0 ? edge_weight : 1;

                while (state_timer >= JUMP_DURATION && state == SIM_MOVING) {
                    state_timer -= JUMP_DURATION;
                    jump_index++;

                    float t = (float)jump_index / (float)jumps;
                    if (t > 1.0f) {
                        t = 1.0f;
                    }

                    entity_pos = v_lerp(positions[u], positions[v], t);

                    if (jump_index >= jumps) {
                        entity_pos = positions[v];
                        edge_index++;
                        jump_index = 0;
                        state_timer = 0.0f;
                        state = SIM_AT_NODE;

                        if (edge_index >= path_len - 1) {
                            state = SIM_FINISHED;
                            finished = true;
                            playing = false;
                        }
                    }
                }
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Milestone 3: Dijkstra Path Animation", 20, 20, 24, DARKGRAY);
        DrawText("Use Play/Stop to control the moving entity", 20, 50, 16, GRAY);

        draw_button(play_button, playing ? "Stop" : "Play", has_path);
        draw_path_text(path, path_len, total_weight, has_path);

        draw_graph(g, positions);

        if (has_path) {
            DrawCircleV(entity_pos, ENTITY_RADIUS + 3.0f, RAYWHITE);
            DrawCircleV(entity_pos, ENTITY_RADIUS, ORANGE);
            DrawCircleLines((int)entity_pos.x, (int)entity_pos.y, ENTITY_RADIUS, RED);
        }

        if (has_path && finished) {
            const char* msg = "Arrived at destination!";
            int width = MeasureText(msg, 28);
            DrawRectangle(screen_width / 2 - width / 2 - 20, screen_height - 75, width + 40, 45, LIGHTGRAY);
            DrawRectangleLines(screen_width / 2 - width / 2 - 20, screen_height - 75, width + 40, 45, DARKGRAY);
            DrawText(msg, screen_width / 2 - width / 2, screen_height - 65, 28, DARKGREEN);
        }

        EndDrawing();
    }

    CloseWindow();
}
