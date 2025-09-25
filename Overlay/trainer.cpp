// trainer.cpp
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <numeric>
#include <onnxruntime_cxx_api.h>
#include <onnxruntime_training_c_api.h>
#include <opencv2/opencv.hpp>
#include <random>
#include <string>
#include <vector>

#include "capture_data.h"
#include "capture_reader.h"
#include "flags.h"

#define STD_MIN(a, b) ((a) < (b) ? (a) : (b))

// Configuration constants
#define TRAIN_RESOLUTION 128
#define NUM_FRAMES       4 // Updated for new model (current frame + 3 previous frames)
#define NUM_CLASSES      3 // Updated for MicroChad model (3 outputs: pitch, yaw, convergence)
#define ENABLE_CUDA      1 // Set to 1 to enable CUDA, 0 to use CPU only

#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#undef max // Undefines the macro
#undef min // If you also have min issues
#else
#include <unistd.h>
#endif

int get_cpu_thread_count() {
    int num_threads = 0;

#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    num_threads = sysinfo.dwNumberOfProcessors;
#else
#ifdef _SC_NPROCESSORS_ONLN
    num_threads = sysconf(_SC_NPROCESSORS_ONLN);
#else
    num_threads = sysconf(_SC_NPROCESSORS_CONF);
#endif
#endif

    // Fallback to at least 1 if detection failed
    return (num_threads > 0) ? num_threads : 1;
}

// Convert RGBA uint32_t to float
float rgba_to_float(uint32_t rgba) {
    // Extract the red channel (or use a more sophisticated conversion)
    return ((rgba & 0xFF) / 255.0f);
}

// Helper function to convert string to wstring
std::wstring to_wstring(const std::string& str) {
    std::wstring result;
    result.reserve(str.size());
    for (char c : str) {
        result.push_back(static_cast<wchar_t>(c));
    }
    return result;
}

// Fast corruption detection (ported from trainerte2.py)
float calculate_row_pattern_consistency(const cv::Mat& image) {
    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image;
    }

    // Normalize to 0-1 range
    cv::Mat gray_norm;
    gray.convertTo(gray_norm, CV_32F, 1.0 / 255.0);

    // Calculate row means
    cv::Mat row_means;
    cv::reduce(gray_norm, row_means, 1, cv::REDUCE_AVG);

    // Calculate consistency (standard deviation of row differences)
    if (row_means.rows > 1) {
        cv::Mat row_diffs;
        cv::Mat shifted_means = row_means(cv::Range(1, row_means.rows), cv::Range::all());
        cv::Mat original_means = row_means(cv::Range(0, row_means.rows - 1), cv::Range::all());
        row_diffs = shifted_means - original_means;

        cv::Scalar mean_diff, stddev_diff;
        cv::meanStdDev(row_diffs, mean_diff, stddev_diff);
        return (float)stddev_diff[0]; // Cast to fix warning
    }
    return 0.0f;
}

class FastCorruptionDetector {
private:
    float base_threshold;
    float current_threshold;
    bool use_adaptive;
    int adaptation_window;
    std::deque<float> recent_values;
    int total_frames;
    int detected_corrupted_left;
    int detected_corrupted_right;
    int threshold_updates;

public:
    FastCorruptionDetector(float threshold = 0.022669f, bool adaptive = true, int window = 100)
        : base_threshold(threshold)
        , current_threshold(threshold)
        , use_adaptive(adaptive)
        , adaptation_window(window)
        , total_frames(0)
        , detected_corrupted_left(0)
        , detected_corrupted_right(0)
        , threshold_updates(0) { }

    void update_adaptive_threshold(float value) {
        if (!use_adaptive)
            return;

        recent_values.push_back(value);
        if (recent_values.size() > adaptation_window) {
            recent_values.pop_front();
        }

        if (recent_values.size() < 20)
            return;

        // Calculate median and MAD for robust statistics
        std::vector<float> values(recent_values.begin(), recent_values.end());
        std::sort(values.begin(), values.end());

        float median = values[values.size() / 2];

        // Calculate MAD (Median Absolute Deviation)
        std::vector<float> abs_deviations;
        for (float val : values) {
            abs_deviations.push_back(std::abs(val - median));
        }
        std::sort(abs_deviations.begin(), abs_deviations.end());
        float mad = abs_deviations[abs_deviations.size() / 2];

        // Set threshold as median + 3*MAD
        float adaptive_threshold = median + 3.0f * mad;

        // Clamp to reasonable bounds
        float min_threshold = base_threshold * 0.5f;
        float max_threshold = base_threshold * 3.0f;
        current_threshold = std::max(min_threshold, std::min(max_threshold, adaptive_threshold));
        threshold_updates++;
    }

    bool is_corrupted(const cv::Mat& frame, float* metric_value = nullptr, float* threshold_used = nullptr) {
        float metric = calculate_row_pattern_consistency(frame);
        update_adaptive_threshold(metric);

        bool corrupted = metric > current_threshold;

        if (metric_value)
            *metric_value = metric;
        if (threshold_used)
            *threshold_used = current_threshold;

        return corrupted;
    }

    struct FramePairResult {
        bool left_corrupted;
        bool right_corrupted;
        float left_value;
        float right_value;
        float left_threshold;
        float right_threshold;
    };

    FramePairResult process_frame_pair(const cv::Mat& left_frame, const cv::Mat& right_frame) {
        total_frames++;

        FramePairResult result;
        result.left_corrupted = is_corrupted(left_frame, &result.left_value, &result.left_threshold);
        result.right_corrupted = is_corrupted(right_frame, &result.right_value, &result.right_threshold);

        if (result.left_corrupted)
            detected_corrupted_left++;
        if (result.right_corrupted)
            detected_corrupted_right++;

        return result;
    }

    void get_stats() {
        printf("Corruption detection stats:\n");
        printf("  Total frames: %d\n", total_frames);
        printf("  Corrupted left: %d (%.2f%%)\n", detected_corrupted_left,
               100.0f * detected_corrupted_left / std::max(1, total_frames));
        printf("  Corrupted right: %d (%.2f%%)\n", detected_corrupted_right,
               100.0f * detected_corrupted_right / std::max(1, total_frames));
        printf("  Current threshold: %.6f\n", current_threshold);
        printf("  Threshold updates: %d\n", threshold_updates);
    }
};

// Structure to hold a temporal sequence of frames with pre-processed images
struct TemporalSequence {
    std::vector<AlignedFrame> frames; // Changed to AlignedFrame to match what read_capture_file returns
    bool is_valid;
    // Pre-processed image data to avoid repeated JPEG decoding
    std::vector<std::vector<float>> preprocessed_images; // [frame_idx][pixel_data]
};

// Function to extract temporal sequences from frames - updated to use AlignedFrame
std::vector<TemporalSequence> createTemporalSequences(const std::vector<AlignedFrame>& frames, int num_frames) {
    std::vector<TemporalSequence> sequences;

    if (frames.size() < num_frames) {
        printf("Not enough frames to create sequences\n");
        return sequences;
    }

    // Initialize corruption detector for dataset filtering
    FastCorruptionDetector corruption_detector;
    int corrupted_sequences = 0;

    for (size_t i = 0; i <= frames.size() - num_frames; i++) {
        TemporalSequence seq;

        // Get the most recent frame first to check if it's safe
        const auto& latest_frame = frames[i + num_frames - 1];

        // Check if the most recent frame has FLAG_GOOD_DATA set
        if (std::get<11>(latest_frame.label_data) & FLAG_GOOD_DATA) {
            // Decode images to check for corruption (matching trainerte2.py)
            static thread_local std::vector<uint32_t> left_eye_data;
            static thread_local std::vector<uint32_t> right_eye_data;
            int left_width, left_height, right_width, right_height;

            latest_frame.DecodeImageLeft(left_eye_data, left_width, left_height);
            latest_frame.DecodeImageRight(right_eye_data, right_width, right_height);

            // Convert to OpenCV format for corruption detection
            cv::Mat left_mat(left_height, left_width, CV_8UC4, left_eye_data.data());
            cv::Mat right_mat(right_height, right_width, CV_8UC4, right_eye_data.data());

            // Check for corruption
            auto corruption_result = corruption_detector.process_frame_pair(left_mat, right_mat);

            if (true) { // if (!corruption_result.left_corrupted && !corruption_result.right_corrupted) {
                seq.is_valid = true;

                // Collect all frames in the sequence
                for (int j = 0; j < num_frames; j++) {
                    seq.frames.push_back(frames[i + j]);
                }

                sequences.push_back(seq);
            } else {
                corrupted_sequences++;
            }
        }
    }

    printf("Created %zu valid temporal sequences from %zu frames\n",
           sequences.size(), frames.size());
    printf("Excluded %d corrupted sequences\n", corrupted_sequences);
    corruption_detector.get_stats();
    return sequences;
}

// Function to calculate dynamic label ranges from the dataset
struct LabelRanges {
    float pitch_min, pitch_max, pitch_range;
    float yaw_min, yaw_max, yaw_range;
    float convergence_max;
};

LabelRanges calculateLabelRanges(const std::vector<TemporalSequence>& sequences) {
    printf("Calculating dynamic label ranges from dataset...\n");

    std::vector<float> pitches, yaws, convergences;

    // Collect all label values from sequences
    for (const auto& sequence : sequences) {
        if (sequence.is_valid && !sequence.frames.empty()) {
            const auto& last_frame = sequence.frames.back();

            float pitch = std::get<0>(last_frame.label_data);
            float yaw = std::get<1>(last_frame.label_data);
            float convergence = std::get<2>(last_frame.label_data);

            pitches.push_back(pitch);
            yaws.push_back(yaw);
            convergences.push_back(convergence);
        }
    }

    if (pitches.empty()) {
        printf("Warning: No valid labels found for range calculation!\n");
        return { -32.0f, 32.0f, 64.0f, -32.0f, 32.0f, 64.0f, 1.0f };
    }

    LabelRanges ranges;

    // Calculate pitch ranges
    ranges.pitch_min = *std::min_element(pitches.begin(), pitches.end());
    ranges.pitch_max = *std::max_element(pitches.begin(), pitches.end());

    // Calculate yaw ranges
    ranges.yaw_min = *std::min_element(yaws.begin(), yaws.end());
    ranges.yaw_max = *std::max_element(yaws.begin(), yaws.end());

    // Calculate convergence max
    ranges.convergence_max = *std::max_element(convergences.begin(), convergences.end());

    // Calculate symmetric ranges (matching trainerte2.py logic)
    float pitch_abs_max = std::max(std::abs(ranges.pitch_min), std::abs(ranges.pitch_max));
    float yaw_abs_max = std::max(std::abs(ranges.yaw_min), std::abs(ranges.yaw_max));

    ranges.pitch_range = 2.0f * pitch_abs_max;
    ranges.yaw_range = 2.0f * yaw_abs_max;

    // Guard against degenerate case
    if (ranges.pitch_range < 1e-6f)
        ranges.pitch_range = 1e-6f;
    if (ranges.yaw_range < 1e-6f)
        ranges.yaw_range = 1e-6f;
    if (ranges.convergence_max < 1e-6f)
        ranges.convergence_max = 1e-6f;

    printf("Dynamic ranges calculated:\n");
    printf("  Pitch: [%.3f, %.3f] range=%.3f\n", ranges.pitch_min, ranges.pitch_max, ranges.pitch_range);
    printf("  Yaw: [%.3f, %.3f] range=%.3f\n", ranges.yaw_min, ranges.yaw_max, ranges.yaw_range);
    printf("  Convergence max: %.3f\n", ranges.convergence_max);

    return ranges;
}

// Function to print parameter info and check for gradient flow
void printParameterInfo(OrtTrainingSession* training_session, const OrtApi* g_ort_api,
                        const OrtTrainingApi* g_ort_training_api,
                        std::vector<float>* prev_params = nullptr) {

    // Get size of all parameters (both trainable and non-trainable)
    size_t all_params_size = 0;
    OrtStatus* status = g_ort_training_api->GetParametersSize(training_session, &all_params_size, false);
    if (status != NULL) {
        const char* error_message = g_ort_api->GetErrorMessage(status);
        fprintf(stderr, "Error getting all parameters size: %s\n", error_message);
        g_ort_api->ReleaseStatus(status);
        return;
    }

    // Get size of trainable parameters only
    size_t trainable_params_size = 0;
    status = g_ort_training_api->GetParametersSize(training_session, &trainable_params_size, true);
    if (status != NULL) {
        const char* error_message = g_ort_api->GetErrorMessage(status);
        fprintf(stderr, "Error getting trainable parameters size: %s\n", error_message);
        g_ort_api->ReleaseStatus(status);
        return;
    }

    // Calculate non-trainable parameters size
    size_t non_trainable_params_size = all_params_size - trainable_params_size;

    printf("===== Parameter Information =====\n");
    printf("Total parameters: %zu\n", all_params_size);
    printf("Trainable parameters: %zu (%.2f%%)\n", trainable_params_size,
           (float)trainable_params_size / all_params_size * 100.0f);
    printf("Frozen parameters: %zu (%.2f%%)\n", non_trainable_params_size,
           (float)non_trainable_params_size / all_params_size * 100.0f);

    // Create memory info for parameter tensor
    OrtMemoryInfo* memory_info = nullptr;
    status = g_ort_api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &memory_info);
    if (status != NULL) {
        const char* error_message = g_ort_api->GetErrorMessage(status);
        fprintf(stderr, "Error creating memory info: %s\n", error_message);
        g_ort_api->ReleaseStatus(status);
        return;
    }

    // Get current parameter values
    std::vector<float> current_params(trainable_params_size);
    const int64_t shape[] = { (int64_t)trainable_params_size };
    OrtValue* params_tensor = nullptr;

    status = g_ort_api->CreateTensorWithDataAsOrtValue(
        memory_info,
        current_params.data(),
        current_params.size() * sizeof(float),
        shape,
        1,
        ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
        &params_tensor);

    if (status != NULL) {
        const char* error_message = g_ort_api->GetErrorMessage(status);
        fprintf(stderr, "Error creating parameter tensor: %s\n", error_message);
        g_ort_api->ReleaseStatus(status);
        g_ort_api->ReleaseMemoryInfo(memory_info);
        return;
    }

    // Copy parameters to our buffer
    status = g_ort_training_api->CopyParametersToBuffer(
        training_session,
        params_tensor,
        true // Only trainable parameters
    );

    if (status != NULL) {
        const char* error_message = g_ort_api->GetErrorMessage(status);
        fprintf(stderr, "Error copying parameters to buffer: %s\n", error_message);
        g_ort_api->ReleaseStatus(status);
        g_ort_api->ReleaseValue(params_tensor);
        g_ort_api->ReleaseMemoryInfo(memory_info);
        return;
    }

    // Print sample of parameters
    printf("Parameter samples: ");
    for (size_t i = 0; i < 5 && i < trainable_params_size; i++) {
        printf("%g ", current_params[i]);
    }
    printf("...\n");

    // Check if parameters have changed from previous values
    if (prev_params != nullptr && prev_params->size() == trainable_params_size) {
        float total_diff = 0.0f;
        int changed_count = 0;

        for (size_t i = 0; i < trainable_params_size; i++) {
            float diff = std::abs(current_params[i] - (*prev_params)[i]);
            total_diff += diff;
            if (diff > 1e-6) {
                changed_count++;
            }
        }

        printf("Gradient movement: %g (%.2f%% of parameters changed)\n",
               total_diff, (float)changed_count / trainable_params_size * 100.0f);

        // Update previous parameters
        *prev_params = current_params;
    } else if (prev_params != nullptr) {
        // First time storing parameters
        *prev_params = current_params;
    }

    // Clean up
    g_ort_api->ReleaseValue(params_tensor);
    g_ort_api->ReleaseMemoryInfo(memory_info);

    printf("================================\n");
}

int main(int argc, char* argv[]) {
    // Default file paths
    std::string capture_file = "capture(2).bin";
    std::string onnx_model_path = "tuned_temporal_eye_tracking.onnx";

    // Check if command line arguments are provided
    if (argc >= 2) {
        capture_file = argv[1]; // First argument is the capture file
    }

    if (argc >= 3) {
        onnx_model_path = argv[2]; // Second argument is the output file
    }

    printf("Loading capture file: %s\n", capture_file.c_str());

    // Load the capture file
    auto frames = read_capture_file(capture_file);

    if (frames.empty()) {
        fprintf(stderr, "No frames loaded from capture file\n");
        return 1;
    }

    printf("Loaded %zu frames from capture file\n", frames.size());

    // Create temporal sequences
    auto sequences = createTemporalSequences(frames, NUM_FRAMES);

    if (sequences.empty()) {
        fprintf(stderr, "No valid temporal sequences created\n");
        return 1;
    }

    // Calculate dynamic label ranges (matching trainerte2.py)
    LabelRanges label_ranges = calculateLabelRanges(sequences);

    printf("DEBUG: About to initialize ONNX Runtime...\n");
    fflush(stdout);

    // Initialize ONNX Runtime
    printf("DEBUG: Getting ORT API...\n");
    fflush(stdout);
    const OrtApi* g_ort_api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    printf("DEBUG: Getting ORT Training API...\n");
    fflush(stdout);
    const OrtTrainingApi* g_ort_training_api = g_ort_api->GetTrainingApi(ORT_API_VERSION);
    printf("DEBUG: ONNX APIs obtained successfully\n");
    fflush(stdout);

    // Create environment
    printf("DEBUG: Creating ONNX environment...\n");
    fflush(stdout);
    OrtEnv* env = NULL;
    OrtStatus* status = g_ort_api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "TemporalEyeTracker", &env);
    if (status != NULL) {
        const char* error_message = g_ort_api->GetErrorMessage(status);
        fprintf(stderr, "Error creating environment: %s\n", error_message);
        g_ort_api->ReleaseStatus(status);
        return 1;
    }
    printf("DEBUG: Environment created successfully\n");
    fflush(stdout);

    // Create session options
    printf("DEBUG: Creating session options...\n");
    fflush(stdout);
    OrtSessionOptions* session_options = NULL;
    status = g_ort_api->CreateSessionOptions(&session_options);
    if (status != NULL) {
        const char* error_message = g_ort_api->GetErrorMessage(status);
        fprintf(stderr, "Error creating session options: %s\n", error_message);
        g_ort_api->ReleaseStatus(status);
        g_ort_api->ReleaseEnv(env);
        return 1;
    }
    printf("DEBUG: Session options created successfully\n");
    fflush(stdout);

    // Set session options with optimizations
    printf("DEBUG: Setting graph optimization level...\n");
    fflush(stdout);
    g_ort_api->SetSessionGraphOptimizationLevel(session_options, ORT_ENABLE_ALL); // Enable all optimizations
    printf("DEBUG: Graph optimization set successfully\n");
    fflush(stdout);

#if ENABLE_CUDA
    // Try to use GPU if available
    printf("DEBUG: Trying to set up CUDA provider...\n");
    fflush(stdout);
    OrtStatus* gpu_status = g_ort_api->SessionOptionsAppendExecutionProvider_CUDA(session_options, 0);
    if (gpu_status != NULL) {
        printf("CUDA not available, falling back to CPU\n");
        g_ort_api->ReleaseStatus(gpu_status);

        printf("DEBUG: Setting up CPU threading...\n");
        fflush(stdout);
        uint32_t threads = get_cpu_thread_count() - 1; // Leave one core free
        if (threads < 1)
            threads = 1;

        g_ort_api->SetIntraOpNumThreads(session_options, threads);
        g_ort_api->SetInterOpNumThreads(session_options, threads);
        printf("Using %u CPU threads\n", threads);
    } else {
        printf("Using CUDA GPU acceleration\n");
    }
#else
    // Use CPU only
    printf("DEBUG: CUDA disabled, using CPU only...\n");
    fflush(stdout);

    printf("DEBUG: Setting up CPU threading...\n");
    fflush(stdout);
    uint32_t threads = get_cpu_thread_count() - 1; // Leave one core free
    if (threads < 1)
        threads = 1;

    g_ort_api->SetIntraOpNumThreads(session_options, threads);
    g_ort_api->SetInterOpNumThreads(session_options, threads);
    printf("Using %u CPU threads\n", threads);
#endif
    printf("DEBUG: Execution provider setup complete\n");
    fflush(stdout);

    // Paths to model artifacts
    std::string checkpoint_path = "onnx_artifacts/training/checkpoint";
    std::string training_model_path = "onnx_artifacts/training/training_model.onnx";
    std::string eval_model_path = "onnx_artifacts/training/eval_model.onnx";
    std::string optimizer_model_path = "onnx_artifacts/training/optimizer_model.onnx";

    // Load checkpoint
    OrtCheckpointState* checkpoint_state = NULL;
    status = g_ort_training_api->LoadCheckpoint(to_wstring(checkpoint_path).c_str(), &checkpoint_state);

    if (status != NULL) {
        const char* error_message = g_ort_api->GetErrorMessage(status);
        fprintf(stderr, "Error loading checkpoint: %s\n", error_message);
        g_ort_api->ReleaseStatus(status);
        g_ort_api->ReleaseSessionOptions(session_options);
        g_ort_api->ReleaseEnv(env);
        return 1;
    }

    printf("Checkpoint loaded successfully\n");
    fflush(stdout);

    // Create training session
    printf("Creating training session...\n");
    printf("Training model: %s\n", training_model_path.c_str());
    printf("Eval model: %s\n", eval_model_path.c_str());
    printf("Optimizer model: %s\n", optimizer_model_path.c_str());
    fflush(stdout);
    OrtTrainingSession* training_session = NULL;
    status = g_ort_training_api->CreateTrainingSession(
        env,
        session_options,
        checkpoint_state,
        to_wstring(training_model_path).c_str(),
        to_wstring(eval_model_path).c_str(),
        to_wstring(optimizer_model_path).c_str(),
        &training_session);

    if (status != NULL) {
        const char* error_message = g_ort_api->GetErrorMessage(status);
        fprintf(stderr, "Error creating training session: %s\n", error_message);
        g_ort_api->ReleaseStatus(status);
        g_ort_training_api->ReleaseCheckpointState(checkpoint_state);
        g_ort_api->ReleaseSessionOptions(session_options);
        g_ort_api->ReleaseEnv(env);
        return 1;
    }

    printf("Training session created successfully!\n");
    fflush(stdout);

    // Vector to track parameter changes
    std::vector<float> previous_params;

    // Print initial parameter info
    printf("Initial parameter information:\n");
    printParameterInfo(training_session, g_ort_api, g_ort_training_api, &previous_params);

    // Set learning rate
    float learning_rate = 1e-4f; // Match the learning rate from Python trainer
    status = g_ort_training_api->SetLearningRate(training_session, learning_rate);
    if (status != NULL) {
        const char* error_message = g_ort_api->GetErrorMessage(status);
        fprintf(stderr, "Error setting learning rate: %s\n", error_message);
        g_ort_api->ReleaseStatus(status);
    } else {
        printf("Learning rate set to: %f\n", learning_rate);
    }

    // Verify learning rate was set correctly
    float current_lr = 0.0f;
    status = g_ort_training_api->GetLearningRate(training_session, &current_lr);
    if (status == NULL) {
        printf("Confirmed learning rate: %f\n", current_lr);
    } else {
        const char* error_message = g_ort_api->GetErrorMessage(status);
        fprintf(stderr, "Error getting learning rate: %s\n", error_message);
        g_ort_api->ReleaseStatus(status);
    }

    // Create memory info for tensors
    OrtMemoryInfo* memory_info = NULL;
    status = g_ort_api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &memory_info);
    if (status != NULL) {
        const char* error_message = g_ort_api->GetErrorMessage(status);
        fprintf(stderr, "Error creating memory info: %s\n", error_message);
        g_ort_api->ReleaseStatus(status);
        g_ort_training_api->ReleaseTrainingSession(training_session);
        g_ort_training_api->ReleaseCheckpointState(checkpoint_state);
        g_ort_api->ReleaseSessionOptions(session_options);
        g_ort_api->ReleaseEnv(env);
        return 1;
    }

    // Create indices for shuffling
    std::vector<size_t> indices(sequences.size());
    std::iota(indices.begin(), indices.end(), 0); // Fill with 0, 1, 2, ...

    // Training configuration
    const int num_epochs = 4;          // Increased from 2 to 5
    const size_t batch_size = 32;      // Match Python trainer
    const size_t check_interval = 500; // Check parameters every N batches (reduced frequency)
    const size_t save_interval = 16;   // Save checkpoint every N epochs

    printf("Starting training with %zu sequences, %d epochs, batch size %zu\n",
           sequences.size(), num_epochs, (size_t)batch_size);

    // Track overall stats
    float best_loss = std::numeric_limits<float>::max();

    // Pre-allocate batch tensors to avoid repeated allocations
    std::vector<float> batch_images(batch_size * 2 * NUM_FRAMES * TRAIN_RESOLUTION * TRAIN_RESOLUTION);
    std::vector<float> batch_labels(batch_size * NUM_CLASSES);

    // Training loop
    auto training_start_time = std::chrono::steady_clock::now();

    for (int epoch = 0; epoch < num_epochs; epoch++) {
        auto epoch_start_time = std::chrono::steady_clock::now();
        printf("\n=== Epoch %d/%d ===\n", epoch + 1, num_epochs);

        // Shuffle data for this epoch
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(indices.begin(), indices.end(), g);

        // Track metrics
        float epoch_loss_sum = 0.0f;
        size_t batch_count = 0;

        // Process data in batches
        for (size_t batch_start = 0; batch_start < sequences.size(); batch_start += batch_size) {
            // Determine actual batch size (may be smaller for the last batch)
            size_t current_batch_size = STD_MIN(batch_size, sequences.size() - batch_start);

            // Resize pre-allocated batch vectors instead of creating new ones
            size_t required_image_size = current_batch_size * 2 * NUM_FRAMES * TRAIN_RESOLUTION * TRAIN_RESOLUTION;
            size_t required_label_size = current_batch_size * NUM_CLASSES;

            if (batch_images.size() != required_image_size) {
                batch_images.resize(required_image_size);
            }
            if (batch_labels.size() != required_label_size) {
                batch_labels.resize(required_label_size);
            }

            // Fill batch with data
            for (size_t i = 0; i < current_batch_size; i++) {
                const auto& sequence = sequences[indices[batch_start + i]];

                // Use the last frame for labels (most recent)
                const auto& last_frame = sequence.frames.back();

                // DEBUG: Check frame validity
                // printf("Processing sequence %zu, frame timestamp: %llu\n",
                //       indices[batch_start + i], last_frame.label_timestamp);

                // Extract MicroChad parameters using dynamic normalization (matching trainerte2.py)
                float raw_pitch = std::get<0>(last_frame.label_data);
                float raw_yaw = std::get<1>(last_frame.label_data);
                float raw_convergence = std::get<2>(last_frame.label_data);

                // Apply dynamic normalization like trainerte2.py
                float pitch = (raw_pitch - std::min(-label_ranges.pitch_max, label_ranges.pitch_min)) / label_ranges.pitch_range;
                float yaw = (raw_yaw - std::min(-label_ranges.yaw_max, label_ranges.yaw_min)) / label_ranges.yaw_range;
                float convergence = raw_convergence / label_ranges.convergence_max;

                // DEBUG: Check for invalid values
                float all_params[] = { pitch, yaw, convergence };
                bool has_invalid = false;
                for (int p = 0; p < 3; p++) {
                    if (!std::isfinite(all_params[p])) {
                        printf("ERROR: Invalid value at param %d: %f\n", p, all_params[p]);
                        has_invalid = true;
                    }
                }
                // if (i == 0) {
                //     printf("Sample %zu labels: pitch=%.3f yaw=%.3f convergence=%.3f\n",
                //            i, pitch, yaw, convergence);
                // }
                if (has_invalid) {
                    printf("Skipping batch due to invalid values\n");
                    continue;
                }

                // Fill batch labels with 3 parameters for MicroChad model
                batch_labels[i * NUM_CLASSES + 0] = pitch;
                batch_labels[i * NUM_CLASSES + 1] = yaw;
                batch_labels[i * NUM_CLASSES + 2] = convergence;

                // Process all frames in the sequence (most recent frame first)
                for (int frame_idx = 0; frame_idx < NUM_FRAMES; frame_idx++) {
                    // Get frame from sequence (most recent to oldest)
                    const auto& frame = sequence.frames[NUM_FRAMES - 1 - frame_idx];

                    // Get eye images (reuse vectors to avoid allocations)
                    static thread_local std::vector<uint32_t> left_eye_data;
                    static thread_local std::vector<uint32_t> right_eye_data;
                    int left_width, left_height, right_width, right_height;

                    // Decode eye images (uses existing caching)
                    frame.DecodeImageLeft(left_eye_data, left_width, left_height);
                    frame.DecodeImageRight(right_eye_data, right_width, right_height);

                    // Apply histogram equalization (matching trainerte2.py preprocessing)
                    // Convert to OpenCV format for histogram equalization
                    cv::Mat left_mat(left_height, left_width, CV_8UC4, left_eye_data.data());
                    cv::Mat right_mat(right_height, right_width, CV_8UC4, right_eye_data.data());
                    cv::Mat left_gray, right_gray;
                    cv::cvtColor(left_mat, left_gray, cv::COLOR_BGRA2GRAY);
                    cv::cvtColor(right_mat, right_gray, cv::COLOR_BGRA2GRAY);
                    cv::equalizeHist(left_gray, left_gray);
                    cv::equalizeHist(right_gray, right_gray);

                    // Convert back to uint32_t format using direct pointer access
                    uint8_t* left_ptr = left_gray.ptr<uint8_t>();
                    uint8_t* right_ptr = right_gray.ptr<uint8_t>();

                    for (int i = 0; i < left_width * left_height; i++) {
                        uint8_t gray_val = left_ptr[i];
                        left_eye_data[i] = gray_val | (gray_val << 8) | (gray_val << 16) | (0xFF << 24);
                    }
                    for (int i = 0; i < right_width * right_height; i++) {
                        uint8_t gray_val = right_ptr[i];
                        right_eye_data[i] = gray_val | (gray_val << 8) | (gray_val << 16) | (0xFF << 24);
                    }

                    // Calculate offsets in the batch tensor
                    size_t frame_offset = i * 2 * NUM_FRAMES * TRAIN_RESOLUTION * TRAIN_RESOLUTION + frame_idx * 2 * TRAIN_RESOLUTION * TRAIN_RESOLUTION;

                    // Process left eye with optimized scaling
                    const float x_scale = (float)left_width / TRAIN_RESOLUTION;
                    const float y_scale = (float)left_height / TRAIN_RESOLUTION;

                    for (int y = 0; y < TRAIN_RESOLUTION; y++) {
                        const int src_y = std::max(0, std::min((int)(y * y_scale), left_height - 1));
                        const int src_row_offset = src_y * left_width;
                        const size_t target_row_offset = frame_offset + y * TRAIN_RESOLUTION;

                        for (int x = 0; x < TRAIN_RESOLUTION; x++) {
                            const int src_x = std::max(0, std::min((int)(x * x_scale), left_width - 1));
                            const uint32_t pixel = left_eye_data[src_row_offset + src_x];
                            batch_images[target_row_offset + x] = (pixel & 0xFF) * (1.0f / 255.0f);
                        }
                    }

                    // Process right eye with optimized scaling (offset by TRAIN_RESOLUTION * TRAIN_RESOLUTION)
                    const float x_scale_r = (float)right_width / TRAIN_RESOLUTION;
                    const float y_scale_r = (float)right_height / TRAIN_RESOLUTION;
                    const size_t right_eye_offset = frame_offset + TRAIN_RESOLUTION * TRAIN_RESOLUTION;

                    for (int y = 0; y < TRAIN_RESOLUTION; y++) {
                        const int src_y = std::max(0, std::min((int)(y * y_scale_r), right_height - 1));
                        const int src_row_offset = src_y * right_width;
                        const size_t target_row_offset = right_eye_offset + y * TRAIN_RESOLUTION;

                        for (int x = 0; x < TRAIN_RESOLUTION; x++) {
                            const int src_x = std::max(0, std::min((int)(x * x_scale_r), right_width - 1));
                            const uint32_t pixel = right_eye_data[src_row_offset + src_x];
                            batch_images[target_row_offset + x] = (pixel & 0xFF) * (1.0f / 255.0f);
                        }
                    }
                }
            }

            // DEBUG: Print tensor shapes before creation
            // printf("Creating tensors - batch_size: %zu, input_shape: [%lld, %lld, %lld, %lld]\n",
            //       current_batch_size, (int64_t)current_batch_size, (int64_t)(2 * NUM_FRAMES),
            //       (int64_t)TRAIN_RESOLUTION, (int64_t)TRAIN_RESOLUTION);

            // Create input tensor for images
            const int64_t input_shape[] = { (int64_t)current_batch_size, 2 * NUM_FRAMES, TRAIN_RESOLUTION, TRAIN_RESOLUTION };
            OrtValue* input_tensor = NULL;

            status = g_ort_api->CreateTensorWithDataAsOrtValue(
                memory_info,
                batch_images.data(),
                batch_images.size() * sizeof(float),
                input_shape,
                4,
                ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
                &input_tensor);

            if (status != NULL) {
                const char* error_message = g_ort_api->GetErrorMessage(status);
                fprintf(stderr, "Error creating input tensor: %s\n", error_message);
                g_ort_api->ReleaseStatus(status);
                continue; // Skip this batch
            }

            // printf("Input tensor created successfully\n");

            // Create label tensor
            const int64_t label_shape[] = { (int64_t)current_batch_size, NUM_CLASSES };
            OrtValue* label_tensor = NULL;
            // printf("Creating label tensor with shape: [%lld, %lld]\n", label_shape[0], label_shape[1]);

            status = g_ort_api->CreateTensorWithDataAsOrtValue(
                memory_info,
                batch_labels.data(),
                batch_labels.size() * sizeof(float),
                label_shape,
                2,
                ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
                &label_tensor);

            if (status != NULL) {
                const char* error_message = g_ort_api->GetErrorMessage(status);
                fprintf(stderr, "Error creating label tensor: %s\n", error_message);
                g_ort_api->ReleaseStatus(status);
                g_ort_api->ReleaseValue(input_tensor);
                continue; // Skip this batch
            }

            // printf("Label tensor created successfully\n");

            // Setup inputs and outputs for training step
            OrtValue* input_values[] = { input_tensor, label_tensor };
            OrtValue* output_values[1] = { NULL };

            // printf("About to call TrainStep for batch %zu...\n", batch_count);
            // fflush(stdout);

            // Run training step
            status = g_ort_training_api->TrainStep(
                training_session,
                NULL,
                2,
                input_values,
                1,
                output_values);

            if (status != NULL) {
                const char* error_message = g_ort_api->GetErrorMessage(status);
                fprintf(stderr, "Error in training step: %s\n", error_message);
                g_ort_api->ReleaseStatus(status);
                g_ort_api->ReleaseValue(input_tensor);
                g_ort_api->ReleaseValue(label_tensor);
                continue; // Skip this batch
            }

            // printf("TrainStep completed successfully for batch %zu\n", batch_count);

            // Get loss value
            float batch_loss = 0.0f;
            if (output_values[0] != NULL) {
                float* loss_data = NULL;
                status = g_ort_api->GetTensorMutableData(output_values[0], (void**)&loss_data);
                if (status == NULL) {
                    batch_loss = loss_data[0];
                    epoch_loss_sum += batch_loss;

                    // Print batch progress
                    printf("\rBatch %zu/%zu, Loss: %.6f",
                           batch_count + 1,
                           (sequences.size() + batch_size - 1) / batch_size,
                           batch_loss);
                    fflush(stdout);
                } else {
                    const char* error_message = g_ort_api->GetErrorMessage(status);
                    fprintf(stderr, "Error getting loss data: %s\n", error_message);
                    g_ort_api->ReleaseStatus(status);
                }
            }

            // Run optimizer step - CRITICAL for weight updates
            status = g_ort_training_api->OptimizerStep(training_session, NULL);
            if (status != NULL) {
                const char* error_message = g_ort_api->GetErrorMessage(status);
                fprintf(stderr, "\nError in optimizer step: %s\n", error_message);
                g_ort_api->ReleaseStatus(status);
            }

            // Reset gradients AFTER optimizer step
            status = g_ort_training_api->LazyResetGrad(training_session);
            if (status != NULL) {
                const char* error_message = g_ort_api->GetErrorMessage(status);
                fprintf(stderr, "\nError resetting gradients: %s\n", error_message);
                g_ort_api->ReleaseStatus(status);
            }

            // Check parameter changes periodically
            if (batch_count % check_interval == 0) {
                printf("\n"); // Add line break after batch progress
                printParameterInfo(training_session, g_ort_api, g_ort_training_api, &previous_params);
            }

            // Clean up batch resources
            if (output_values[0] != NULL) {
                g_ort_api->ReleaseValue(output_values[0]);
            }
            g_ort_api->ReleaseValue(input_tensor);
            g_ort_api->ReleaseValue(label_tensor);

            batch_count++;
        }

        // Print epoch summary
        auto epoch_end_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> epoch_duration = epoch_end_time - epoch_start_time;

        float epoch_avg_loss = epoch_loss_sum / batch_count;
        printf("\nEpoch %d/%d completed in %.2fs. Average loss: %.6f\n",
               epoch + 1, num_epochs, epoch_duration.count(), epoch_avg_loss);

        // Check if this is the best loss so far
        if (epoch_avg_loss < best_loss) {
            best_loss = epoch_avg_loss;
            printf("New best loss achieved!\n");

            // Save best model checkpoint
            std::string best_checkpoint_path = "onnx_artifacts/training/checkpoint_best";
            status = g_ort_training_api->SaveCheckpoint(
                checkpoint_state,
                to_wstring(best_checkpoint_path).c_str(),
                true // Include optimizer state
            );

            if (status != NULL) {
                const char* error_message = g_ort_api->GetErrorMessage(status);
                fprintf(stderr, "Error saving best checkpoint: %s\n", error_message);
                g_ort_api->ReleaseStatus(status);
            } else {
                printf("Best checkpoint saved to %s\n", best_checkpoint_path.c_str());
            }
        }

        // Save checkpoint periodically
        if ((epoch + 1) % save_interval == 0 || epoch == num_epochs - 1) {
            std::string checkpoint_save_path = "onnx_artifacts/training/checkpoint_epoch" + std::to_string(epoch + 1);
            status = g_ort_training_api->SaveCheckpoint(
                checkpoint_state,
                to_wstring(checkpoint_save_path).c_str(),
                true // Include optimizer state
            );

            if (status != NULL) {
                const char* error_message = g_ort_api->GetErrorMessage(status);
                fprintf(stderr, "Error saving checkpoint: %s\n", error_message);
                g_ort_api->ReleaseStatus(status);
            } else {
                printf("Checkpoint saved to %s\n", checkpoint_save_path.c_str());
            }
        }
    }

    // Print final parameter info
    printf("\nFinal parameter information:\n");
    printParameterInfo(training_session, g_ort_api, g_ort_training_api, &previous_params);

    // Calculate total training time
    auto training_end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> total_training_time = training_end_time - training_start_time;
    printf("Total training time: %.2f seconds\n", total_training_time.count());

    // Export the model
    std::wstring wide_onnx_path = to_wstring(onnx_model_path);

    // Define the output names for your inference model
    const char* output_names[] = { "output" };

    // Export the model
    status = g_ort_training_api->ExportModelForInferencing(
        training_session,
        wide_onnx_path.c_str(),
        1, // Number of outputs
        output_names);

    if (status != NULL) {
        const char* error_message = g_ort_api->GetErrorMessage(status);
        fprintf(stderr, "Error exporting model to ONNX: %s\n", error_message);
        g_ort_api->ReleaseStatus(status);
    } else {
        printf("Model successfully exported to ONNX at: %s\n", onnx_model_path.c_str());
    }

    // Clean up resources
    g_ort_api->ReleaseMemoryInfo(memory_info);
    g_ort_training_api->ReleaseTrainingSession(training_session);
    g_ort_training_api->ReleaseCheckpointState(checkpoint_state);
    g_ort_api->ReleaseSessionOptions(session_options);
    g_ort_api->ReleaseEnv(env);

    printf("Training completed successfully!\n");
    return 0;
}