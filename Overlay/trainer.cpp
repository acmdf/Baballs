// trainer.cpp
#include <onnxruntime_cxx_api.h>
#include <onnxruntime_training_c_api.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>
#include <cstring>
#include <random>
#include <algorithm>
#include <numeric>
#include <chrono>

#define STD_MIN(a, b) ((a) < (b) ? (a) : (b))
#define FLOAT_TO_INT_CONSTANT 1

// CaptureFrame structure
struct CaptureFrame {
    std::vector<uint32_t> image_data_left;
    std::vector<uint32_t> image_data_right;
    float routinePitch;
    float routineYaw;
    float routineDistance;
    float fovAdjustDistance;
    uint32_t timestampLow;
    uint32_t timestampHigh;
    uint32_t routineState;
};

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

// Function to load capture file - optimized version
std::vector<CaptureFrame> loadCaptureFile(const std::string& filename) {
    std::vector<CaptureFrame> frames;
    std::ifstream file(filename, std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return frames;
    }
    
    // Calculate sizes
    const size_t image_size = 128 * 128 * 4; // Size of one eye's image data in bytes
    const size_t metadata_size = 7 * 4; // Size of the 7 metadata fields (4 bytes each)
    const size_t frame_size = image_size * 2 + metadata_size;
    
    // Read file in chunks
    std::vector<char> buffer(frame_size);
    size_t frame_count = 0;
    
    while (file.read(buffer.data(), frame_size)) {
        CaptureFrame frame;
        
        // Extract image data for both eyes
        frame.image_data_left.resize(128 * 128);
        frame.image_data_right.resize(128 * 128);
        
        // Copy image data
        std::memcpy(frame.image_data_left.data(), buffer.data(), image_size);
        std::memcpy(frame.image_data_right.data(), buffer.data() + image_size, image_size);
        
        // Calculate offset to the metadata section
        const char* metadata_ptr = buffer.data() + (image_size * 2);
        
        // Copy all metadata fields at once
        std::memcpy(&frame.routinePitch, metadata_ptr, metadata_size);
        
        frames.push_back(frame);
        frame_count++;
        
        if (frame_count % 1000 == 0) {
            printf("Loaded %zu frames...\n", frame_count);
        }
    }
    
    printf("Processed %zu samples total!\n", frames.size());
    return frames;
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
    
    /*printf("===== Parameter Information =====\n");
    printf("Total parameters: %zu\n", all_params_size);
    printf("Trainable parameters: %zu (%.2f%%)\n", trainable_params_size, 
           (float)trainable_params_size / all_params_size * 100.0f);
    printf("Frozen parameters: %zu (%.2f%%)\n", non_trainable_params_size, 
           (float)non_trainable_params_size / all_params_size * 100.0f);*/
    
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
    const int64_t shape[] = {(int64_t)trainable_params_size};
    OrtValue* params_tensor = nullptr;
    
    status = g_ort_api->CreateTensorWithDataAsOrtValue(
        memory_info,
        current_params.data(),
        current_params.size() * sizeof(float),
        shape,
        1,
        ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
        &params_tensor
    );
    
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
        true  // Only trainable parameters
    );
    
    if (status != NULL) {
        const char* error_message = g_ort_api->GetErrorMessage(status);
        fprintf(stderr, "Error copying parameters to buffer: %s\n", error_message);
        g_ort_api->ReleaseStatus(status);
        g_ort_api->ReleaseValue(params_tensor);
        g_ort_api->ReleaseMemoryInfo(memory_info);
        return;
    }
    
    /*// Print sample of parameters
    printf("Parameter samples: ");
    for (size_t i = 0; i < 5 && i < trainable_params_size; i++) {
        printf("%g ", current_params[i]);
    }
    printf("...\n");*/
    
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
        
        printf("Gradient movement: %g\n", total_diff);
        
        // Update previous parameters
        *prev_params = current_params;
    } else if (prev_params != nullptr) {
        // First time storing parameters
        *prev_params = current_params;
    }
    
    // Clean up
    g_ort_api->ReleaseValue(params_tensor);
    g_ort_api->ReleaseMemoryInfo(memory_info);
    
    //printf("================================\n");
}

int main(int argc, char* argv[]) {
    // Default file paths
    std::string capture_file = "capture.bin";
    std::string onnx_model_path = "tuned_model.onnx";
    
    // Check if command line arguments are provided
    if (argc >= 2) {
        capture_file = argv[1];  // First argument is the capture file
    }
    
    if (argc >= 3) {
        onnx_model_path = argv[2];   // Second argument is the output file
    }
    
    // Load the capture file
    auto frames = loadCaptureFile(capture_file);
    
    if (frames.empty()) {
        fprintf(stderr, "No frames loaded from capture file\n");
        return 1;
    }
    
    // Initialize ONNX Runtime
    const OrtApi* g_ort_api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    const OrtTrainingApi* g_ort_training_api = g_ort_api->GetTrainingApi(ORT_API_VERSION);
    
    // Create environment
    OrtEnv* env = NULL;
    OrtStatus* status = g_ort_api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "BaballsTrainer", &env);
    if (status != NULL) {
        const char* error_message = g_ort_api->GetErrorMessage(status);
        fprintf(stderr, "Error creating environment: %s\n", error_message);
        g_ort_api->ReleaseStatus(status);
        return 1;
    }
    
    // Create session options
    OrtSessionOptions* session_options = NULL;
    status = g_ort_api->CreateSessionOptions(&session_options);
    if (status != NULL) {
        const char* error_message = g_ort_api->GetErrorMessage(status);
        fprintf(stderr, "Error creating session options: %s\n", error_message);
        g_ort_api->ReleaseStatus(status);
        g_ort_api->ReleaseEnv(env);
        return 1;
    }
    
    // Set session options
    g_ort_api->SetSessionGraphOptimizationLevel(session_options, ORT_DISABLE_ALL);
    g_ort_api->SetIntraOpNumThreads(session_options, 4);
    
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
    
    // Create training session
    OrtTrainingSession* training_session = NULL;
    status = g_ort_training_api->CreateTrainingSession(
        env,
        session_options,
        checkpoint_state,
        to_wstring(training_model_path).c_str(),
        to_wstring(eval_model_path).c_str(),
        to_wstring(optimizer_model_path).c_str(),
        &training_session
    );
    
    if (status != NULL) {
        const char* error_message = g_ort_api->GetErrorMessage(status);
        fprintf(stderr, "Error creating training session: %s\n", error_message);
        g_ort_api->ReleaseStatus(status);
        g_ort_training_api->ReleaseCheckpointState(checkpoint_state);
        g_ort_api->ReleaseSessionOptions(session_options);
        g_ort_api->ReleaseEnv(env);
        return 1;
    }
    
    // Vector to track parameter changes
    std::vector<float> previous_params;
    
    // Print initial parameter info
    printf("Initial parameter information:\n");
    printParameterInfo(training_session, g_ort_api, g_ort_training_api, &previous_params);
    
    // Set learning rate
    float learning_rate = 5e-5f;
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
    std::vector<size_t> indices(frames.size());
    std::iota(indices.begin(), indices.end(), 0);  // Fill with 0, 1, 2, ...
    
    // Training configuration
    const int num_epochs = 5;
    const size_t batch_size = 16;
    const size_t check_interval = 100;  // Check parameters every N batches
    const size_t save_interval = 1;    // Save checkpoint every N epochs
    
    printf("Starting training with %zu samples, %d epochs, batch size %zu\n", 
           frames.size(), num_epochs, batch_size);
    
    // Track overall stats
    float best_loss = std::numeric_limits<float>::max();
    
    // Training loop
    auto training_start_time = std::chrono::steady_clock::now();


    auto total_step_count = num_epochs * frames.size();
    auto warmup_step_count = total_step_count / 3;
    
    g_ort_training_api->RegisterLinearLRScheduler(training_session, warmup_step_count, total_step_count, 0.0001f);
    
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
        for (size_t batch_start = 0; batch_start < frames.size(); batch_start += batch_size) {
            // Determine actual batch size (may be smaller for the last batch)
            size_t current_batch_size = STD_MIN(batch_size, frames.size() - batch_start);
            
            // Prepare batch data - images and labels
            std::vector<float> batch_images(current_batch_size * 2 * 128 * 128);
            std::vector<float> batch_labels(current_batch_size * 7);
            
            // Fill batch with data
            for (size_t i = 0; i < current_batch_size; i++) {
                const auto& frame = frames[indices[batch_start + i]];
                
                // Process left eye image
                for (size_t j = 0; j < 128 * 128; j++) {
                    batch_images[i * 2 * 128 * 128 + j] = rgba_to_float(frame.image_data_left[j]);
                }
                
                // Process right eye image
                for (size_t j = 0; j < 128 * 128; j++) {
                    batch_images[i * 2 * 128 * 128 + 128 * 128 + j] = rgba_to_float(frame.image_data_right[j]);
                }
                
                // Process labels (normalized to [-1, 1])
                batch_labels[i * 7 + 0] = 2.0f * (frame.routinePitch / FLOAT_TO_INT_CONSTANT) / 360.0f - 1.0f;
                batch_labels[i * 7 + 1] = 2.0f * (frame.routineYaw / FLOAT_TO_INT_CONSTANT) / 360.0f - 1.0f;
                batch_labels[i * 7 + 2] = 2.0f * (frame.routineDistance / FLOAT_TO_INT_CONSTANT) / 360.0f - 1.0f;
                batch_labels[i * 7 + 3] = 0.0f;
                batch_labels[i * 7 + 4] = 0.0f;
                batch_labels[i * 7 + 5] = 0.0f;
                batch_labels[i * 7 + 6] = 0.0f;
            }
            
            // Create input tensor for images
            const int64_t input_shape[] = {(int64_t)current_batch_size, 2, 128, 128};
            OrtValue* input_tensor = NULL;
            
            status = g_ort_api->CreateTensorWithDataAsOrtValue(
                memory_info,
                batch_images.data(),
                batch_images.size() * sizeof(float),
                input_shape,
                4,
                ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
                &input_tensor
            );
            
            if (status != NULL) {
                const char* error_message = g_ort_api->GetErrorMessage(status);
                fprintf(stderr, "Error creating input tensor: %s\n", error_message);
                g_ort_api->ReleaseStatus(status);
                continue;  // Skip this batch
            }
            
            // Create label tensor
            const int64_t label_shape[] = {(int64_t)current_batch_size, 7};
            OrtValue* label_tensor = NULL;
            
            status = g_ort_api->CreateTensorWithDataAsOrtValue(
                memory_info,
                batch_labels.data(),
                batch_labels.size() * sizeof(float),
                label_shape,
                2,
                ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT,
                &label_tensor
            );
            
            if (status != NULL) {
                const char* error_message = g_ort_api->GetErrorMessage(status);
                fprintf(stderr, "Error creating label tensor: %s\n", error_message);
                g_ort_api->ReleaseStatus(status);
                g_ort_api->ReleaseValue(input_tensor);
                continue;  // Skip this batch
            }
            
            // Setup inputs and outputs for training step
            const char* input_names[] = {"input", "target"};
            OrtValue* input_values[] = {input_tensor, label_tensor};
            OrtValue* output_values[1] = {NULL};
            
            // Run training step
            status = g_ort_training_api->TrainStep(
                training_session,
                NULL,
                2,
                input_values,
                1,
                output_values
            );
            
            if (status != NULL) {
                const char* error_message = g_ort_api->GetErrorMessage(status);
                fprintf(stderr, "Error in training step: %s\n", error_message);
                g_ort_api->ReleaseStatus(status);
                g_ort_api->ReleaseValue(input_tensor);
                g_ort_api->ReleaseValue(label_tensor);
                continue;  // Skip this batch
            }
            
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
                           (frames.size() + batch_size - 1) / batch_size, 
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

            g_ort_training_api->SchedulerStep(training_session);

            // Reset gradients AFTER optimizer step
            status = g_ort_training_api->LazyResetGrad(training_session);
            if (status != NULL) {
                const char* error_message = g_ort_api->GetErrorMessage(status);
                fprintf(stderr, "\nError resetting gradients: %s\n", error_message);
                g_ort_api->ReleaseStatus(status);
            }
            
            // Check parameter changes periodically
            /*if (batch_count % check_interval == 0) {
                printf("\n");  // Add line break after batch progress
                printParameterInfo(training_session, g_ort_api, g_ort_training_api, &previous_params);
            }*/
            
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
        /*
        // Check if this is the best loss so far
        if (epoch_avg_loss < best_loss) {
            best_loss = epoch_avg_loss;
            printf("New best loss achieved!\n");
            
            // Save best model checkpoint
            std::string best_checkpoint_path = "onnx_artifacts/training/checkpoint_best";
            status = g_ort_training_api->SaveCheckpoint(
                checkpoint_state,
                to_wstring(best_checkpoint_path).c_str(),
                true  // Include optimizer state
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
                true  // Include optimizer state
            );
            
            if (status != NULL) {
                const char* error_message = g_ort_api->GetErrorMessage(status);
                fprintf(stderr, "Error saving checkpoint: %s\n", error_message);
                g_ort_api->ReleaseStatus(status);
            } else {
                printf("Checkpoint saved to %s\n", checkpoint_save_path.c_str());
            }
        }*/
        printParameterInfo(training_session, g_ort_api, g_ort_training_api, &previous_params);
    }
    
    // Print final parameter info
    //printf("\nFinal parameter information:\n");
    //printParameterInfo(training_session, g_ort_api, g_ort_training_api, &previous_params);
    
    // Calculate total training time
    auto training_end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> total_training_time = training_end_time - training_start_time;
    printf("Total training time: %.2f seconds\n", total_training_time.count());
    
    // Save final checkpoint
    /*printf("Saving final checkpoint...\n");
    status = g_ort_training_api->SaveCheckpoint(
        checkpoint_state,
        to_wstring("onnx_artifacts/training/checkpoint_final").c_str(),
        true  // Include optimizer state
    );
    
    if (status != NULL) {
        const char* error_message = g_ort_api->GetErrorMessage(status);
        fprintf(stderr, "Error saving final checkpoint: %s\n", error_message);
        g_ort_api->ReleaseStatus(status);
    } else {
        printf("Final checkpoint saved successfully\n");
    }*/

    std::wstring wide_onnx_path = to_wstring(onnx_model_path);

    // Define the output names for your inference model
    const char* output_names[] = {"output"}; 

    // Export the model
    status = g_ort_training_api->ExportModelForInferencing(
        training_session,
        wide_onnx_path.c_str(),
        1,  // Number of outputs
        output_names
    );

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