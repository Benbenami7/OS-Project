# OS Project - Graph Traffic Simulation
Students -
Ben Ben Ami
Eyal Zamero

This project implements a directed weighted graph, Dijkstra shortest path,
raylib graph visualization, single-traveler animation, and multi-traveler
process simulations.

## Requirements

- POSIX C environment with `fork`, `pipe`, `kill`, and `waitpid`
- `gcc`
- `raylib`
- `make`

## Input Formats

Milestones 1-3 still accept the original single-query format:

```txt
N M
src dst weight
src dst weight
...
query_src query_dst
```

Milestones 4-5 use the extended traveler format. Blank lines and lines whose
first non-space character is `#` are ignored:

```txt
# graph definition
N M
src dst weight
src dst weight
...
# travelers
K
source destination
source destination
...
```

Example file: `milestone45_example.txt`

## Build and Run

Milestone 1:

```bash
make milestone1
./dijkstra graph.txt
```

Milestone 2/3:

```bash
make milestone3
./sim graph.txt
```

Milestone 4:

```bash
make milestone4
./sim milestone45_example.txt
```

Milestone 5:

```bash
make milestone5
./sim milestone45_example.txt
```

Clean:

```bash
make clean
```

## Milestone 4

The parent reads the graph and traveler list, computes a Dijkstra path for every
traveler before calling `fork`, then creates one child per traveler. Each child
prints:

```txt
[PID] started
```

After printing, the child sleeps in `pause()` until the parent terminates it.
The parent owns the raylib GUI and animates all travelers at the same time. Each
traveler uses a distinct color. When a traveler reaches its destination, the
parent sends `SIGTERM` to that child and reaps it with `waitpid`.

## Milestone 5

Children are autonomous. The parent gives each child only the graph/traveler
data inherited through `fork`; it does not pass precomputed routes. Each child
computes its own Dijkstra path, simulates the route timing, and sends progress
messages whenever it arrives at a node.

The parent owns all terminal logging and raylib drawing. It reads child messages
without blocking the GUI loop and logs arrivals as:

```txt
[PID=1021] arrived at node 0 | next node: 2
[PID=1021] arrived at node 4 | DESTINATION
[PID=1021] finished
```

## IPC Choice

Milestone 5 uses one POSIX pipe per child. Pipes are simple, local to the
parent-child relationship created by `fork`, and fit the one-way progress stream
needed here. The parent closes every unused write end, marks read ends
non-blocking with `fcntl(O_NONBLOCK)`, polls them during each GUI frame, and
uses `waitpid` to prevent zombies.

## Notes and Limits

- Maximum nodes: 15.
- Maximum travelers: 64.
- Edge weights must be non-negative.
- Edge weight `W` is animated as `W` equal steps at 300 ms per step.
- Travelers wait 1 second only at intermediate nodes.
- Invalid traveler nodes, no-path travelers, and `source == destination` cases
  are handled without crashing.
