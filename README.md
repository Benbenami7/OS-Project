# OS Project - Graph Traffic Simulation

This project implements a directed weighted graph, Dijkstra shortest path, graph visualization with Raylib, and a moving entity animation along the shortest path.

## Input file format

```txt
N M
src dst weight
src dst weight
...
query_src query_dst
```

Example:

```txt
6 8
0 1 4
0 2 2
1 3 5
2 1 1
2 3 8
3 4 2
4 5 3
2 5 10
0 5
```

## Milestone 1

Build:

```bash
make milestone1
```

Run:

```bash
./dijkstra graph.txt
```

Description: loads the graph from a text file, runs Dijkstra from the source to the destination written in the file, and prints the shortest path and total weight. If there is no path, it prints `No path found`.

## Milestone 2

Build:

```bash
make milestone2
```

Run:

```bash
./sim graph.txt
```

Description: displays the graph in a Raylib window. Nodes are displayed as circles, edges are displayed as directed arrows, and edge weights are shown next to the arrows.

## Milestone 3

Build:

```bash
make milestone3
```

Run:

```bash
./sim graph.txt
```

Description: calculates the shortest path using Dijkstra and animates a moving entity along that path. The Play/Stop button starts and pauses the animation. Each edge with weight `W` is divided into `W` equal jumps, where each jump takes 300 milliseconds. The entity waits one second on every intermediate node, not including the source and destination. When the entity reaches the destination, a message is displayed on the screen.

## Clean

```bash
make clean
```

## Notes

- Maximum number of nodes: 15.
- Negative numbers are treated as invalid input.
- The project uses only standard C libraries and Raylib.
