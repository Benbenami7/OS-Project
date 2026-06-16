#ifndef GUI_H
#define GUI_H

#include "graph.h"

// Launches the Raylib window and runs the milestone 3 animation.
void run_gui(Graph* g);
void run_gui_milestone4(Graph* g, const TravelerRequest travelers[], int traveler_count);
void run_gui_milestone5(Graph* g, const TravelerRequest travelers[], int traveler_count);

#endif // GUI_H
