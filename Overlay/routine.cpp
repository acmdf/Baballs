#include "routine.h"
#include "config.h"
#include "routines.h" // Your header with the calibration routines
#include "flags.h"
#include <sstream>
#include <cmath>
#include <algorithm>
#include <regex>
#include <iostream>

// Constants for angle conversion
constexpr float MAX_YAW_ANGLE = 45.0f;    // Maximum yaw angle in degrees
constexpr float MAX_PITCH_ANGLE = 30.0f;  // Maximum pitch angle in degrees

RoutineController::RoutineController(float maxMoveSpeed)
    : m_currentOpIndex(0)
    , m_routineStarted(false)
    , m_maxMoveSpeed(maxMoveSpeed)
{
    // Initialize positions
    TargetPosition current;
    TargetPosition target;

    current.pitch = target.pitch = 0;
    current.yaw = target.yaw = 0;
    current.state = target.state = 0;

    m_currentPosition = current;
    m_targetPosition = target;
}

/**
 * Gets the current state flags for this routine
 * 
 * @return An integer with flags representing the current routine state
 */
uint32_t RoutineController::getStateFlags() const 
{
    // Hard-coded approach to ensure correct flag generation
    uint32_t flags = 0;
    
    if (m_loadedRoutineIndex >= 0 && m_loadedRoutineIndex < 24) {
        flags |= (1U << (m_loadedRoutineIndex - 1));
    }
    
    // Determine current state directly
    if (!m_operations.empty() && m_currentOpIndex < m_operations.size()) {
        const Operation& op = m_operations[m_currentOpIndex];
        
        if (op.type == OperationType::REST) {
            flags = flags | FLAG_RESTING; // Explicit OR operation
        }else if (op.type == OperationType::SMOOTH || op.type == OperationType::SMOOTH_CIRCLE) {
            flags = flags | FLAG_IN_MOVEMENT; // Explicit OR operation
        }else if(op.type == OperationType::MOVE_AWAY_TOWARD){
            flags = flags | FLAG_CONVERGENCE;
        }
    }
    
    return flags;
}

bool RoutineController::parseRoutine(const std::string& routineStr)
{
    // Clear any existing operations
    m_operations.clear();
    m_currentOpIndex = 0;
    m_routineStarted = false;
    
    // Split the routine string by semicolons
    std::istringstream stream(routineStr);
    std::string opStr;
    
    while (std::getline(stream, opStr, ';')) {
        if (!opStr.empty()) {
            if (!parseOperation(opStr)) {
                std::cerr << "Failed to parse operation: " << opStr << std::endl;
                m_operations.clear();
                return false;
            }
        }
    }
    
    return !m_operations.empty();
}

bool RoutineController::loadRoutine(int routineIndex) {
    // Array of routine strings
    const char* routines[] = ALL_ROUTINES;
    
    // Check if index is valid
    if (routineIndex < 0 || routineIndex >= NUM_CALIBRATION_ROUTINES) {
        std::cerr << "Invalid routine index: " << routineIndex << std::endl;
        return false;
    }
    
    // Store the loaded routine index
    m_loadedRoutineIndex = routineIndex;
    
    // Parse the selected routine
    return parseRoutine(routines[routineIndex]);
}

bool RoutineController::parseOperation(const std::string& opStr)
{
    Operation op;
    op.elapsedTime = 0.0f;
    
    // Regular expressions for different operation types
    static const std::regex moveRegex(R"(move\s*\(\s*([0-9]+\.[0-9]+)\s*,\s*([0-9]+\.[0-9]+)\s*\))");
    static const std::regex restRegex(R"(rest\s*\(\s*([0-9]+\.[0-9]+)\s*\))");
    static const std::regex smoothRegex(R"(smooth\s*\(\s*([0-9]+\.[0-9]+)\s*,\s*([0-9]+\.[0-9]+)\s*,\s*([0-9]+\.[0-9]+)\s*,\s*([0-9]+\.[0-9]+)\s*,\s*([0-9]+\.[0-9]+)\s*\))");
    static const std::regex circleRegex(R"(smoothCircle\s*\(\s*([0-9]+\.[0-9]+)\s*,\s*([0-9]+\.[0-9]+)\s*,\s*([0-9]+\.[0-9]+)\s*,\s*([0-9]+\.[0-9]+)\s*,\s*([0-1])\s*\))");
    static const std::regex depthRegex(R"(moveDepth\s*\(\s*([0-9]+\.[0-9]+)\s*,\s*([0-9]+\.[0-9]+)\s*,\s*([0-9]+\.[0-9]+)\s*,\s*([0-9]+\.[0-9]+)\s*,\s*([0-9]+\.[0-9]+)\s*\))");
    
    std::smatch matches;
    
    // Try to match move operation
    if (std::regex_search(opStr, matches, moveRegex) && matches.size() == 3) {
        op.type = OperationType::MOVE;
        op.params.move.x = std::stof(matches[1].str());
        op.params.move.y = std::stof(matches[2].str());
        op.duration = 0.0f; // Instant
    }
    // Try to match rest operation
    else if (std::regex_search(opStr, matches, restRegex) && matches.size() == 2) {
        op.type = OperationType::REST;
        op.params.rest.seconds = std::stof(matches[1].str());
        op.duration = op.params.rest.seconds;
    }
    // Try to match smooth operation
    else if (std::regex_search(opStr, matches, smoothRegex) && matches.size() == 6) {
        op.type = OperationType::SMOOTH;
        op.params.smooth.x1 = std::stof(matches[1].str());
        op.params.smooth.y1 = std::stof(matches[2].str());
        op.params.smooth.x2 = std::stof(matches[3].str());
        op.params.smooth.y2 = std::stof(matches[4].str());
        op.params.smooth.seconds = std::stof(matches[5].str());
        op.duration = op.params.smooth.seconds;
    }
    // Try to match circle operation
    else if (std::regex_search(opStr, matches, circleRegex) && matches.size() == 6) {
        op.type = OperationType::SMOOTH_CIRCLE;
        op.params.circle.centerX = std::stof(matches[1].str());
        op.params.circle.centerY = std::stof(matches[2].str());
        op.params.circle.radius = std::stof(matches[3].str());
        op.params.circle.seconds = std::stof(matches[4].str());
        op.params.circle.clockwise = (std::stoi(matches[5].str()) == 1);
        op.duration = op.params.circle.seconds;
    }
    // Try to match depth operation
    else if (std::regex_search(opStr, matches, depthRegex) && matches.size() == 6) {
        op.type = OperationType::MOVE_AWAY_TOWARD;
        op.params.depth.x = std::stof(matches[1].str());
        op.params.depth.y = std::stof(matches[2].str());
        op.params.depth.startDistance = std::stof(matches[3].str());
        op.params.depth.endDistance = std::stof(matches[4].str());
        op.params.depth.seconds = std::stof(matches[5].str());
        op.duration = op.params.depth.seconds;
    }
    else {
        return false; // No match found
    }
    
    m_operations.push_back(op);
    return true;
}

TargetPosition RoutineController::step()
{
    // Initialize timing if this is the first call
    if (!m_routineStarted) {
        m_lastUpdateTime = std::chrono::high_resolution_clock::now();
        m_routineStarted = true;
    }
    
    // Calculate elapsed time since last update
    auto currentTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - m_lastUpdateTime).count();
    m_lastUpdateTime = currentTime;
    
    // If we have operations to process
    if (!m_operations.empty() && m_currentOpIndex < m_operations.size()) {
        Operation& currentOp = m_operations[m_currentOpIndex];
        
        // Update elapsed time for current operation
        currentOp.elapsedTime += deltaTime;
        
        // Check if we need to move to the next operation
        if (currentOp.elapsedTime >= currentOp.duration) {
            // Move to next operation
            m_currentOpIndex++;
            
            // If we have more operations, initialize the next one
            if (m_currentOpIndex < m_operations.size()) {
                currentOp = m_operations[m_currentOpIndex];
                currentOp.elapsedTime = 0.0f;
            }
        }
    }
    
    // Calculate current position based on active operation
    return calculatePosition();
}

TargetPosition RoutineController::calculatePosition()
{
    // If routine is complete or hasn't started, return current position
    if (m_operations.empty() || m_currentOpIndex >= m_operations.size()) {
        return m_currentPosition;
    }
    
    const Operation& currentOp = m_operations[m_currentOpIndex];
    float progress = 0.0f;
    
    // Calculate target position based on operation type
    switch (currentOp.type) {
        case OperationType::MOVE:
            // Instant movement
            m_currentPosition = screenToAngles(currentOp.params.move.x, currentOp.params.move.y);
            // Default distance when not specified
            m_currentPosition.distance = TARGET_DEFAULT_DISTANCE;
            break;
            
        case OperationType::REST:
            // No movement during rest, maintain current position
            break;
            
        case OperationType::SMOOTH:
            // Linear interpolation between start and end points
            if (currentOp.duration > 0.0f) {
                progress = std::min(currentOp.elapsedTime / currentOp.duration, 1.0f);
                
                // Calculate screen coordinates using linear interpolation
                float x = currentOp.params.smooth.x1 + progress * (currentOp.params.smooth.x2 - currentOp.params.smooth.x1);
                float y = currentOp.params.smooth.y1 + progress * (currentOp.params.smooth.y2 - currentOp.params.smooth.y1);
                
                m_currentPosition = screenToAngles(x, y);
                m_currentPosition.distance = TARGET_DEFAULT_DISTANCE;
            }
            break;
            
        case OperationType::SMOOTH_CIRCLE:
            // Circular movement
            if (currentOp.duration > 0.0f) {
                progress = std::min(currentOp.elapsedTime / currentOp.duration, 1.0f);
                
                // Calculate angle based on progress (0 to 2π)
                float angle = progress * 2.0f * M_PI;
                if (!currentOp.params.circle.clockwise) {
                    angle = 2.0f * M_PI - angle;
                }
                
                // Calculate position on circle
                float x = currentOp.params.circle.centerX + currentOp.params.circle.radius * cos(angle);
                float y = currentOp.params.circle.centerY + currentOp.params.circle.radius * sin(angle);
                
                m_currentPosition = screenToAngles(x, y);
                m_currentPosition.distance = TARGET_DEFAULT_DISTANCE;
            }
            break;
            
        case OperationType::MOVE_AWAY_TOWARD:
            // Depth movement while maintaining screen position
            if (currentOp.duration > 0.0f) {
                progress = std::min(currentOp.elapsedTime / currentOp.duration, 1.0f);
                
                // Keep x,y position fixed
                m_currentPosition = screenToAngles(currentOp.params.depth.x, currentOp.params.depth.y);
                
                // Interpolate distance
                m_currentPosition.distance = currentOp.params.depth.startDistance + 
                    progress * (currentOp.params.depth.endDistance - currentOp.params.depth.startDistance);
            }
            break;
    }
    
    // Update state flags
    m_currentPosition.state = getStateFlags();
    
    return m_currentPosition;
}

TargetPosition RoutineController::screenToAngles(float x, float y) const
{
    // Convert normalized screen coordinates (0-1) to yaw and pitch angles
    // Center (0.5, 0.5) corresponds to (0, 0) in angles
    TargetPosition pos;
    
    // Map x from 0-1 to -MAX_YAW_ANGLE to +MAX_YAW_ANGLE
    pos.yaw = (x - 0.5f) * 2.0f * MAX_YAW_ANGLE;
    
    // Map y from 0-1 to -MAX_PITCH_ANGLE to +MAX_PITCH_ANGLE
    // Note: y=0 is top of screen, but pitch is positive upward, so we invert
    pos.pitch = (0.5f - y) * 2.0f * MAX_PITCH_ANGLE;
    
    // Set default distance
    pos.distance = TARGET_DEFAULT_DISTANCE;
    
    // Initialize state to 0
    pos.state = 0;
    
    return pos;
}

void RoutineController::reset()
{
    m_currentOpIndex = 0;
    m_routineStarted = false;
    
    // Reset elapsed time for all operations
    for (auto& op : m_operations) {
        op.elapsedTime = 0.0f;
    }
    
    // Reset position to center if we have operations
    if (!m_operations.empty()) {
        // Find the first move operation to set initial position
        for (const auto& op : m_operations) {
            if (op.type == OperationType::MOVE) {
                m_currentPosition = screenToAngles(op.params.move.x, op.params.move.y);
                break;
            }
        }
    }
}

bool RoutineController::isComplete() const
{
    return !m_operations.empty() && m_currentOpIndex >= m_operations.size();
}

size_t RoutineController::getCurrentOperationIndex() const
{
    return m_currentOpIndex;
}

size_t RoutineController::getTotalOperationCount() const
{
    return m_operations.size();
}

std::vector<std::string> RoutineController::getRoutineNames()
{
    // Return the predefined routine names
    const char* names[] = ALL_ROUTINE_NAMES;
    return std::vector<std::string>(names, names + NUM_CALIBRATION_ROUTINES);
}