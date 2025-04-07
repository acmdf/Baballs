#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <sys/types.h>
#include <sys/stat.h>

// Function prototypes
int count_lines(const char* filename);
int is_source_file(const char* filename);

long get_file_size(const char* filename) {
    struct _stat file_stat;
    if (_stat(filename, &file_stat) == 0) {
        return file_stat.st_size;
    }
    return -1;
}
int main(int argc, char* argv[]) {
    WIN32_FIND_DATA find_data;
    HANDLE find_handle;
    int total_lines = 0;
    int total_files = 0;
    const char* path = ".";  // Current directory by default
    char search_path[MAX_PATH];
    
    // Process command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "/clformat") == 0) {
            printf(" |---");
            return 0;
        } else {
            // Assume it's a path
            path = argv[i];
        }
    }


    // Use provided directory if specified
    if (argc > 1) {
        path = argv[1];
    }
    
    // Prepare the search path with wildcard
    snprintf(search_path, sizeof(search_path), "%s\\*", path);

    
    // Start finding files
    find_handle = FindFirstFile(search_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed to open directory: %s\n", path);
        return 1;
    }
    
    do {
        // Skip directories
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
        
        if (is_source_file(find_data.cFileName)) {
            char filepath[MAX_PATH];
            snprintf(filepath, sizeof(filepath), "%s\\%s", path, find_data.cFileName);
            
            int lines = count_lines(filepath);
            if (lines >= 0) {
                total_lines += lines;
                total_files++;
                //printf("%-50s %5d\n", find_data.cFileName, lines);
            }
        }
    } while (FindNextFile(find_handle, &find_data));
    
    FindClose(find_handle);
    
    printf("Total lines of code: %d\n", total_lines);
    printf("Final binary size: %dKB\n", get_file_size("gaze_overlay.exe") / 1024);
    
    return 0;
}

// Count lines in a file
int count_lines(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", filename);
        return -1;
    }
    
    int lines = 0;
    int ch, prev_ch = '\n';
    
    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\n') {
            lines++;
        }
        prev_ch = ch;
    }
    
    // Count the last line if it doesn't end with a newline
    if (prev_ch != '\n' && prev_ch != EOF) {
        lines++;
    }
    
    fclose(file);
    return lines;
}

// Check if the file has a source code extension
int is_source_file(const char* filename) {
    const char* extensions[] = {".c", ".cpp", ".cc", ".cxx", ".h", ".hpp", ".hxx"};
    int num_extensions = sizeof(extensions) / sizeof(extensions[0]);
    
    const char* dot = strrchr(filename, '.');
    if (dot) {
        for (int i = 0; i < num_extensions; i++) {
            if (_stricmp(dot, extensions[i]) == 0) {  // Case-insensitive comparison for Windows
                return 1;
            }
        }
    }
    
    return 0;
}