#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <time.h>
#endif
#include <string>
#include <unordered_map>

#include "rest_server.h"
#include <sstream>

int redirectOutputToLogFile(const char* logFilePath) {
    char filename[100];
    FILE* logFile = NULL;

    // Store original stdout and stderr for diagnostic messages
    FILE* originalStdout = stdout;
    FILE* originalStderr = stderr;

    // Generate timestamp-based filename if none provided
    if (logFilePath == NULL) {
        time_t now = time(NULL);
        struct tm* timeinfo = localtime(&now);
        strftime(filename, sizeof(filename), "log_%Y%m%d_%H%M%S.txt", timeinfo);
        logFilePath = filename;
    }

    // First try opening the file directly to test permissions
    logFile = fopen(logFilePath, "w");
    if (logFile == NULL) {
        fprintf(originalStderr, "ERROR: Cannot open log file '%s': %s\n",
                logFilePath, strerror(errno));
        return -1;
    }

    // Write a test message and flush it
    fprintf(logFile, "Log file initialized at: %s\n", __DATE__ " " __TIME__);
    fflush(logFile);
    fclose(logFile);

    // Now redirect stdout
    logFile = freopen(logFilePath, "a", stdout);
    if (logFile == NULL) {
        fprintf(originalStderr, "ERROR: Failed to redirect stdout: %s\n",
                strerror(errno));
        return -1;
    }

    // Redirect stderr separately (safer than using dup2)
    if (freopen(logFilePath, "a", stderr) == NULL) {
        fprintf(originalStdout, "ERROR: Failed to redirect stderr: %s\n",
                strerror(errno));
        return -1;
    }

    // Disable buffering to ensure output appears immediately
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("Log started successfully at: %s\n", __DATE__ " " __TIME__);
    fflush(stdout);

    return 0;
}

int main(int argc, char* argv[]) {
    // initialize rest server
    HTTPServer server(23951);

    redirectOutputToLogFile(NULL);

    // returns the status of the current calibration. if status=complete, you can
    // use the checkpoint at the path specified in /start_calibration
    server.register_handler(
        "/status",
        [](const std::unordered_map<std::string, std::string>& params) {
            printf("Got status\n");

            return "{\"result\":\"ok\"}";
        });

    server.register_handler(
        "/settings",
        [](const std::unordered_map<std::string, std::string>& params) {
            printf("Got settings\n");

            return "{\"result\":\"ok\"}";
        });

    server.register_handler(
        "/set_target",
        [](const std::unordered_map<std::string, std::string>& params) {
            printf("Got set_target with the following params:\n");
            for (const auto& param : params) {
                printf("  %s: %s\n", param.first.c_str(), param.second.c_str());
            }

            return "{\"result\":\"ok\"}";
        });

    server.register_handler(
        "/start_cameras",
        [](const std::unordered_map<std::string, std::string>& params) {
            printf("Got start_cameras\n");
            printf("Left param: %s\n", params.at("left").c_str());
            printf("Right param: %s\n", params.at("right").c_str());

            return "{\"result\":\"ok\", \"width\": 240, \"height\": 240}";
        });

    server.register_handler(
        "/start_calibration",
        [](const std::unordered_map<std::string, std::string>& params) {
            std::string decodedPath = params.at("onnx_filename");
            size_t pos = 0;
            while ((pos = decodedPath.find('%', pos)) != std::string::npos) {
                if (pos + 2 < decodedPath.length()) {
                    int hexValue;
                    std::istringstream iss(decodedPath.substr(pos + 1, 2));
                    iss >> std::hex >> hexValue;
                    decodedPath.replace(pos, 3, 1, static_cast<char>(hexValue));
                } else {
                    break;
                }
            }
            printf("Got start calibration with routine ID %s and model path %s\n",
                   params.at("routine_id").c_str(), decodedPath.c_str());

            return "{\"result\":\"ok\"}";
        });

    server.register_handler(
        "/stop_preview",
        [](const std::unordered_map<std::string, std::string>& params) {
            printf("Got stop_preview\n");

            return "{\"result\":\"ok\"}";
        });

    server.register_post_handler("/start_calibration_json",
                                 [](const auto& params, const std::string& body) {
                                     printf("Got start_calibration_json");

                                     return "{\"result\":\"ok\"}";
                                 });

    server.start();

    while (1) {
        Sleep(1000000);
    }

    return 0;
}