#include "subprocess.h"
#include <thread>
#include <iostream>
#include <array>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#endif

bool spawnProcess(
    const std::string& program,
    const std::vector<std::string>& params,
    std::function<void(const std::string&)> onStdOut,
    std::function<void(const std::string&)> onStdErr,
    std::function<void(int)> onComplete
) {
    return ProcessRunner::spawnProcess(program, params, onStdOut, onStdErr, onComplete);
}

bool ProcessRunner::spawnProcess(
    const std::string& program,
    const std::vector<std::string>& args,
    OutputCallback onStdOut,
    OutputCallback onStdErr,
    CompletionCallback onComplete
) {
#ifdef _WIN32
    return spawnProcessWindows(program, args, onStdOut, onStdErr, onComplete);
#else
    return spawnProcessUnix(program, args, onStdOut, onStdErr, onComplete);
#endif
}

#ifdef _WIN32
bool ProcessRunner::spawnProcessWindows(
    const std::string& program,
    const std::vector<std::string>& args,
    OutputCallback onStdOut,
    OutputCallback onStdErr,
    CompletionCallback onComplete
) {
    // Build command line
    std::string cmdLine = "\"" + program + "\"";
    for (const auto& arg : args) {
        cmdLine += " \"" + arg + "\"";
    }

    // Set up security attributes for pipes
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create pipes for stdout and stderr
    HANDLE stdoutRead, stdoutWrite;
    HANDLE stderrRead, stderrWrite;

    if (!CreatePipe(&stdoutRead, &stdoutWrite, &saAttr, 0) ||
        !CreatePipe(&stderrRead, &stderrWrite, &saAttr, 0)) {
        return false;
    }

    // Ensure the read handles are not inherited
    SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderrRead, HANDLE_FLAG_INHERIT, 0);

    // Set up process startup info
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = stdoutWrite;
    si.hStdError = stderrWrite;
    si.hStdInput = NULL;

    // Set up process info
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // Debug: Print command line
    printf("DEBUG: Attempting to spawn process: %s\n", cmdLine.c_str());

    // Create the child process
    BOOL success = CreateProcessA(
        NULL,                   // No module name (use command line)
        const_cast<LPSTR>(cmdLine.c_str()), // Command line
        NULL,                   // Process handle not inheritable
        NULL,                   // Thread handle not inheritable
        TRUE,                   // Handles are inherited
        0,                      // No creation flags
        NULL,                   // Use parent's environment block
        NULL,                   // Use parent's starting directory
        &si,                    // Pointer to STARTUPINFO
        &pi                     // Pointer to PROCESS_INFORMATION
    );

    printf("DEBUG: CreateProcessA result: %s\n", success ? "SUCCESS" : "FAILED");

    // Close pipe write-ends as we don't need them
    CloseHandle(stdoutWrite);
    CloseHandle(stderrWrite);

    if (!success) {
        DWORD error = GetLastError();
        printf("DEBUG: CreateProcessA failed with error code: %lu\n", error);
        CloseHandle(stdoutRead);
        CloseHandle(stderrRead);
        return false;
    }

    // Create threads to read from pipes
    std::thread stdoutThread([stdoutRead, onStdOut]() {
        printf("DEBUG: stdout reading thread started\n");
        char buffer[4096];
        DWORD bytesRead;
        bool firstRead = true;
        while (ReadFile(stdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            if (firstRead) {
                printf("DEBUG: First stdout data received (%lu bytes)\n", bytesRead);
                firstRead = false;
            }
            buffer[bytesRead] = '\0';
            onStdOut(std::string(buffer, bytesRead));
        }
        printf("DEBUG: stdout reading thread ending (ReadFile returned false or 0 bytes)\n");
        CloseHandle(stdoutRead);
    });

    std::thread stderrThread([stderrRead, onStdErr]() {
        printf("DEBUG: stderr reading thread started\n");
        char buffer[4096];
        DWORD bytesRead;
        bool firstRead = true;
        while (ReadFile(stderrRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            if (firstRead) {
                printf("DEBUG: First stderr data received (%lu bytes)\n", bytesRead);
                firstRead = false;
            }
            buffer[bytesRead] = '\0';
            onStdErr(std::string(buffer, bytesRead));
        }
        printf("DEBUG: stderr reading thread ending (ReadFile returned false or 0 bytes)\n");
        CloseHandle(stderrRead);
    });

    // Wait for process to complete
    std::thread completionThread([pi, onComplete]() {
        printf("DEBUG: completion thread started, waiting for process...\n");
        WaitForSingleObject(pi.hProcess, INFINITE);
        
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        printf("DEBUG: process completed with exit code: %lu\n", exitCode);
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        onComplete(static_cast<int>(exitCode));
    });

    // Detach threads to let them run independently
    stdoutThread.detach();
    stderrThread.detach();
    completionThread.detach();

    return true;
}
#else
bool ProcessRunner::spawnProcessUnix(
    const std::string& program,
    const std::vector<std::string>& args,
    OutputCallback onStdOut,
    OutputCallback onStdErr,
    CompletionCallback onComplete
) {
    // Create pipes for stdout and stderr
    int stdoutPipe[2];
    int stderrPipe[2];
    
    if (pipe(stdoutPipe) == -1 || pipe(stderrPipe) == -1) {
        return false;
    }
    
    // Fork the process
    pid_t pid = fork();
    
    if (pid < 0) {
        // Fork failed
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        close(stderrPipe[0]);
        close(stderrPipe[1]);
        return false;
    }
    
    if (pid == 0) {
        // Child process
        
        // Redirect stdout and stderr
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
        
        // Close unused pipe ends
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        close(stderrPipe[0]);
        close(stderrPipe[1]);
        
        // Prepare arguments for exec
        std::vector<char*> cargs;
        cargs.push_back(const_cast<char*>(program.c_str()));
        for (const auto& arg : args) {
            cargs.push_back(const_cast<char*>(arg.c_str()));
        }
        cargs.push_back(nullptr);  // Null-terminate the array
        
        // Execute the program
        execvp(program.c_str(), cargs.data());
        
        // If we get here, exec failed
        exit(EXIT_FAILURE);
    }
    
    // Parent process
    
    // Close write ends of pipes
    close(stdoutPipe[1]);
    close(stderrPipe[1]);
    
    // Set non-blocking mode for the pipes
    fcntl(stdoutPipe[0], F_SETFL, O_NONBLOCK);
    fcntl(stderrPipe[0], F_SETFL, O_NONBLOCK);
    
    // Create threads to read from pipes
    std::thread stdoutThread([stdoutPipe, onStdOut]() {
        char buffer[4096];
        ssize_t bytesRead;
        while (true) {
            bytesRead = read(stdoutPipe[0], buffer, sizeof(buffer) - 1);
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                onStdOut(std::string(buffer, bytesRead));
            } else if (bytesRead == 0 || (bytesRead == -1 && errno != EAGAIN)) {
                break;
            } else {
                // No data available, sleep a bit
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        close(stdoutPipe[0]);
    });
    
    std::thread stderrThread([stderrPipe, onStdErr]() {
        char buffer[4096];
        ssize_t bytesRead;
        while (true) {
            bytesRead = read(stderrPipe[0], buffer, sizeof(buffer) - 1);
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                onStdErr(std::string(buffer, bytesRead));
            } else if (bytesRead == 0 || (bytesRead == -1 && errno != EAGAIN)) {
                break;
            } else {
                // No data available, sleep a bit
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        close(stderrPipe[0]);
    });
    
    // Wait for process to complete
    std::thread completionThread([pid, onComplete]() {
        int status;
        waitpid(pid, &status, 0);
        
        int exitCode = 0;
        if (WIFEXITED(status)) {
            exitCode = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            exitCode = 128 + WTERMSIG(status);
        }
        
        onComplete(exitCode);
    });
    
    // Detach threads to let them run independently
    stdoutThread.detach();
    stderrThread.detach();
    completionThread.detach();
    
    return true;
}
#endif
