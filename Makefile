CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
RAYLIB_FLAGS = -lraylib -lm

DIJKSTRA_TARGET = dijkstra
SIM_TARGET = sim

DIJKSTRA_OBJS = main_dijkstra.o graph.o
SIM_OBJS = main_sim.o graph.o gui.o

.PHONY: all milestone1 milestone2 milestone3 clean

all: milestone3

milestone1: $(DIJKSTRA_TARGET)

milestone2: $(SIM_TARGET)

milestone3: $(SIM_TARGET)

$(DIJKSTRA_TARGET): $(DIJKSTRA_OBJS)
	$(CC) $(CFLAGS) -o $(DIJKSTRA_TARGET) $(DIJKSTRA_OBJS)

$(SIM_TARGET): $(SIM_OBJS)
	$(CC) $(CFLAGS) -o $(SIM_TARGET) $(SIM_OBJS) $(RAYLIB_FLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(DIJKSTRA_TARGET) $(SIM_TARGET)
