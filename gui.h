#ifndef GUI_H
#define GUI_H

#include "graph.h"

// Launches the Raylib window and runs the milestone 3 animation.
void run_gui(Graph* g);
void run_gui_milestone4(Graph* g, const TravelerRequest travelers[], int traveler_count);
void run_gui_milestone5(Graph* g, const TravelerRequest travelers[], int traveler_count);
void run_gui_milestone6(Graph* g, const TravelerRequest travelers[], int traveler_count);
void run_gui_milestone7(Graph* g, const TravelerRequest requests[], int traveler_count, const char* algo);
#endif // GUI_H
