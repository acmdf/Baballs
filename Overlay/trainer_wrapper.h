#ifndef TRAINER_WRAPPER_H
#define TRAINER_WRAPPER_H

#include <string>
#include <functional>
#include "trainer_progress.h"

/**
 * @brief TrainerWrapper class for managing the training process
 * 
 * This class provides a simple interface to run the trainer executable
 * with appropriate callbacks for output and completion.
 */
class TrainerWrapper {
public:
    using OutputCallback = std::function<void(const std::string&)>;
    using ProgressCallback = std::function<void(const TrainerProgress&)>;
    using CompletionCallback = std::function<void()>;

    /**
     * @brief Constructor
     * 
     * @param trainerPath Path to the trainer executable (defaults to "trainer.exe")
     */
    explicit TrainerWrapper(const std::string& trainerPath = "calibration_runner.exe");

    /**
     * @brief Start the training process
     * 
     * @param datasetFile Path to the input dataset file
     * @param outputFile Path where the trained model will be saved
     * @param onOutput Callback function that receives output from the trainer
     * @param onCompleted Callback function that is called when training completes
     * @return true if the trainer was successfully started, false otherwise
     */
    bool start(
        const std::string& datasetFile,
        const std::string& outputFile,
        OutputCallback onOutput,
        ProgressCallback onProgress,
        CompletionCallback onCompleted
    );

    /**
     * @brief Check if the trainer is currently running
     * 
     * @return true if the trainer is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Get current training progress
     * 
     * @return const TrainerProgress& Current progress information
     */
    const TrainerProgress& getProgress() const;

private:
    std::string m_trainerPath;
    bool m_isRunning;
    TrainerProgressParser m_progressParser;
};

#endif // TRAINER_WRAPPER_H