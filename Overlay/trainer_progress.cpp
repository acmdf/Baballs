#include "trainer_progress.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

TrainerProgressParser::TrainerProgressParser()
    : // Regex patterns based on trainer.cpp output
    m_epochStartPattern("=== Epoch (\\d+)/(\\d+) ===")
    , m_batchProgressPattern("Batch (\\d+)/(\\d+), Loss: ([0-9.]+)")
    , m_epochCompletePattern("Epoch (\\d+)/(\\d+) completed in ([0-9.]+)s\\. Average loss: ([0-9.]+)")
    , m_trainingCompletePattern("Training completed successfully!")
    , m_errorPattern("Error|ERROR|Failed|FAILED") {
    Reset();
}

void TrainerProgressParser::Reset() {
    m_progress = TrainerProgress();
    m_progress.startTime = std::chrono::steady_clock::now();
}

void TrainerProgressParser::ParseLine(const std::string& line) {
    std::smatch match;

    // Check for epoch start
    if (std::regex_search(line, match, m_epochStartPattern)) {
        m_progress.currentEpoch = std::stoi(match[1]);
        m_progress.totalEpochs = std::stoi(match[2]);
        m_progress.epochStartTime = std::chrono::steady_clock::now();
        m_progress.isTraining = true;
        return;
    }

    // Check for batch progress
    if (std::regex_search(line, match, m_batchProgressPattern)) {
        m_progress.currentBatch = std::stoi(match[1]);
        m_progress.totalBatches = std::stoi(match[2]);
        m_progress.currentLoss = std::stof(match[3]);
        UpdateLossHistory(m_progress.currentLoss);
        return;
    }

    // Check for epoch completion
    if (std::regex_search(line, match, m_epochCompletePattern)) {
        m_progress.epochDuration = std::stof(match[3]);
        m_progress.epochAverageLoss = std::stof(match[4]);
        UpdateLossHistory(m_progress.epochAverageLoss);
        return;
    }

    // Check for training completion
    if (std::regex_search(line, match, m_trainingCompletePattern)) {
        m_progress.isComplete = true;
        m_progress.isTraining = false;
        return;
    }

    // Check for errors
    if (std::regex_search(line, match, m_errorPattern)) {
        m_progress.hasError = true;
        m_progress.lastError = line;
        return;
    }

    // Parse training configuration if found
    if (line.find("Starting training with") != std::string::npos) {
        // Extract total epochs and batches if available
        std::regex configPattern("(\\d+) epochs, batch size (\\d+)");
        if (std::regex_search(line, match, configPattern)) {
            m_progress.totalEpochs = std::stoi(match[1]);
        }
        m_progress.isTraining = true;
        return;
    }
}

std::string TrainerProgressParser::GenerateProgressDisplay() const {
    if (!m_progress.isTraining && !m_progress.isComplete) {
        return "Training not started";
    }

    if (m_progress.hasError) {
        return "Training Error:\n" + m_progress.lastError;
    }

    if (m_progress.isComplete) {
        return "Training Complete!\nFinal Loss: " + std::to_string(m_progress.epochAverageLoss);
    }

    std::stringstream display;

    // Title
    display << "Neural Network Training\n\n";

    // Epoch progress
    if (m_progress.totalEpochs > 0) {
        float epochProgress = static_cast<float>(m_progress.currentEpoch) / m_progress.totalEpochs;
        display << "Epoch: " << m_progress.currentEpoch << "/" << m_progress.totalEpochs << "\n";
        display << GenerateProgressBar(epochProgress, 25) << " "
                << std::fixed << std::setprecision(1) << (epochProgress * 100) << "%\n\n";
    }

    // Batch progress (if available)
    if (m_progress.totalBatches > 0) {
        float batchProgress = static_cast<float>(m_progress.currentBatch) / m_progress.totalBatches;
        display << "Batch: " << m_progress.currentBatch << "/" << m_progress.totalBatches << "\n";
        display << GenerateProgressBar(batchProgress, 25) << " "
                << std::fixed << std::setprecision(1) << (batchProgress * 100) << "%\n\n";
    }

    // Current loss
    if (m_progress.currentLoss > 0) {
        display << "Current Loss: " << std::fixed << std::setprecision(6) << m_progress.currentLoss << "\n";
    }

    if (m_progress.epochAverageLoss > 0) {
        display << "Epoch Avg Loss: " << std::fixed << std::setprecision(6) << m_progress.epochAverageLoss << "\n";
    }

    // ETA
    float eta = CalculateETA();
    if (eta > 0) {
        display << "ETA: " << FormatTime(eta) << "\n";
    }

    // Loss graph will be rendered separately in GL

    return display.str();
}

void TrainerProgressParser::UpdateLossHistory(float loss) {
    // Clamp loss to maximum of 0.1 before adding to history
    float clampedLoss = std::min(loss, 0.1f);
    m_progress.lossHistory.push_back(clampedLoss);

    // Improved downsampling to maintain smooth trends
    const size_t maxDisplayPoints = 200; // Maximum points to display

    if (m_progress.lossHistory.size() > maxDisplayPoints * 1.5) {
        // Only downsample when we have significantly more than max points
        // This prevents frequent re-downsampling

        std::vector<float> downsampled;
        downsampled.reserve(maxDisplayPoints);

        // Calculate how many points to average together
        size_t totalPoints = m_progress.lossHistory.size();
        float pointsPerBucket = static_cast<float>(totalPoints) / maxDisplayPoints;

        // Process each bucket
        for (size_t bucket = 0; bucket < maxDisplayPoints; bucket++) {
            // Calculate the range of points for this bucket
            size_t startIdx = static_cast<size_t>(bucket * pointsPerBucket);
            size_t endIdx = static_cast<size_t>((bucket + 1) * pointsPerBucket);

            // Ensure we don't go out of bounds
            if (startIdx >= totalPoints)
                break;
            if (endIdx > totalPoints)
                endIdx = totalPoints;

            // Calculate average for this bucket
            float sum = 0.0f;
            size_t count = 0;
            for (size_t i = startIdx; i < endIdx; i++) {
                sum += m_progress.lossHistory[i];
                count++;
            }

            if (count > 0) {
                downsampled.push_back(sum / count);
            }
        }

        // Replace history with downsampled version
        m_progress.lossHistory = std::move(downsampled);
    }
}

std::string TrainerProgressParser::FormatTime(float seconds) const {
    int hours = static_cast<int>(seconds) / 3600;
    int minutes = (static_cast<int>(seconds) % 3600) / 60;
    int secs = static_cast<int>(seconds) % 60;

    std::stringstream ss;
    if (hours > 0) {
        ss << hours << "h ";
    }
    if (minutes > 0 || hours > 0) {
        ss << minutes << "m ";
    }
    ss << secs << "s";

    return ss.str();
}

std::string TrainerProgressParser::GenerateProgressBar(float progress, int width) const {
    // Use block characters for smooth progress bar
    const std::string blocks[] = { "", "▏", "▎", "▍", "▌", "▋", "▊", "▉", "█" };

    std::string bar;
    float scaledProgress = progress * width;
    int fullBlocks = static_cast<int>(scaledProgress);
    float remainder = scaledProgress - fullBlocks;

    // Add full blocks
    for (int i = 0; i < fullBlocks && i < width; i++) {
        bar += "█";
    }

    // Add partial block
    if (fullBlocks < width && remainder > 0) {
        int blockIndex = static_cast<int>(remainder * 8) + 1;
        if (blockIndex > 8)
            blockIndex = 8;
        bar += blocks[blockIndex];
        fullBlocks++;
    }

    // Add empty spaces
    for (int i = fullBlocks; i < width; i++) {
        bar += " ";
    }

    return "[" + bar + "]";
}

float TrainerProgressParser::CalculateETA() const {
    if (m_progress.totalEpochs == 0 || m_progress.currentEpoch == 0) {
        return 0.0f;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_progress.startTime).count();

    if (elapsed == 0) {
        return 0.0f;
    }

    float epochsPerSecond = static_cast<float>(m_progress.currentEpoch) / elapsed;
    int remainingEpochs = m_progress.totalEpochs - m_progress.currentEpoch;

    return remainingEpochs / epochsPerSecond;
}