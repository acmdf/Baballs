// calibration_routines.h

#ifndef CALIBRATION_ROUTINES_H
#define CALIBRATION_ROUTINES_H

// Basic calibration with varied positions
#define CALIBRATION_ROUTINE_BASIC "move(0.512,0.487);rest(2.1);move(0.127,0.142);rest(1.7);move(0.873,0.138);rest(1.6);move(0.891,0.879);rest(1.8);move(0.134,0.912);rest(1.5);move(0.512,0.487);rest(1.2);"

// Extended calibration with irregular grid
#define CALIBRATION_ROUTINE_EXTENDED "move(0.512,0.487);rest(2.3);move(0.127,0.142);rest(1.6);move(0.483,0.118);rest(1.7);move(0.873,0.138);rest(1.8);move(0.132,0.487);rest(1.5);move(0.512,0.487);rest(1.4);move(0.887,0.523);rest(1.7);move(0.134,0.912);rest(1.6);move(0.523,0.876);rest(1.8);move(0.891,0.879);rest(1.7);move(0.512,0.487);rest(1.3);"

// Horizontal sweep with non-uniform spacing
#define CALIBRATION_ROUTINE_HORIZONTAL "move(0.512,0.487);rest(2.2);move(0.083,0.512);rest(1.8);move(0.231,0.478);rest(1.3);move(0.342,0.523);rest(1.1);move(0.437,0.491);rest(1.2);move(0.512,0.487);rest(1.4);move(0.627,0.504);rest(1.3);move(0.723,0.471);rest(1.2);move(0.836,0.517);rest(1.3);move(0.917,0.489);rest(1.5);move(0.512,0.487);rest(1.3);"

// Vertical sweep with non-uniform spacing
#define CALIBRATION_ROUTINE_VERTICAL "move(0.512,0.487);rest(2.1);move(0.489,0.076);rest(1.7);move(0.523,0.218);rest(1.2);move(0.496,0.327);rest(1.3);move(0.517,0.419);rest(1.1);move(0.512,0.487);rest(1.4);move(0.493,0.612);rest(1.3);move(0.527,0.736);rest(1.2);move(0.503,0.823);rest(1.3);move(0.519,0.927);rest(1.6);move(0.512,0.487);rest(1.2);"

// Diagonal calibration with varied points
#define CALIBRATION_ROUTINE_DIAGONAL_1 "move(0.512,0.487);rest(2.2);move(0.127,0.142);rest(1.7);move(0.214,0.243);rest(1.3);move(0.323,0.319);rest(1.1);move(0.417,0.421);rest(1.2);move(0.512,0.487);rest(1.4);move(0.624,0.597);rest(1.3);move(0.718,0.693);rest(1.1);move(0.827,0.813);rest(1.2);move(0.891,0.879);rest(1.6);move(0.512,0.487);rest(1.3);"

// Diagonal calibration (opposite direction) with varied points
#define CALIBRATION_ROUTINE_DIAGONAL_2 "move(0.512,0.487);rest(2.1);move(0.873,0.138);rest(1.6);move(0.792,0.217);rest(1.2);move(0.683,0.327);rest(1.3);move(0.613,0.404);rest(1.1);move(0.512,0.487);rest(1.4);move(0.423,0.578);rest(1.3);move(0.317,0.691);rest(1.2);move(0.218,0.791);rest(1.3);move(0.134,0.912);rest(1.7);move(0.512,0.487);rest(1.2);"

// Circular calibration with natural variations
#define CALIBRATION_ROUTINE_CIRCULAR "move(0.512,0.487);rest(2.3);move(0.512,0.237);rest(1.6);move(0.673,0.312);rest(1.2);move(0.781,0.487);rest(1.3);move(0.712,0.683);rest(1.4);move(0.512,0.813);rest(1.3);move(0.317,0.724);rest(1.5);move(0.213,0.487);rest(1.2);move(0.294,0.317);rest(1.3);move(0.512,0.237);rest(1.1);move(0.512,0.487);rest(1.4);"

// Spiral calibration with natural variations
#define CALIBRATION_ROUTINE_SPIRAL "move(0.512,0.487);rest(2.2);move(0.583,0.487);rest(1.3);move(0.593,0.573);rest(1.1);move(0.512,0.591);rest(1.2);move(0.437,0.583);rest(1.3);move(0.421,0.487);rest(1.4);move(0.427,0.403);rest(1.2);move(0.512,0.389);rest(1.3);move(0.587,0.412);rest(1.1);move(0.673,0.419);rest(1.2);move(0.691,0.487);rest(1.3);move(0.684,0.563);rest(1.2);move(0.673,0.647);rest(1.1);move(0.587,0.691);rest(1.3);move(0.512,0.712);rest(1.2);move(0.421,0.697);rest(1.1);move(0.337,0.673);rest(1.3);move(0.294,0.587);rest(1.2);move(0.313,0.487);rest(1.3);move(0.317,0.412);rest(1.1);move(0.347,0.327);rest(1.2);move(0.421,0.283);rest(1.3);move(0.512,0.273);rest(1.2);move(0.512,0.487);rest(1.4);"

// Random saccade movements with natural positions
#define CALIBRATION_ROUTINE_SACCADES "move(0.512,0.487);rest(2.1);move(0.237,0.683);rest(1.2);move(0.813,0.317);rest(1.3);move(0.127,0.489);rest(1.1);move(0.724,0.817);rest(1.2);move(0.291,0.213);rest(1.3);move(0.917,0.523);rest(1.2);move(0.489,0.893);rest(1.3);move(0.218,0.329);rest(1.1);move(0.783,0.721);rest(1.2);move(0.512,0.113);rest(1.3);move(0.512,0.487);rest(1.4);"

// Smooth pursuit - horizontal with natural variations
#define CALIBRATION_ROUTINE_SMOOTH_H "move(0.512,0.487);rest(2.2);smooth(0.127,0.523,0.873,0.487,4.7);rest(1.3);smooth(0.873,0.487,0.127,0.512,5.2);rest(1.2);move(0.512,0.487);rest(1.4);"

// Smooth pursuit - vertical with natural variations
#define CALIBRATION_ROUTINE_SMOOTH_V "move(0.512,0.487);rest(2.1);smooth(0.489,0.127,0.512,0.891,4.9);rest(1.3);smooth(0.512,0.891,0.489,0.127,5.3);rest(1.2);move(0.512,0.487);rest(1.5);"

// Smooth pursuit - circular with natural variations
#define CALIBRATION_ROUTINE_SMOOTH_CIRCLE "move(0.512,0.487);rest(2.3);smoothCircle(0.512,0.487,0.273,8.2,1);rest(1.4);smoothCircle(0.512,0.487,0.321,7.8,0);rest(1.3);move(0.512,0.487);rest(1.5);"

// Comprehensive calibration with natural variations
#define CALIBRATION_ROUTINE_COMPREHENSIVE "move(0.512,0.487);rest(2.2);move(0.127,0.142);rest(1.7);move(0.873,0.138);rest(1.6);move(0.891,0.879);rest(1.5);move(0.134,0.912);rest(1.7);move(0.512,0.487);rest(1.6);smooth(0.512,0.487,0.489,0.127,2.3);rest(1.2);smooth(0.489,0.127,0.512,0.891,4.1);rest(1.3);smooth(0.512,0.891,0.512,0.487,2.2);rest(1.4);smooth(0.512,0.487,0.127,0.523,2.1);rest(1.3);smooth(0.127,0.523,0.873,0.487,4.2);rest(1.2);smooth(0.873,0.487,0.512,0.487,2.3);rest(1.4);smoothCircle(0.512,0.487,0.287,6.3,1);rest(1.3);move(0.512,0.487);rest(1.5);"

// Peripheral vision calibration with natural variations
#define CALIBRATION_ROUTINE_PERIPHERAL "move(0.512,0.487);rest(2.2);move(0.063,0.487);rest(1.7);move(0.512,0.487);rest(1.3);move(0.937,0.512);rest(1.6);move(0.512,0.487);rest(1.2);move(0.489,0.067);rest(1.7);move(0.512,0.487);rest(1.3);move(0.523,0.943);rest(1.6);move(0.512,0.487);rest(1.2);move(0.073,0.063);rest(1.7);move(0.512,0.487);rest(1.3);move(0.927,0.057);rest(1.6);move(0.512,0.487);rest(1.2);move(0.937,0.931);rest(1.7);move(0.512,0.487);rest(1.3);move(0.063,0.943);rest(1.6);move(0.512,0.487);rest(1.5);"

// Quick calibration with natural variations
#define CALIBRATION_ROUTINE_QUICK "move(0.512,0.487);rest(1.6);move(0.127,0.142);rest(1.2);move(0.873,0.138);rest(1.3);move(0.891,0.879);rest(1.1);move(0.134,0.912);rest(1.2);move(0.512,0.487);rest(1.3);"

// Fine-grained central calibration with natural variations
#define CALIBRATION_ROUTINE_CENTRAL "move(0.512,0.487);rest(2.1);move(0.412,0.417);rest(1.3);move(0.613,0.423);rest(1.2);move(0.607,0.583);rest(1.3);move(0.417,0.587);rest(1.2);move(0.487,0.413);rest(1.3);move(0.613,0.512);rest(1.2);move(0.489,0.593);rest(1.3);move(0.413,0.523);rest(1.2);move(0.512,0.487);rest(1.4);"

// Dynamic range calibration with natural variations
#define CALIBRATION_ROUTINE_DYNAMIC "move(0.512,0.487);rest(2.2);smooth(0.512,0.487,0.817,0.523,1.1);rest(1.3);smooth(0.817,0.523,0.213,0.489,2.2);rest(1.2);smooth(0.213,0.489,0.512,0.817,1.2);rest(1.3);smooth(0.512,0.817,0.512,0.213,2.3);rest(1.2);move(0.317,0.293);rest(0.7);move(0.683,0.723);rest(0.6);move(0.293,0.723);rest(0.8);move(0.723,0.293);rest(0.7);move(0.512,0.487);rest(1.4);"

// Micro-saccade calibration (small movements around fixation points)
#define CALIBRATION_ROUTINE_MICRO_SACCADES "move(0.512,0.487);rest(2.2);move(0.523,0.497);rest(0.6);move(0.502,0.477);rest(0.7);move(0.517,0.482);rest(0.5);move(0.507,0.492);rest(0.6);move(0.512,0.487);rest(1.3);move(0.723,0.293);rest(1.2);move(0.734,0.303);rest(0.6);move(0.713,0.283);rest(0.7);move(0.728,0.288);rest(0.5);move(0.718,0.298);rest(0.6);move(0.723,0.293);rest(1.3);move(0.317,0.723);rest(1.2);move(0.328,0.733);rest(0.6);move(0.307,0.713);rest(0.7);move(0.322,0.718);rest(0.5);move(0.312,0.728);rest(0.6);move(0.317,0.723);rest(1.3);move(0.512,0.487);rest(1.4);"

// Natural reading pattern simulation
#define CALIBRATION_ROUTINE_READING "move(0.213,0.423);rest(1.3);smooth(0.213,0.423,0.783,0.423,2.1);rest(0.8);move(0.213,0.487);rest(0.7);smooth(0.213,0.487,0.783,0.487,2.1);rest(0.8);move(0.213,0.551);rest(0.7);smooth(0.213,0.551,0.783,0.551,2.1);rest(0.8);move(0.213,0.615);rest(0.7);smooth(0.213,0.615,0.783,0.615,2.1);rest(0.8);move(0.512,0.487);rest(1.4);"

// Z-pattern with varied speed
#define CALIBRATION_ROUTINE_Z_PATTERN "move(0.213,0.213);rest(1.5);smooth(0.213,0.213,0.783,0.213,1.8);rest(0.9);smooth(0.783,0.213,0.213,0.783,2.7);rest(0.8);smooth(0.213,0.783,0.783,0.783,1.8);rest(1.2);move(0.512,0.487);rest(1.4);"

// Figure-8 pattern with natural variations
#define CALIBRATION_ROUTINE_FIGURE_8 "move(0.512,0.487);rest(2.1);smoothCircle(0.367,0.367,0.153,3.7,1);rest(0.5);smoothCircle(0.657,0.607,0.153,3.7,0);rest(0.5);move(0.512,0.487);rest(1.4);"

#define CALIBRATION_ROUTINE_DEPTH "move(0.5,0.5);rest(2.0);\
moveDepth(0.5,0.5,2.0,0.5,3.0);rest(1.5);\
moveDepth(0.5,0.5,0.5,1.0,2.0);rest(1.5);\
moveDepth(0.5,0.5,1.0,0.3,2.5);rest(1.5);\
moveDepth(0.5,0.5,0.3,1.5,3.0);rest(1.5);\
moveDepth(0.5,0.5,1.5,0.4,2.5);rest(1.5);\
moveDepth(0.5,0.5,0.4,2.0,3.0);rest(1.5);\
moveDepth(0.5,0.5,2.0,0.25,3.5);rest(2.0);\
moveDepth(0.5,0.5,0.25,3.0,4.0);rest(1.5);\
moveDepth(0.5,0.5,3.0,1.0,2.5);rest(1.5);\
moveDepth(0.5,0.5,1.0,0.5,2.0);rest(1.5);\
moveDepth(0.5,0.5,0.5,2.0,3.0);rest(1.5);"



// List of all calibration routines for easy access
#define ALL_ROUTINES { \
    CALIBRATION_ROUTINE_BASIC, \
    CALIBRATION_ROUTINE_EXTENDED, \
    CALIBRATION_ROUTINE_HORIZONTAL, \
    CALIBRATION_ROUTINE_VERTICAL, \
    CALIBRATION_ROUTINE_DIAGONAL_1, \
    CALIBRATION_ROUTINE_DIAGONAL_2, \
    CALIBRATION_ROUTINE_CIRCULAR, \
    CALIBRATION_ROUTINE_SPIRAL, \
    CALIBRATION_ROUTINE_SACCADES, \
    CALIBRATION_ROUTINE_SMOOTH_H, \
    CALIBRATION_ROUTINE_SMOOTH_V, \
    CALIBRATION_ROUTINE_SMOOTH_CIRCLE, \
    CALIBRATION_ROUTINE_COMPREHENSIVE, \
    CALIBRATION_ROUTINE_PERIPHERAL, \
    CALIBRATION_ROUTINE_QUICK, \
    CALIBRATION_ROUTINE_CENTRAL, \
    CALIBRATION_ROUTINE_DYNAMIC, \
    CALIBRATION_ROUTINE_MICRO_SACCADES, \
    CALIBRATION_ROUTINE_READING, \
    CALIBRATION_ROUTINE_Z_PATTERN, \
    CALIBRATION_ROUTINE_FIGURE_8, \
    CALIBRATION_ROUTINE_DEPTH \
}

// Routine names for reference/display
#define ALL_ROUTINE_NAMES { \
    "Basic Calibration", \
    "Extended Calibration", \
    "Horizontal Sweep", \
    "Vertical Sweep", \
    "Diagonal Pattern 1", \
    "Diagonal Pattern 2", \
    "Circular Pattern", \
    "Spiral Pattern", \
    "Saccade Movements", \
    "Smooth Horizontal", \
    "Smooth Vertical", \
    "Smooth Circle", \
    "Comprehensive", \
    "Peripheral Vision", \
    "Quick Calibration", \
    "Central Fine-Grained", \
    "Dynamic Range", \
    "Micro-Saccades", \
    "Reading Pattern", \
    "Z-Pattern", \
    "Figure-8 Pattern", \
    "Depth Simulation" \
}

#define NUM_CALIBRATION_ROUTINES 22

#endif // CALIBRATION_ROUTINES_H