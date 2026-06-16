CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
RAYLIB_FLAGS = -lraylib -lm -pthread

DIJKSTRA_TARGET = dijkstra
SIM_TARGET = sim

DIJKSTRA_OBJS = main_dijkstra.o graph.o
SIM_OBJS = main_sim.o graph.o gui.o

.PHONY: all milestone1 milestone2 milestone3 milestone4 milestone5 milestone6 clean

all: milestone3

milestone1: $(DIJKSTRA_TARGET)

milestone2:
	$(CC) $(CFLAGS) -DSIM_MILESTONE=3 -o $(SIM_TARGET) main_sim.c graph.c gui.c $(RAYLIB_FLAGS)

milestone3:
	$(CC) $(CFLAGS) -DSIM_MILESTONE=3 -o $(SIM_TARGET) main_sim.c graph.c gui.c $(RAYLIB_FLAGS)

milestone4:
	$(CC) $(CFLAGS) -DSIM_MILESTONE=4 -o $(SIM_TARGET) main_sim.c graph.c gui.c $(RAYLIB_FLAGS)

milestone5:
	$(CC) $(CFLAGS) -DSIM_MILESTONE=5 -o $(SIM_TARGET) main_sim.c graph.c gui.c $(RAYLIB_FLAGS)

milestone6:
	$(CC) $(CFLAGS) -DSIM_MILESTONE=6 -o $(SIM_TARGET) main_sim.c graph.c gui.c $(RAYLIB_FLAGS)

$(DIJKSTRA_TARGET): $(DIJKSTRA_OBJS)
	$(CC) $(CFLAGS) -o $(DIJKSTRA_TARGET) $(DIJKSTRA_OBJS)

$(SIM_TARGET): $(SIM_OBJS)
	$(CC) $(CFLAGS) -o $(SIM_TARGET) $(SIM_OBJS) $(RAYLIB_FLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(DIJKSTRA_TARGET) $(SIM_TARGET)
