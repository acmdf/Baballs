#include "trainer_wrapper.h"
#include "subprocess.h"
#include <iostream>
#include <sstream>

TrainerWrapper::TrainerWrapper(const std::string& trainerPath)
    : m_trainerPath(trainerPath)
    , m_isRunning(false) {
}

bool TrainerWrapper::start(
    const std::string& datasetFile,
    const std::string& outputFile,
    OutputCallback onOutput,
    ProgressCallback onProgress,
    CompletionCallback onCompleted) {
    if (m_isRunning) {
        std::cerr << "Training process is already running" << std::endl;
        return false;
    }

    // Reset progress parser
    m_progressParser.Reset();

    // Prepare arguments for Python script via venv
    std::vector<std::string> args = { "python", "trainermin.py", datasetFile, outputFile };

    // Start the trainer process using venv Python
    bool success = spawnProcess(
        args[0],
        std::vector<std::string>(args.begin() + 1, args.end()),
        // Redirect stdout to the output callback
        [this, onOutput, onProgress](const std::string& output) {
            onOutput(output);
            // Parse each line for progress updates
            std::istringstream iss(output);
            std::string line;
            while (std::getline(iss, line)) {
                m_progressParser.ParseLine(line);
                onProgress(m_progressParser.GetProgress());
            }
        },
        // Redirect stderr to the output callback as well
        [this, onOutput, onProgress](const std::string& output) {
            onOutput(output);
            // Parse stderr for errors too
            std::istringstream iss(output);
            std::string line;
            while (std::getline(iss, line)) {
                m_progressParser.ParseLine(line);
                onProgress(m_progressParser.GetProgress());
            }
        },
        // Handle process completion
        [this, onCompleted](int exitCode) {
            m_isRunning = false;

            if (exitCode != 0) {
                std::cerr << "Trainer process exited with code: " << exitCode << std::endl;
            }

            onCompleted();
        });

    if (success) {
        m_isRunning = true;
    }

    return success;
}

bool TrainerWrapper::isRunning() const {
    return m_isRunning;
}

const TrainerProgress& TrainerWrapper::getProgress() const {
    return m_progressParser.GetProgress();
}
