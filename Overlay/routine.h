#ifndef ROUTINE_H
#define ROUTINE_H

#include <vector>
#include <string>
#include <chrono>

// Forward declaration
class RoutineController;

// Operation types
enum class OperationType {
    MOVE,       // Instant movement to position
    REST,       // Stay at current position
    SMOOTH,     // Smooth linear movement
    SMOOTH_CIRCLE, // Smooth circular movement
    MOVE_AWAY_TOWARD // moves the crosshair away / toward
};

constexpr float TARGET_DEFAULT_DISTANCE = 2.0f; // Default distance in meters

// Routine stage definitions
#define MAX_ROUTINE_STAGE 22              // Highest routine stage number
#define COMPLETION_STAGE 23               // Completion stage number
#define CONVERGENCE_NOTIFY_STAGE 15       // Pre-convergence notification stage
#define CONVERGENCE_STAGE 16              // Convergence test stage
#define DILATION_STAGE_START 17           // First dilation stage  
#define DILATION_STAGE_END 22             // Last dilation stage

// Specific dilation stages
#define DILATION_NOTIFY_1_STAGE 17        // Pre-black screen notification
#define DILATION_BLACK_STAGE 18           // Black screen stage
#define DILATION_NOTIFY_2_STAGE 19        // Pre-white screen notification
#define DILATION_WHITE_STAGE 20           // White screen stage
#define DILATION_NOTIFY_3_STAGE 21        // Pre-gradient notification  
#define DILATION_GRADIENT_STAGE 22        // Gradient fade stage

// Structure to represent a single operation
struct Operation {
    OperationType type;
    
    // Parameters for different operations
    union {
        struct { float x, y; } move;
        struct { float seconds; } rest;
        struct { float x1, y1, x2, y2, seconds; } smooth;
        struct { float centerX, centerY, radius, seconds; bool clockwise; } circle;
        struct { 
            float x, y;             // Fixed screen position
            float startDistance;    // Starting distance in meters
            float endDistance;      // Ending distance in meters
            float seconds;          // Duration of movement
        } depth;
    } params;
    
    // Time tracking
    float duration;        // How long this operation should take
    float elapsedTime;     // How much time has elapsed in this operation
};

// Position structure for current target
struct TargetPosition {
    float yaw;        // Horizontal angle
    float pitch;      // Vertical angle
    float distance;   // Distance from viewer in meters
    uint32_t state;   // Current state flags
    bool written;     // has this sample been written?
};

// Routine parser and controller
class RoutineController {
public:
    uint32_t getStateFlags() const;

    // Constructor with configurable movement speed
    RoutineController(float maxMoveSpeed = 1.0f);
    
    // Parse a routine string into operations
    bool parseRoutine(const std::string& routineStr);
    
    // Load a routine from the predefined list by index
    bool loadRoutine(int routineIndex);
    
    // Step through the routine, returns target position
    TargetPosition step();
    
    // Reset the routine to start from the beginning
    void reset();
    
    // Check if routine is complete
    bool isComplete() const;
    
    // Get current operation index for progress tracking
    size_t getCurrentOperationIndex() const;
    
    // Get total operation count
    size_t getTotalOperationCount() const;
    
    int getTimeTillNext();

    // Get available routine names
    static std::vector<std::string> getRoutineNames();
    static bool m_stepWritten;
    static double_t m_globalAdvancedTime;
    static int m_routineStage;
    static double_t m_stageStartTime;

private:
    // Operation list for the current routine
    std::vector<Operation> m_operations;
    
    // Current state
    size_t m_currentOpIndex;
    TargetPosition m_currentPosition;
    TargetPosition m_targetPosition;
    int m_loadedRoutineIndex = -1; // -1 indicates no routine loaded
    
    // Timing
    std::chrono::time_point<std::chrono::high_resolution_clock> m_lastUpdateTime;
    bool m_routineStarted;
    
    // Configuration
    float m_maxMoveSpeed; // Maximum movement speed in units per second

    double_t m_elapsedTime;
    double_t m_lastRandomPointTime;
    
    // Helper methods
    bool parseOperation(const std::string& opStr);
    TargetPosition calculatePosition();
    TargetPosition calculateConvergencePosition();
    TargetPosition calculateDilationPosition();
    void handleStageProgression();
    
    // Convert screen coordinates (0-1) to yaw/pitch angles
    TargetPosition screenToAngles(float x, float y) const;
};

#endif // ROUTINE_H