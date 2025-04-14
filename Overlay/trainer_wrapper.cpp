#include "trainer_wrapper.h"
#include "subprocess.h"
#include <iostream>

TrainerWrapper::TrainerWrapper(const std::string& trainerPath)
    : m_trainerPath(trainerPath)
    , m_isRunning(false)
{
}

bool TrainerWrapper::start(
    const std::string& datasetFile,
    const std::string& outputFile,
    OutputCallback onOutput,
    CompletionCallback onCompleted
) {
    if (m_isRunning) {
        std::cerr << "Training process is already running" << std::endl;
        return false;
    }

    // Prepare arguments for the trainer
    std::vector<std::string> args = { datasetFile, outputFile };

    // Start the trainer process
    bool success = spawnProcess(
        m_trainerPath,
        args,
        // Redirect stdout to the output callback
        [this, onOutput](const std::string& output) {
            onOutput(output);
        },
        // Redirect stderr to the output callback as well
        [this, onOutput](const std::string& output) {
            onOutput(output);
        },
        // Handle process completion
        [this, onCompleted](int exitCode) {
            m_isRunning = false;
            
            if (exitCode != 0) {
                std::cerr << "Trainer process exited with code: " << exitCode << std::endl;
            }
            
            onCompleted();
        }
    );

    if (success) {
        m_isRunning = true;
    }

    return success;
}

bool TrainerWrapper::isRunning() const {
    return m_isRunning;
}
