#define _POSIX_C_SOURCE 200809L

#include <raylib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "gui.h"
#include "graph.h"

#define NODE_RADIUS 32.0f
#define NODE_FONT_SIZE 20
#define ENTITY_RADIUS 13.0f
#define JUMP_DURATION 0.80f
#define NODE_WAIT_SECONDS 2.5f
#define IPC_LINE_SIZE 128
#define SEM_NAME_SIZE 64

typedef enum {
    SIM_AT_NODE,
    SIM_MOVING,
    SIM_FINISHED
} SimState;

typedef struct {
    int id;
    pid_t pid;
    TravelerRequest request;
    int pipe_fd;
    bool pipe_open;
    //=====EXAM CHANGED=====
    int ack_fd;
    bool ack_open;
    //======================
    bool has_path;
    bool done;
    bool terminate_sent;
    bool reaped;
    bool finished_logged;
    bool waiting;
    int waiting_node;
    int path[MAX_NODES];
    int path_len;
    int total_weight;
    int edge_index;
    int jump_index;
    int current_node;
    int next_node;
    float state_timer;
    SimState state;
    Vector2 position;
    Color color;
    char ipc_buffer[IPC_LINE_SIZE];
    int ipc_len;
} TravelerSim;

// =================================================================
// UTILITY FUNCTIONS (Math, Colors, Basic Drawing)
// =================================================================
static Vector2 v_add(Vector2 a, Vector2 b) { return (Vector2){ a.x + b.x, a.y + b.y }; }
static Vector2 v_sub(Vector2 a, Vector2 b) { return (Vector2){ a.x - b.x, a.y - b.y }; }
static Vector2 v_scale(Vector2 a, float s) { return (Vector2){ a.x * s, a.y * s }; }
static float v_len(Vector2 a) { return sqrtf(a.x * a.x + a.y * a.y); }
static Vector2 v_norm(Vector2 a) {
    float len = v_len(a);
    if (len <= 0.0001f) return (Vector2){ 0.0f, 0.0f };
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
    draw_centered_text(text, (int)(button.x + button.width / 2.0f), (int)(button.y + button.height / 2.0f), 20, text_color);
}

// =================================================================
// MILESTONE 2: STATIC GUI DRAWING
// Drawing the nodes and directed edges on the screen
// =================================================================
static void draw_directed_edge(Vector2 start_pos, Vector2 end_pos, int weight) {
    Vector2 dir = v_sub(end_pos, start_pos);
    float length = v_len(dir);
    if (length <= 2.0f * NODE_RADIUS) return;

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

static bool valid_node(const Graph* g, int node) {
    return g && node >= 0 && node < g->num_nodes;
}

static void sleep_seconds(double seconds) {
    if (seconds <= 0.0) return;
    struct timespec req;
    req.tv_sec = (time_t)seconds;
    req.tv_nsec = (long)((seconds - (double)req.tv_sec) * 1000000000.0);
    while (nanosleep(&req, &req) == -1 && errno == EINTR) {}
}

static unsigned char color_channel(float value) {
    if (value < 0.0f) value = 0.0f;
    else if (value > 1.0f) value = 1.0f;
    return (unsigned char)(value * 255.0f + 0.5f);
}

static Color hsv_color(float hue, float saturation, float value) {
    float c = value * saturation;
    float h = fmodf(hue, 360.0f) / 60.0f;
    float x = c * (1.0f - fabsf(fmodf(h, 2.0f) - 1.0f));
    float m = value - c;
    float r = 0.0f, g = 0.0f, b = 0.0f;
    if (h < 1.0f) { r = c; g = x; }
    else if (h < 2.0f) { r = x; g = c; }
    else if (h < 3.0f) { g = c; b = x; }
    else if (h < 4.0f) { g = x; b = c; }
    else if (h < 5.0f) { r = x; b = c; }
    else { r = c; b = x; }
    return (Color){ color_channel(r + m), color_channel(g + m), color_channel(b + m), 255 };
}

static Color traveler_color(int id) {
    float hue = fmodf((float)(id * 137), 360.0f);
    float saturation = 0.68f + 0.12f * (float)(id % 3);
    float value = 0.72f + 0.16f * (float)((id / 3) % 2);
    return hsv_color(hue, saturation, value);
}

static void build_node_positions(const Graph* g, Vector2 positions[], int screen_width, int screen_height) {
    Vector2 center = { screen_width / 2.0f, screen_height / 2.0f + 35.0f };
    float layout_radius = 265.0f;
    for (int i = 0; i < g->num_nodes; i++) {
        float angle = -PI / 2.0f + i * (2.0f * PI / g->num_nodes);
        positions[i].x = center.x + cosf(angle) * layout_radius;
        positions[i].y = center.y + sinf(angle) * layout_radius;
    }
}

static Vector2 draw_position_for_traveler(const TravelerSim* traveler) {
    float angle = (float)traveler->id * 1.0471976f;
    float radius = 9.0f + (float)(traveler->id % 3) * 3.0f;
    if (traveler->waiting) {
        radius = NODE_RADIUS + 18.0f + (float)(traveler->id % 3) * 5.0f;
    }
    return (Vector2){ traveler->position.x + cosf(angle) * radius, traveler->position.y + sinf(angle) * radius };
}

static void draw_traveler_status(const TravelerSim travelers[], int traveler_count, const char* title, const char* subtitle) {
    DrawText(title, 20, 20, 24, DARKGRAY);
    DrawText(subtitle, 20, 50, 16, GRAY);
    if (traveler_count == 0) {
        DrawText("No travelers in input.", 20, 82, 18, RED);
        return;
    }
    int y = 82;
    for (int i = 0; i < traveler_count && i < 10; i++) {
        const TravelerSim* t = &travelers[i];
        char line[160];
        const char* state_text = "moving";
        if (!t->has_path && t->pipe_fd < 0) state_text = "no path";
        else if (t->waiting) state_text = "waiting";
        else if (t->done) state_text = "done";
        else if (t->state == SIM_AT_NODE) state_text = "at node";

        snprintf(line, sizeof(line), "T%d [%d -> %d] PID=%d %s", t->id, t->request.source, t->request.destination, (int)t->pid, state_text);
        DrawText(line, 20, y, 16, t->color);
        y += 20;
    }
    if (traveler_count > 10) DrawText("...", 20, y, 16, GRAY);
}

static void draw_travelers(const TravelerSim travelers[], int traveler_count) {
    for (int i = 0; i < traveler_count; i++) {
        const TravelerSim* t = &travelers[i];
        if (!t->has_path) continue;
        Vector2 draw_pos = draw_position_for_traveler(t);
        DrawCircleV(draw_pos, ENTITY_RADIUS + 3.0f, RAYWHITE);
        DrawCircleV(draw_pos, ENTITY_RADIUS, t->color);
        DrawCircleLines((int)draw_pos.x, (int)draw_pos.y, ENTITY_RADIUS, BLACK);
        if (t->waiting) {
            DrawCircleLines((int)draw_pos.x, (int)draw_pos.y, ENTITY_RADIUS + 7.0f, RED);
            draw_centered_text("W", (int)draw_pos.x, (int)draw_pos.y - 24, 14, RED);
        }
        char id_text[8];
        snprintf(id_text, sizeof(id_text), "%d", t->id);
        draw_centered_text(id_text, (int)draw_pos.x, (int)draw_pos.y, 12, BLACK);
    }
}

static bool all_travelers_done(const TravelerSim travelers[], int traveler_count) {
    for (int i = 0; i < traveler_count; i++) {
        if (!travelers[i].done) return false;
    }
    return true;
}

static void terminate_child(TravelerSim* traveler) {
    if (traveler->pid > 0 && !traveler->terminate_sent && !traveler->reaped) {
        kill(traveler->pid, SIGTERM);
        traveler->terminate_sent = true;
    }
}

static void log_child_finished(TravelerSim* traveler) {
    if (!traveler->finished_logged && traveler->pid > 0) {
        printf("[PID=%d] finished\n", (int)traveler->pid);
        fflush(stdout);
        traveler->finished_logged = true;
    }
}

static void mark_child_finished(TravelerSim* traveler) {
    traveler->done = true;
    traveler->state = SIM_FINISHED;
    log_child_finished(traveler);
}

static void reap_child_nonblocking(TravelerSim* traveler) {
    if (traveler->pid <= 0 || traveler->reaped) return;
    int status = 0;
    pid_t result = waitpid(traveler->pid, &status, WNOHANG);
    if (result == traveler->pid) {
        traveler->reaped = true;
        if (traveler->pipe_fd >= 0 && !traveler->done) mark_child_finished(traveler);
    }
}

static void cleanup_children(TravelerSim travelers[], int traveler_count) {
    for (int i = 0; i < traveler_count; i++) {
        if (travelers[i].pipe_open) {
            close(travelers[i].pipe_fd);
            travelers[i].pipe_open = false;
        }
        if(travelers[i].ack_open){
            close(travelers[i].ack_fd);
            travelers[i].ack_open = false; 
        }
        if (travelers[i].pid > 0 && !travelers[i].reaped && (!travelers[i].done || travelers[i].pipe_fd < 0)) {
            terminate_child(&travelers[i]);
        }
    }
    for (int i = 0; i < traveler_count; i++) {
        if (travelers[i].pid > 0 && !travelers[i].reaped) {
            int status = 0;
            while (waitpid(travelers[i].pid, &status, 0) == -1 && errno == EINTR) {}
            travelers[i].reaped = true;
        }
    }
}

static void advance_path_traveler(const Graph* g, TravelerSim* traveler, const Vector2 positions[], float dt) {
    if (!traveler->has_path || traveler->done || traveler->state == SIM_FINISHED) return;

    traveler->state_timer += dt;

    if (traveler->state == SIM_AT_NODE) {
        if (traveler->edge_index >= traveler->path_len - 1) {
            traveler->state = SIM_FINISHED;
            traveler->done = true;
            return;
        }

        float wait_time = (traveler->edge_index > 0 && traveler->edge_index < traveler->path_len - 1) ? NODE_WAIT_SECONDS : 0.0f;
        if (traveler->state_timer >= wait_time) {
            traveler->state = SIM_MOVING;
            traveler->state_timer = 0.0f;
            traveler->jump_index = 0;
        }
    } else if (traveler->state == SIM_MOVING) {
        int u = traveler->path[traveler->edge_index];
        int v = traveler->path[traveler->edge_index + 1];
        int edge_weight = g->adj_matrix[u][v];
        int jumps = edge_weight > 0 ? edge_weight : 1;

        while (traveler->state_timer >= JUMP_DURATION && traveler->state == SIM_MOVING) {
            traveler->state_timer -= JUMP_DURATION;
            traveler->jump_index++;

            float t = (float)traveler->jump_index / (float)jumps;
            if (t > 1.0f) t = 1.0f;

            traveler->position = v_lerp(positions[u], positions[v], t);

            if (traveler->jump_index >= jumps) {
                traveler->position = positions[v];
                traveler->edge_index++;
                traveler->jump_index = 0;
                traveler->state_timer = 0.0f;
                traveler->state = SIM_AT_NODE;

                if (traveler->edge_index >= traveler->path_len - 1) {
                    traveler->state = SIM_FINISHED;
                    traveler->done = true;
                }
            }
        }
    }
}

static void advance_ipc_traveler(const Graph* g, TravelerSim* traveler, const Vector2 positions[], float dt) {
    if (traveler->done || traveler->next_node < 0 || traveler->state == SIM_FINISHED) return;

    traveler->state_timer += dt;

    if (traveler->state == SIM_AT_NODE) {
        float wait_time = (traveler->current_node != traveler->request.source) ? NODE_WAIT_SECONDS : 0.0f;
        if (traveler->state_timer >= wait_time) {
            traveler->state = SIM_MOVING;
            traveler->state_timer = 0.0f;
            traveler->jump_index = 0;
        }
    } else if (traveler->state == SIM_MOVING) {
        int u = traveler->current_node;
        int v = traveler->next_node;
        if (!valid_node(g, u) || !valid_node(g, v) || g->adj_matrix[u][v] < 0) {
            traveler->state = SIM_AT_NODE;
            traveler->next_node = -1;
            return;
        }

        int jumps = g->adj_matrix[u][v] > 0 ? g->adj_matrix[u][v] : 1;
        while (traveler->state_timer >= JUMP_DURATION && traveler->state == SIM_MOVING) {
            traveler->state_timer -= JUMP_DURATION;
            traveler->jump_index++;

            float t = (float)traveler->jump_index / (float)jumps;
            if (t > 1.0f) t = 1.0f;

            traveler->position = v_lerp(positions[u], positions[v], t);

            if (traveler->jump_index >= jumps) {
                traveler->position = positions[v];
                traveler->state = SIM_AT_NODE;
                traveler->state_timer = 0.0f;
                traveler->jump_index = 0;
                traveler->current_node = v;
                traveler->next_node = -1;
            }
        }
    }
}

static void draw_finished_banner(int screen_width, int screen_height, const char* text) {
    int width = MeasureText(text, 28);
    DrawRectangle(screen_width / 2 - width / 2 - 20, screen_height - 75, width + 40, 45, LIGHTGRAY);
    DrawRectangleLines(screen_width / 2 - width / 2 - 20, screen_height - 75, width + 40, 45, DARKGRAY);
    DrawText(text, screen_width / 2 - width / 2, screen_height - 65, 28, DARKGREEN);
}

// =================================================================
// MILESTONE 3: SINGLE TRAVELER ANIMATION
// =================================================================
void run_gui(Graph* g) {
    const int screen_width = 950;
    const int screen_height = 780;
    int path[MAX_NODES];
    int path_len = 0;
    int total_weight = 0;
    
    // 🎯 EXAM HOTSPOT 1: DIJKSTRA ALGORITHM CALL
    // If the exam asks you to change how the path is calculated (e.g., avoid a specific node, 
    // or use a different algorithm), the path array is populated right here.
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

        if (has_path && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), play_button)) {
            if (finished) {
                reset_simulation(path, positions, &entity_pos, &state, &edge_index, &jump_index, &state_timer, &finished);
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
                    if (t > 1.0f) t = 1.0f;
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
            draw_finished_banner(screen_width, screen_height, "Arrived at destination!");
        }
        EndDrawing();
    }
    CloseWindow();
}

static void init_milestone4_traveler(const Graph* g, TravelerSim* traveler, const TravelerRequest* request, const Vector2 positions[], int id) {
    memset(traveler, 0, sizeof(*traveler));
    traveler->id = id;
    traveler->pid = -1;
    traveler->pipe_fd = -1;
    traveler->request = *request;
    traveler->color = traveler_color(id);
    traveler->state = SIM_AT_NODE;
    traveler->next_node = -1;

    traveler->has_path = get_dijkstra_path_between(g, request->source, request->destination, traveler->path, &traveler->path_len, &traveler->total_weight);

    if (valid_node(g, request->source)) traveler->position = positions[request->source];

    if (traveler->has_path) {
        traveler->position = positions[traveler->path[0]];
        if (traveler->path_len <= 1) {
            traveler->state = SIM_FINISHED;
            traveler->done = true;
        }
    } else {
        traveler->done = true;
    }
}

// =================================================================
// MILESTONE 4: FORK & CHILD PROCESSES
// Here the parent calculates paths, and the child just "lives" 
// =================================================================
static void fork_milestone4_children(TravelerSim travelers[], int traveler_count) {
    for (int i = 0; i < traveler_count; i++) {
        pid_t pid = fork();
        
        // 🎯 EXAM HOTSPOT 2: FORK FAILURE HANDLING
        // If they ask what to do when fork fails (e.g., print specific error and exit),
        // you add your exit(1) or custom error handling inside this block.
        if (pid == -1) {
            perror("fork");
            travelers[i].pid = -1;
            continue;
        }

        if (pid == 0) {
            printf("[%d] started\n", (int)getpid());
            fflush(stdout);
            
            // 🎯 EXAM HOTSPOT 3: SIGNAL HANDLING IN CHILD
            // If the exam asks you to handle SIGUSR1 instead of dying, 
            // you should place your signal(SIGUSR1, my_handler) right here
            // before the infinite loop.
            for (;;) {
                pause();
            }
        }
        travelers[i].pid = pid;
    }
    sleep_seconds(0.05);
}

void run_gui_milestone4(Graph* g, const TravelerRequest requests[], int traveler_count) {
    const int screen_width = 950;
    const int screen_height = 780;
    Vector2 positions[MAX_NODES];
    build_node_positions(g, positions, screen_width, screen_height);

    TravelerSim travelers[MAX_TRAVELERS];
    for (int i = 0; i < traveler_count; i++) {
        init_milestone4_traveler(g, &travelers[i], &requests[i], positions, i);
    }

    fork_milestone4_children(travelers, traveler_count);

    for (int i = 0; i < traveler_count; i++) {
        if (travelers[i].done) terminate_child(&travelers[i]);
    }

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screen_width, screen_height, "OS Project - Milestone 4 Multi-Traveler Simulation");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        for (int i = 0; i < traveler_count; i++) {
            bool was_done = travelers[i].done;
            advance_path_traveler(g, &travelers[i], positions, dt);

            if (travelers[i].done && !travelers[i].terminate_sent) {
                // 🎯 EXAM HOTSPOT 4: PARENT SENDING SIGNALS TO CHILD
                // If they ask to send a different signal (like SIGSTOP) instead of killing,
                // you need to modify the terminate_child() function called right here.
                terminate_child(&travelers[i]);
            }
            if (travelers[i].done || was_done) {
                // 🎯 EXAM HOTSPOT 5: REAPING ZOMBIES
                // waitpid() with WNOHANG happens inside reap_child_nonblocking.
                // This is what prevents zombie processes from lingering.
                reap_child_nonblocking(&travelers[i]);
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        draw_traveler_status(travelers, traveler_count, "Milestone 4: Parent-Controlled Travelers", "Parent computed all Dijkstra paths before fork; children sleep until terminated.");
        draw_graph(g, positions);
        draw_travelers(travelers, traveler_count);

        if (all_travelers_done(travelers, traveler_count)) {
            draw_finished_banner(screen_width, screen_height, "All travelers finished");
        }
        EndDrawing();
    }
    CloseWindow();
    cleanup_children(travelers, traveler_count);
}

// =================================================================
// MILESTONE 5: IPC WRITER FUNCTIONS (Child side)
// =================================================================
static bool write_all(int fd, const char* data, size_t len) {
    size_t written = 0;
    while (written < len) {
        ssize_t result = write(fd, data + written, len - written);
        if (result == -1) {
            if (errno == EINTR) continue;
            return false;
        }
        written += (size_t)result;
    }
    return true;
}
//==========NEW FUNCTION FOR EXAM=============
static bool child_wait_for_ack(int ack_fd){
    char buffer[16];
    for(;;){
        ssize_t result = read(ack_fd, buffer, sizeof(buffer));
        if(result > 0){
            return  true;
        }
        if(result == 0){
            return false;
        }
        return false;
    }
}
static bool parent_send_ack(int ack_fd){
    const char* msg = "OK\n";
    return write_all(ack_fd,msf,strlen(msg));
}
//============================================
static bool child_send_arrival(int fd, int node, int next_node) {
    char line[IPC_LINE_SIZE];
    
    // 🎯 EXAM HOTSPOT 6: FORMATTING IPC MESSAGES (snprintf)
    // If you need to send new data to the parent (like weight, ticket, etc.),
    // you MUST change the format string here. Example: "ARRIVED %d %d %d\n"
    int len = snprintf(line, sizeof(line), "ARRIVED %d %d\n", node, next_node);
    if (len < 0 || len >= (int)sizeof(line)) return false;
    return write_all(fd, line, (size_t)len);
}

static bool child_send_finished(int fd) {
    const char* line = "FINISHED\n";
    return write_all(fd, line, strlen(line));
}

static bool child_send_message(int fd, const char* message) {
    char line[IPC_LINE_SIZE];
    int len = snprintf(line, sizeof(line), "%s\n", message);
    if (len < 0 || len >= (int)sizeof(line)) return false;
    return write_all(fd, line, (size_t)len);
}

static bool child_send_node_message(int fd, const char* message, int node) {
    char line[IPC_LINE_SIZE];
    int len = snprintf(line, sizeof(line), "%s %d\n", message, node);
    if (len < 0 || len >= (int)sizeof(line)) return false;
    return write_all(fd, line, (size_t)len);
}

static bool child_send_left_node(int fd, int node, int next_node) {
    char line[IPC_LINE_SIZE];
    int len = snprintf(line, sizeof(line), "LEFT_NODE %d %d\n", node, next_node);
    if (len < 0 || len >= (int)sizeof(line)) return false;
    return write_all(fd, line, (size_t)len);
}

// =================================================================
// MILESTONE 5: AUTONOMOUS CHILD (IPC)
// =================================================================
static void run_milestone5_child(const Graph* g, TravelerRequest request, int write_fd, int ack_fd) {
    int path[MAX_NODES];
    int path_len = 0;
    int total_weight = 0;

    // 🎯 EXAM HOTSPOT 7: CHILD LOGIC BEFORE MOVING
    // If they ask: "Child checks condition X before moving, and if false, sends ERROR",
    // you put that check right here, before the movement loop starts.
    if (!get_dijkstra_path_between(g, request.source, request.destination, path, &path_len, &total_weight)) {
        child_send_finished(write_fd);
        close(write_fd);
        //======EXAM CHANGE======
        close(ack_fd);
        //=======================
        _exit(0);
    }
    (void)total_weight;

    for (int i = 0; i < path_len; i++) {
        int current = path[i];
        int next = (i < path_len - 1) ? path[i + 1] : -1;
        if (!child_send_arrival(write_fd, current, next)) {
            close(write_fd);
            close(ack_fd);
            _exit(0);
        }
        //========EXAM CHANGE===========
        if (!child_wait_for_ack(ack_fd)){
            close(write_fd);
            close(ack_fd);
            _exit(0);
        }
        //==============================
        if (next == -1) break;

        if (i > 0) sleep_seconds(NODE_WAIT_SECONDS);

        int edge_weight = g->adj_matrix[current][next];
        int jumps = edge_weight > 0 ? edge_weight : 1;
        for (int step = 0; step < jumps; step++) {
            sleep_seconds(JUMP_DURATION);
        }
    }
    child_send_finished(write_fd);
    close(write_fd);
    //===========EXAM CHANGE===========
    close(ack_fd);
    //=================================
    _exit(0);
}

// =================================================================
// MILESTONE 6: SEMAPHORES INITIALIZATION
// =================================================================
static bool init_node_semaphores(const Graph* g, sem_t* node_sems[], char sem_names[][SEM_NAME_SIZE]) {
    for (int i = 0; i < g->num_nodes; i++) {
        snprintf(sem_names[i], SEM_NAME_SIZE, "/osgraph_%ld_%d", (long)getpid(), i);
        sem_unlink(sem_names[i]);
        
        // 🎯 EXAM HOTSPOT 8: SEMAPHORE CAPACITY (MUTEX vs COUNTING)
        // The last parameter '1' means this is a Mutex (1 traveler per node).
        // EXAM QUESTION: "Allow 3 travelers per node" -> Change the '1' to '3' here!
        node_sems[i] = sem_open(sem_names[i], O_CREAT | O_EXCL, 0600, 1);
        if (node_sems[i] == SEM_FAILED) {
            perror("sem_open");
            for (int j = 0; j < i; j++) {
                sem_close(node_sems[j]);
                sem_unlink(sem_names[j]);
                node_sems[j] = SEM_FAILED;
            }
            return false;
        }
    }
    return true;
}

static void cleanup_node_semaphores(const Graph* g, sem_t* node_sems[], char sem_names[][SEM_NAME_SIZE]) {
    for (int i = 0; i < g->num_nodes; i++) {
        if (node_sems[i] != SEM_FAILED && node_sems[i] != NULL) {
            sem_close(node_sems[i]);
            sem_unlink(sem_names[i]);
            node_sems[i] = SEM_FAILED;
        }
    }
}

// =================================================================
// MILESTONE 6: CRITICAL SECTION (Wait for Node)
// =================================================================
static bool wait_for_node_lock(int write_fd, sem_t* node_sems[], int node) {
    if (sem_trywait(node_sems[node]) == 0) return true;

    if (errno != EAGAIN) {
        while (sem_wait(node_sems[node]) == -1) {
            if (errno != EINTR) return false;
        }
        return true;
    }

    if (!child_send_node_message(write_fd, "WAITING_FOR_NODE", node)) return false;

    while (sem_wait(node_sems[node]) == -1) {
        if (errno != EINTR) return false;
    }
    return true;
}

static void sleep_edge_steps(const Graph* g, int current, int next) {
    int edge_weight = g->adj_matrix[current][next];
    int jumps = edge_weight > 0 ? edge_weight : 1;
    for (int step = 0; step < jumps; step++) {
        sleep_seconds(JUMP_DURATION);
    }
}

// =================================================================
// MILESTONE 6: SYNCHRONIZED CHILD LOGIC
// =================================================================
static void run_milestone6_child(const Graph* g, TravelerRequest request, int write_fd, sem_t* node_sems[]) {
    int path[MAX_NODES];
    int path_len = 0;
    int total_weight = 0;

    if (!valid_node(g, request.source) || !valid_node(g, request.destination)) {
        child_send_message(write_fd, "ERROR invalid_node");
        child_send_finished(write_fd);
        close(write_fd);
        _exit(0);
    }

    if (!get_dijkstra_path_between(g, request.source, request.destination, path, &path_len, &total_weight)) {
        child_send_message(write_fd, "NO_PATH");
        child_send_finished(write_fd);
        close(write_fd);
        _exit(0);
    }
    (void)total_weight;

    if (path_len == 1) {
        if (!wait_for_node_lock(write_fd, node_sems, path[0])) {
            child_send_message(write_fd, "ERROR semaphore_wait");
            child_send_finished(write_fd);
            close(write_fd);
            _exit(0);
        }
        child_send_node_message(write_fd, "DESTINATION", path[0]);
        sem_post(node_sems[path[0]]);
        child_send_finished(write_fd);
        close(write_fd);
        _exit(0);
    }

    for (int i = 0; i < path_len - 1; i++) {
        int current = path[i];
        int next = path[i + 1];

        // 🎯 EXAM HOTSPOT 9: ENTERING CRITICAL SECTION (sem_wait)
        // This function calls sem_wait. If the node is locked, it blocks here.
        if (!wait_for_node_lock(write_fd, node_sems, current)) {
            child_send_message(write_fd, "ERROR semaphore_wait");
            child_send_finished(write_fd);
            close(write_fd);
            _exit(0);
        }

        if (!child_send_node_message(write_fd, "ENTERED_NODE", current)) {
            sem_post(node_sems[current]);
            close(write_fd);
            _exit(0);
        }

        if (i > 0) sleep_seconds(NODE_WAIT_SECONDS);

        if (!child_send_left_node(write_fd, current, next)) {
            sem_post(node_sems[current]);
            close(write_fd);
            _exit(0);
        }

        // 🎯 EXAM HOTSPOT 10: LEAVING CRITICAL SECTION (sem_post)
        // The child frees the node for the next traveler.
        // DO NOT put any sleep related to this node AFTER this line.
        sem_post(node_sems[current]);
        
        sleep_edge_steps(g, current, next);
    }

    if (!wait_for_node_lock(write_fd, node_sems, path[path_len - 1])) {
        child_send_message(write_fd, "ERROR semaphore_wait");
        child_send_finished(write_fd);
        close(write_fd);
        _exit(0);
    }
    child_send_node_message(write_fd, "DESTINATION", path[path_len - 1]);
    sem_post(node_sems[path[path_len - 1]]);
    child_send_finished(write_fd);
    close(write_fd);
    _exit(0);
}

static void init_milestone5_traveler(const Graph* g, TravelerSim* traveler, const TravelerRequest* request, const Vector2 positions[], int id) {
    memset(traveler, 0, sizeof(*traveler));
    traveler->id = id;
    traveler->pid = -1;
    traveler->pipe_fd = -1;
    //=======EXAM CHANGE==========
    traveler->ack_fd = -1;
    traveler->ack_open = false;
    //============================
    traveler->request = *request;
    traveler->color = traveler_color(id);
    traveler->state = SIM_AT_NODE;
    traveler->next_node = -1;
    traveler->waiting_node = -1;
    traveler->pipe_open = false;
    traveler->has_path = valid_node(g, request->source);
    if (valid_node(g, request->source)) {
        traveler->position = positions[request->source];
        traveler->current_node = request->source;
    }
}

static bool set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

// =================================================================
// MILESTONE 5: FORK WITH PIPES
// =================================================================
static void fork_milestone5_children(const Graph* g, TravelerSim travelers[], int traveler_count) {
    //int pipes[MAX_TRAVELERS][2];
    int child_to_parent[MAX_TRAVELERS][2];
    int parent_to_child[MAX_TRAVELERS][2];

    for (int i = 0; i < traveler_count; i++) {
        child_to_parent[i][0] = -1;
        child_to_parent[i][1] = -1;
        parent_to_child[i][0] = -1;
        parent_to_child[i][0] = -1;

        if (pipe(child_to_parent[i]) == -1) {
            perror("pipe child_to_parent");
            travelers[i].done = true;
            continue;
        }
        if(pipe(parent_to_child[i] == -1)){
            perror("pipe parent_to_child")
            close(child_to_parent[i][0]);
            close(child_to_parent[i][1]);

            child_to_parent[i][0] = -1;
            child_to_parent[i][1] = -1;
            
            travelers[i].done = true;
            continue;
        }

    }

    for (int i = 0; i < traveler_count; i++) {
        if(child_to_parent[i][0] == -1 || child_to_parent[i][1] == -1 || parent_to_child[i][0] = -1 || parent_to_child[i][1] = -1){
            continue;
        }
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            close(child_to_parent[i][0]);
            close(child_to_parent[i][1]);
            close(parent_to_child[i][0]);
            close(parent_to_child[i][1]);

            child_to_parent[i][0] = -1;
            child_to_parent[i][1] = -1;
            parent_to_child[i][0] = -1;
            parent_to_child[i][1] = -1;

            travelers[i].done = true;
            continue;
        }

        if (pid == 0) {
            for(int j = 0; j <traveler_count; j++){
                if(child_to_parent[j][0] != -1){
                    close(child_to_parent[j][0]);
                }
                if(j != i && child_to_parent[j][1] != -1){
                    close(child_to_parent[j][1]);
                }
                if(parent_to_child[j][1] != -1){
                    close(parent_to_child[j][1]);
                }
                if(j != i && parent_to_child[j][0] != -1){
                    close(parent_to_child[j][0]);
                }
            }
            run_milestone5_child(g,travelers[i].request,child_to_parent[i][1],parent_to_child[i][0]);
        }
        travelers[i].pid = pid;
            
    }

    for (int i = 0; i < traveler_count; i++) {
        if(child_to_parent[i][1] != -1){
            close(child_to_parent[i][1]);
        }
        if(parent_to_child[i][0] != -1){
            close(parent_to_child[i][0]);
        }
        if(child_to_parent[i][0] != -1 && travelers[i].pid > 0){
            travelers[i].pipe_fd = child_to_parent[i][0];
            travelers[i].pipe_open = true;
            if( !set_nonblocking(travelers[i].pipe_fd)){
                perror("fcntl");
            }
            else if(child_to_parent[i][0] != -1){
                close(child_to_parent[i][0]);
            }    
        }
        if(parent_to_child[i][1] != -1 && travelers[i].pid >0){
            travelers[i].ack_fd = parent_to_child[i][1];
            travelers[i].ack_open = true;
        } else if(parent_to_child[i][1] != -1){
            close(parent_to_child[i][1]);
        }
    }
}

// =================================================================
// MILESTONE 6: FORK WITH PIPES AND SEMAPHORES
// =================================================================
static void fork_milestone6_children(const Graph* g, TravelerSim travelers[], int traveler_count, sem_t* node_sems[]) {
    int pipes[MAX_TRAVELERS][2];
    for (int i = 0; i < traveler_count; i++) {
        pipes[i][0] = -1;
        pipes[i][1] = -1;
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            travelers[i].done = true;
        }
    }

    for (int i = 0; i < traveler_count; i++) {
        if (pipes[i][0] == -1 || pipes[i][1] == -1) continue;
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            close(pipes[i][0]);
            close(pipes[i][1]);
            pipes[i][0] = -1;
            pipes[i][1] = -1;
            travelers[i].done = true;
            continue;
        }

        if (pid == 0) {
            for (int j = 0; j < traveler_count; j++) {
                if (pipes[j][0] != -1) close(pipes[j][0]);
                if (j != i && pipes[j][1] != -1) close(pipes[j][1]);
            }
            run_milestone6_child(g, travelers[i].request, pipes[i][1], node_sems);
        }
        travelers[i].pid = pid;
    }

    for (int i = 0; i < traveler_count; i++) {
        if (pipes[i][1] != -1) close(pipes[i][1]);
        if (pipes[i][0] != -1 && travelers[i].pid > 0) {
            travelers[i].pipe_fd = pipes[i][0];
            travelers[i].pipe_open = true;
            if (!set_nonblocking(travelers[i].pipe_fd)) perror("fcntl");
        } else if (pipes[i][0] != -1) {
            close(pipes[i][0]);
        }
    }
}

// =================================================================
// MILESTONE 5/6: PARENT READING IPC (Parsing messages from child)
// =================================================================
static void handle_ipc_line(TravelerSim* traveler, const Vector2 positions[], const char* line) {
    int node = -1;
    int next_node = -1;

    // 🎯 EXAM HOTSPOT 11: PARSING IPC MESSAGES (sscanf)
    // If the exam asks you to add a NEW message type (e.g., "HELP_ME <node>"), 
    // you must add a new 'else if' block here. Use sscanf to extract numbers.
    if (sscanf(line, "ARRIVED %d %d", &node, &next_node) == 2) {
        traveler->has_path = true;
        traveler->current_node = node;
        traveler->next_node = next_node;
        traveler->state = (next_node >= 0) ? SIM_AT_NODE : SIM_FINISHED;
        traveler->state_timer = 0.0f;
        traveler->jump_index = 0;
        if (node >= 0 && node < MAX_NODES) traveler->position = positions[node];

        if (next_node >= 0) printf("[PID=%d] arrived at node %d | next node: %d\n", (int)traveler->pid, node, next_node);
        else {
            traveler->done = true;
            printf("[PID=%d] arrived at node %d | DESTINATION\n", (int)traveler->pid, node);
        }
        fflush(stdout);
    } else if (sscanf(line, "WAITING_FOR_NODE %d", &node) == 1) {
        traveler->has_path = true;
        traveler->waiting = true;
        traveler->waiting_node = node;
        traveler->next_node = -1;
        traveler->state = SIM_AT_NODE;
        traveler->state_timer = 0.0f;
        traveler->jump_index = 0;
        if (node >= 0 && node < MAX_NODES) traveler->position = positions[node];
        printf("[PID=%d] waiting for node %d\n", (int)traveler->pid, node);
        fflush(stdout);
    } else if (sscanf(line, "ENTERED_NODE %d", &node) == 1) {
        traveler->has_path = true;
        traveler->waiting = false;
        traveler->waiting_node = -1;
        traveler->current_node = node;
        traveler->next_node = -1;
        traveler->state = SIM_AT_NODE;
        traveler->state_timer = 0.0f;
        traveler->jump_index = 0;
        if (node >= 0 && node < MAX_NODES) traveler->position = positions[node];
        printf("[PID=%d] entered node %d\n", (int)traveler->pid, node);
        fflush(stdout);
    } else if (sscanf(line, "LEFT_NODE %d %d", &node, &next_node) == 2) {
        traveler->has_path = true;
        traveler->waiting = false;
        traveler->waiting_node = -1;
        traveler->current_node = node;
        traveler->next_node = next_node;
        traveler->state = SIM_MOVING;
        traveler->state_timer = 0.0f;
        traveler->jump_index = 0;
        if (node >= 0 && node < MAX_NODES) traveler->position = positions[node];
        printf("[PID=%d] left node %d | next node: %d\n", (int)traveler->pid, node, next_node);
        fflush(stdout);
    } else if (sscanf(line, "DESTINATION %d", &node) == 1) {
        traveler->has_path = true;
        traveler->waiting = false;
        traveler->waiting_node = -1;
        traveler->current_node = node;
        traveler->next_node = -1;
        traveler->done = true;
        traveler->state = SIM_FINISHED;
        if (node >= 0 && node < MAX_NODES) traveler->position = positions[node];
        printf("[PID=%d] arrived at node %d | DESTINATION\n", (int)traveler->pid, node);
        fflush(stdout);
    } else if (strcmp(line, "NO_PATH") == 0) {
        traveler->has_path = false;
        traveler->waiting = false;
        traveler->done = true;
        traveler->state = SIM_FINISHED;
        printf("[PID=%d] NO_PATH from %d to %d\n", (int)traveler->pid, traveler->request.source, traveler->request.destination);
        fflush(stdout);
    } else if (strncmp(line, "ERROR", 5) == 0) {
        traveler->has_path = false;
        traveler->waiting = false;
        traveler->done = true;
        traveler->state = SIM_FINISHED;
        printf("[PID=%d] %s\n", (int)traveler->pid, line);
        fflush(stdout);
    } else if (strcmp(line, "FINISHED") == 0) {
        mark_child_finished(traveler);
    }
    if(traveler->ack_open && traveler->ack_fd >= 0){
        parent_send_ack(traveler->ack_fd);
    }
}

static void consume_ipc_buffer(TravelerSim* traveler, const Vector2 positions[]) {
    for (;;) {
        char* newline = memchr(traveler->ipc_buffer, '\n', (size_t)traveler->ipc_len);
        if (!newline) break;

        int line_len = (int)(newline - traveler->ipc_buffer);
        char line[IPC_LINE_SIZE];
        if (line_len >= (int)sizeof(line)) line_len = (int)sizeof(line) - 1;
        memcpy(line, traveler->ipc_buffer, (size_t)line_len);
        line[line_len] = '\0';

        int remaining = traveler->ipc_len - line_len - 1;
        memmove(traveler->ipc_buffer, newline + 1, (size_t)remaining);
        traveler->ipc_len = remaining;

        handle_ipc_line(traveler, positions, line);
    }
}

static void read_ipc_messages(TravelerSim travelers[], int traveler_count, const Vector2 positions[]) {
    char buffer[128];
    for (int i = 0; i < traveler_count; i++) {
        TravelerSim* traveler = &travelers[i];
        if (!traveler->pipe_open) continue;

        for (;;) {
            ssize_t bytes = read(traveler->pipe_fd, buffer, sizeof(buffer));
            if (bytes > 0) {
                for (ssize_t j = 0; j < bytes; j++) {
                    if (traveler->ipc_len < IPC_LINE_SIZE - 1) {
                        traveler->ipc_buffer[traveler->ipc_len++] = buffer[j];
                    } else {
                        traveler->ipc_len = 0;
                    }
                }
                consume_ipc_buffer(traveler, positions);
            } else if (bytes == 0) {
                close(traveler->pipe_fd);
                traveler->pipe_open = false;
                if (!traveler->done) mark_child_finished(traveler);
                break;
            } else {
                if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                    perror("read");
                    close(traveler->pipe_fd);
                    traveler->pipe_open = false;
                }
                break;
            }
        }
    }
}

void run_gui_milestone5(Graph* g, const TravelerRequest requests[], int traveler_count) {
    const int screen_width = 950;
    const int screen_height = 780;
    Vector2 positions[MAX_NODES];
    build_node_positions(g, positions, screen_width, screen_height);

    TravelerSim travelers[MAX_TRAVELERS];
    for (int i = 0; i < traveler_count; i++) {
        init_milestone5_traveler(g, &travelers[i], &requests[i], positions, i);
    }

    fork_milestone5_children(g, travelers, traveler_count);

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screen_width, screen_height, "OS Project - Milestone 5 IPC Traveler Simulation");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        read_ipc_messages(travelers, traveler_count, positions);

        for (int i = 0; i < traveler_count; i++) {
            advance_ipc_traveler(g, &travelers[i], positions, dt);
            if (travelers[i].done) reap_child_nonblocking(&travelers[i]);
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        draw_traveler_status(travelers, traveler_count, "Milestone 5: Autonomous Traveler Children", "Each child computes Dijkstra and reports node arrivals through its own pipe.");
        draw_graph(g, positions);
        draw_travelers(travelers, traveler_count);

        if (all_travelers_done(travelers, traveler_count)) {
            draw_finished_banner(screen_width, screen_height, "All travelers finished");
        }
        EndDrawing();
    }
    CloseWindow();
    cleanup_children(travelers, traveler_count);
}

void run_gui_milestone6(Graph* g, const TravelerRequest requests[], int traveler_count) {
    const int screen_width = 950;
    const int screen_height = 780;
    Vector2 positions[MAX_NODES];
    build_node_positions(g, positions, screen_width, screen_height);

    sem_t* node_sems[MAX_NODES];
    char sem_names[MAX_NODES][SEM_NAME_SIZE];
    for (int i = 0; i < MAX_NODES; i++) {
        node_sems[i] = SEM_FAILED;
        sem_names[i][0] = '\0';
    }

    if (!init_node_semaphores(g, node_sems, sem_names)) {
        fprintf(stderr, "Error: Failed to initialize node semaphores.\n");
        return;
    }

    TravelerSim travelers[MAX_TRAVELERS];
    for (int i = 0; i < traveler_count; i++) {
        init_milestone5_traveler(g, &travelers[i], &requests[i], positions, i);
    }

    fork_milestone6_children(g, travelers, traveler_count, node_sems);

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screen_width, screen_height, "OS Project - Milestone 6 Synchronized Nodes");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        read_ipc_messages(travelers, traveler_count, positions);

        for (int i = 0; i < traveler_count; i++) {
            advance_ipc_traveler(g, &travelers[i], positions, dt);
            if (travelers[i].done) reap_child_nonblocking(&travelers[i]);
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        draw_traveler_status(travelers, traveler_count, "Milestone 6: Synchronized Node Access", "Intermediate-node waits are protected by one POSIX semaphore per node.");
        draw_graph(g, positions);
        draw_travelers(travelers, traveler_count);

        if (all_travelers_done(travelers, traveler_count)) {
            draw_finished_banner(screen_width, screen_height, "All travelers finished");
        }
        EndDrawing();
    }
    CloseWindow();
    cleanup_children(travelers, traveler_count);
    cleanup_node_semaphores(g, node_sems, sem_names);
}

// =================================================================
// MILESTONE 7: PARENT-MANAGED SCHEDULING (FCFS / SJF)
// =================================================================

static void run_milestone7_child(const Graph* g, TravelerRequest request, int write_fd, sem_t* my_sem) {
    int path[MAX_NODES];
    int path_len = 0;
    int total_weight = 0;

    if (!valid_node(g, request.source) || !valid_node(g, request.destination)) {
        child_send_message(write_fd, "ERROR invalid_node");
        child_send_finished(write_fd); close(write_fd); _exit(0);
    }

    if (!get_dijkstra_path_between(g, request.source, request.destination, path, &path_len, &total_weight)) {
        child_send_message(write_fd, "NO_PATH");
        child_send_finished(write_fd); close(write_fd); _exit(0);
    }
    (void)total_weight;

    if (path_len == 1) {
        child_send_node_message(write_fd, "WAITING_FOR_NODE", path[0]);
        sem_wait(my_sem); // Wait for Parent's Scheduling decision!
        child_send_node_message(write_fd, "DESTINATION", path[0]);
        child_send_finished(write_fd); close(write_fd); _exit(0);
    }

    for (int i = 0; i < path_len - 1; i++) {
        int current = path[i];
        int next = path[i + 1];

        // Child asks parent for permission and goes to sleep
        child_send_node_message(write_fd, "WAITING_FOR_NODE", current);
        sem_wait(my_sem); 

        // Parent woke us up! We are in the critical section.
        child_send_node_message(write_fd, "ENTERED_NODE", current);
        if (i > 0) sleep_seconds(NODE_WAIT_SECONDS);

        // Leaving critical section
        child_send_left_node(write_fd, current, next);
        sleep_edge_steps(g, current, next);
    }

    child_send_node_message(write_fd, "WAITING_FOR_NODE", path[path_len - 1]);
    sem_wait(my_sem);
    child_send_node_message(write_fd, "DESTINATION", path[path_len - 1]);
    child_send_finished(write_fd); close(write_fd); _exit(0);
}

static void fork_milestone7_children(const Graph* g, TravelerSim travelers[], int traveler_count, sem_t* trav_sems[]) {
    int pipes[MAX_TRAVELERS][2];
    for (int i = 0; i < traveler_count; i++) {
        pipes[i][0] = -1; pipes[i][1] = -1;
        if (pipe(pipes[i]) == -1) { perror("pipe"); travelers[i].done = true; }
    }

    for (int i = 0; i < traveler_count; i++) {
        if (pipes[i][0] == -1 || pipes[i][1] == -1) continue;
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            close(pipes[i][0]); close(pipes[i][1]);
            pipes[i][0] = -1; pipes[i][1] = -1;
            travelers[i].done = true;
            continue;
        }

        if (pid == 0) {
            for (int j = 0; j < traveler_count; j++) {
                if (pipes[j][0] != -1) close(pipes[j][0]);
                if (j != i && pipes[j][1] != -1) close(pipes[j][1]);
            }
            run_milestone7_child(g, travelers[i].request, pipes[i][1], trav_sems[i]);
        }
        travelers[i].pid = pid;
    }

    for (int i = 0; i < traveler_count; i++) {
        if (pipes[i][1] != -1) close(pipes[i][1]);
        if (pipes[i][0] != -1 && travelers[i].pid > 0) {
            travelers[i].pipe_fd = pipes[i][0];
            travelers[i].pipe_open = true;
            if (!set_nonblocking(travelers[i].pipe_fd)) perror("fcntl");
        } else if (pipes[i][0] != -1) close(pipes[i][0]);
    }
}

void run_gui_milestone7(Graph* g, const TravelerRequest requests[], int traveler_count, const char* algo) {
    const int screen_width = 950;
    const int screen_height = 780;
    Vector2 positions[MAX_NODES];
    build_node_positions(g, positions, screen_width, screen_height);

    bool is_sjf = (strcmp(algo, "sjf") == 0);

    // Create a private semaphore for EACH traveler (Starts locked - 0)
    sem_t* trav_sems[MAX_TRAVELERS];
    char sem_names[MAX_TRAVELERS][SEM_NAME_SIZE];
    for (int i = 0; i < traveler_count; i++) {
        snprintf(sem_names[i], SEM_NAME_SIZE, "/osgraph_trav_%ld_%d", (long)getpid(), i);
        sem_unlink(sem_names[i]);
        trav_sems[i] = sem_open(sem_names[i], O_CREAT | O_EXCL, 0600, 0); 
    }

    TravelerSim travelers[MAX_TRAVELERS];
    for (int i = 0; i < traveler_count; i++) {
        // We use milestone4 init here because the parent needs to calculate Dijkstra 
        // in advance so it knows the t->total_weight for SJF calculations!
        init_milestone4_traveler(g, &travelers[i], &requests[i], positions, i);
        travelers[i].state = SIM_AT_NODE; 
        travelers[i].waiting_node = -1;
        travelers[i].waiting = false;
        if (!valid_node(g, requests[i].source)) travelers[i].has_path = false;
    }

    fork_milestone7_children(g, travelers, traveler_count, trav_sems);

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    char title[128];
    snprintf(title, sizeof(title), "OS Project - Milestone 7 Scheduler (%s)", is_sjf ? "SJF" : "FCFS");
    InitWindow(screen_width, screen_height, title);
    SetTargetFPS(60);

    int node_occupant[MAX_NODES];
    for (int i = 0; i < MAX_NODES; i++) node_occupant[i] = -1;
    
    int request_tickets[MAX_TRAVELERS] = {0};
    int global_ticket = 1;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        read_ipc_messages(travelers, traveler_count, positions);

        // 🎯 EXAM HOTSPOT 12: FREEING NODES
        // The scheduler first checks if nodes became free before assigning them.
        for (int i = 0; i < traveler_count; i++) {
            TravelerSim* t = &travelers[i];
            for (int n = 0; n < MAX_NODES; n++) {
                if (node_occupant[n] == t->id) {
                    if (t->done || t->state == SIM_MOVING || (t->waiting && t->waiting_node != n)) {
                        node_occupant[n] = -1; // Node is free!
                    }
                }
            }
        }

        // 2. SCHEDULER: Assign free nodes to waiting travelers based on FCFS or SJF
        for (int n = 0; n < MAX_NODES; n++) {
            if (node_occupant[n] == -1) {
                int best_candidate = -1;
                int best_score = -1;

                for (int i = 0; i < traveler_count; i++) {
                    TravelerSim* t = &travelers[i];
                    if (t->waiting && t->waiting_node == n && !t->done) {
                        // Assign a timestamp ticket for FCFS
                        if (request_tickets[i] == 0) request_tickets[i] = global_ticket++;

                        // 🎯 EXAM HOTSPOT 13: SCHEDULING ALGORITHM (SCORE)
                        // Modify this line to implement a new algorithm like LDF. 
                        // The traveler with the lowest 'score' wins the node.
                        int score = is_sjf ? t->total_weight : request_tickets[i];
                        
                        if (best_candidate == -1 || score < best_score) {
                            best_candidate = i;
                            best_score = score;
                        }
                    }
                }

                if (best_candidate != -1) {
                    node_occupant[n] = travelers[best_candidate].id;
                    request_tickets[best_candidate] = 0; // Reset ticket for next wait
                    
                    // 🎯 EXAM HOTSPOT 14: PARENT WAKES UP CHILD
                    // Parent calls sem_post on the chosen traveler's private semaphore.
                    sem_post(trav_sems[best_candidate]);
                }
            }
        }

        for (int i = 0; i < traveler_count; i++) {
            advance_ipc_traveler(g, &travelers[i], positions, dt);
            if (travelers[i].done) reap_child_nonblocking(&travelers[i]);
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        draw_traveler_status(travelers, traveler_count, "Milestone 7: Scheduling Algorithm", 
            is_sjf ? "SJF (Shortest Job First) Active" : "FCFS (First Come First Serve) Active");
        draw_graph(g, positions);
        draw_travelers(travelers, traveler_count);

        if (all_travelers_done(travelers, traveler_count)) {
            draw_finished_banner(screen_width, screen_height, "All travelers finished");
        }
        EndDrawing();
    }
    
    CloseWindow();
    cleanup_children(travelers, traveler_count);
    for (int i = 0; i < traveler_count; i++) {
        if (trav_sems[i] != SEM_FAILED) {
            sem_close(trav_sems[i]);
            sem_unlink(sem_names[i]);
        }
    }
}
