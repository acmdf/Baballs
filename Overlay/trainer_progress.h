#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <regex>

struct TrainerProgress {
    // Current state
    int currentEpoch = 0;
    int totalEpochs = 0;
    int currentBatch = 0;
    int totalBatches = 0;
    
    // Loss tracking
    float currentLoss = 0.0f;
    float epochAverageLoss = 0.0f;
    std::vector<float> lossHistory;
    
    // Timing
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point epochStartTime;
    float epochDuration = 0.0f;
    
    // State flags
    bool isTraining = false;
    bool isComplete = false;
    bool hasError = false;
    std::string lastError;
};

class TrainerProgressParser {
public:
    TrainerProgressParser();
    
    // Parse a line of trainer output
    void ParseLine(const std::string& line);
    
    // Get current progress
    const TrainerProgress& GetProgress() const { return m_progress; }
    
    // Generate display text for overlay
    std::string GenerateProgressDisplay() const;
    
    // Reset parser state
    void Reset();

private:
    TrainerProgress m_progress;
    
    // Regex patterns for parsing
    std::regex m_epochStartPattern;
    std::regex m_batchProgressPattern;
    std::regex m_epochCompletePattern;
    std::regex m_trainingCompletePattern;
    std::regex m_errorPattern;
    
    // Helper methods
    std::string FormatTime(float seconds) const;
    std::string GenerateProgressBar(float progress, int width = 20) const;
    float CalculateETA() const;
    void UpdateLossHistory(float loss);
};