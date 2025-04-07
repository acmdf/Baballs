#ifndef SUBPROCESS_H
#define SUBPROCESS_H

#include <string>
#include <vector>
#include <functional>

/**
 * @brief Spawns a child process and provides callbacks for output and completion
 * 
 * This function creates a new process with the specified program and arguments,
 * and provides callbacks for standard output, standard error, and process completion.
 * 
 * @param program The path to the executable to run
 * @param params Command line arguments for the program
 * @param onStdOut Callback function that receives stdout output as it becomes available
 * @param onStdErr Callback function that receives stderr output as it becomes available
 * @param onComplete Callback function that is called when the process terminates with the exit code
 * @return true if the process was successfully started, false otherwise
 */
bool spawnProcess(
    const std::string& program,
    const std::vector<std::string>& params,
    std::function<void(const std::string&)> onStdOut,
    std::function<void(const std::string&)> onStdErr,
    std::function<void(int)> onComplete
);

/**
 * @brief ProcessRunner class that handles the details of spawning and managing processes
 * 
 * This class contains the platform-specific implementation details for process management.
 * It's generally not needed to use this class directly - use the spawnProcess function instead.
 */
class ProcessRunner {
public:
    using OutputCallback = std::function<void(const std::string&)>;
    using CompletionCallback = std::function<void(int)>;

    /**
     * @brief Spawns a child process with the specified parameters
     * 
     * @param program The path to the executable to run
     * @param args Command line arguments for the program
     * @param onStdOut Callback function for stdout output
     * @param onStdErr Callback function for stderr output
     * @param onComplete Callback function for process completion
     * @return true if the process was started successfully, false otherwise
     */
    static bool spawnProcess(
        const std::string& program,
        const std::vector<std::string>& args,
        OutputCallback onStdOut,
        OutputCallback onStdErr,
        CompletionCallback onComplete
    );

private:
#ifdef _WIN32
    static bool spawnProcessWindows(
        const std::string& program,
        const std::vector<std::string>& args,
        OutputCallback onStdOut,
        OutputCallback onStdErr,
        CompletionCallback onComplete
    );
#else
    static bool spawnProcessUnix(
        const std::string& program,
        const std::vector<std::string>& args,
        OutputCallback onStdOut,
        OutputCallback onStdErr,
        CompletionCallback onComplete
    );
#endif
};

#endif // SUBPROCESS_H