import os
import torch
import torch.nn as nn
import torch.nn.functional as F
import onnx
from onnxruntime.training import artifacts
import numpy as np

# Temporal Eye Tracking Model (from pytraintesttemporal.py)
class EyeTrackingModel(nn.Module):
    def __init__(self, dropout_p=0.2):
        super(EyeTrackingModel, self).__init__()

        # Feature extraction layers - for 8 channels (current frame + 3 previous frames)
        self.conv1 = nn.Conv2d(8, 32, kernel_size=5, stride=2, padding=2)  # 64x64
        self.bn1 = nn.BatchNorm2d(32)
        
        self.conv2 = nn.Conv2d(32, 64, kernel_size=3, stride=2, padding=1)  # 32x32
        self.bn2 = nn.BatchNorm2d(64)
        
        self.conv3 = nn.Conv2d(64, 128, kernel_size=3, stride=2, padding=1)  # 16x16
        self.bn3 = nn.BatchNorm2d(128)
        
        self.conv4 = nn.Conv2d(128, 256, kernel_size=3, stride=2, padding=1)  # 8x8
        self.bn4 = nn.BatchNorm2d(256)
        
        self.conv5 = nn.Conv2d(256, 512, kernel_size=3, stride=2, padding=1)  # 4x4
        self.bn5 = nn.BatchNorm2d(512)
        
        self.gap = nn.AdaptiveAvgPool2d(1)
        
        self.fc1 = nn.Linear(512, 256)
        self.dropout1 = nn.Dropout(dropout_p)
        
        self.fc2 = nn.Linear(256, 64)
        self.dropout2 = nn.Dropout(dropout_p)
        
        self.fc_blends = nn.Linear(64, 10)  # Output for eye tracking parameters (pitch, yaw, distance, leftLid, rightLid, browRaise, browAngry, widen, squint, dilate) - excluding fovAdjustDistance

    def forward(self, x, return_blends=True, return_embs=False):
        # Feature extraction
        x = F.relu(self.conv1(x))
        x = self.dropout1(x)
        x = F.relu(self.conv2(x))
        x = self.dropout1(x)
        x = F.relu(self.conv3(x))
        x = self.dropout1(x)
        x = F.relu(self.conv4(x))
        x = self.dropout1(x)
        x = F.relu(self.conv5(x))
        x = self.dropout1(x)
        
        # Global average pooling
        x = self.gap(x)
        x = x.view(x.size(0), -1)
        
        # Fully connected layers
        x = F.relu(self.fc1(x))
        x = self.dropout1(x)
        
        x = F.relu(self.fc2(x))
        x = self.dropout2(x)
        
        if return_embs:
            return x
        else:
            # Output layer (pitch, yaw, distance, leftLid, rightLid, browRaise, browAngry, widen, squint, dilate) - excluding fovAdjustDistance
            eye_tracking_params = self.fc_blends(x)
            return eye_tracking_params

def count_parameters(model):
    """Count the number of trainable parameters in the model"""
    return sum(p.numel() for p in model.parameters() if p.requires_grad)

def freeze_batchnorm(model):
    """Freeze BatchNorm layers to avoid training issues"""
    for module in model.modules():
        if isinstance(module, nn.BatchNorm2d):
            module.eval()  # Set to eval mode
            module.weight.requires_grad = False
            module.bias.requires_grad = False
            module.running_mean.requires_grad = False
            module.running_var.requires_grad = False
            if hasattr(module, 'num_batches_tracked'):
                module.num_batches_tracked.requires_grad = False

def export_model_with_training_artifacts():
    # Configuration
    num_frames = 4  # Current frame + 3 previous frames
    input_resolution = 128
    batch_size = 16
    
    # Create model instance
    model = EyeTrackingModel(dropout_p=0.0)  # Set dropout to 0 for inference
    
    # Set model to eval mode to freeze BatchNorm statistics
    model.eval()
    
    # Freeze BatchNorm layers
    freeze_batchnorm(model)
    
    # Count and print trainable parameters in PyTorch model
    total_params = count_parameters(model)
    print(f"Total trainable parameters in PyTorch model: {total_params:,}")
    
    # Create a dummy input tensor (8 channels - 2 channels per frame * 4 frames)
    dummy_input = torch.randn(batch_size, 8, input_resolution, input_resolution)

    # Verify model works
    out = model(dummy_input)
    print(f"Model output shape: {out.shape}")
    
    # Create output directories
    os.makedirs("onnx_artifacts", exist_ok=True)
    os.makedirs("onnx_artifacts/training", exist_ok=True)
    
    onnx_model_path = "onnx_artifacts/temporal_eye_tracking_model.onnx"
    artifacts_output_dir = "onnx_artifacts/training"
    
    # Export the model to ONNX format with training mode DISABLED
    torch.onnx.export(
        model,                      # PyTorch model
        dummy_input,                # Dummy input
        onnx_model_path,            # Output path
        export_params=True,         # Store the trained weights
        opset_version=15,           # ONNX version
        do_constant_folding=False,  # Don't fold constants
        input_names=['input'],      # Input names
        output_names=['output'],    # Output names
        dynamic_axes={              # Variable length axes
            'input': {0: 'batch_size'},
            'output': {0: 'batch_size'}
        },
        training=torch.onnx.TrainingMode.EVAL  # Use EVAL mode
    )
    
    print(f"ONNX model exported to {onnx_model_path}")
    
    # Load the exported ONNX model
    onnx_model = onnx.load(onnx_model_path)
    
    # Identify parameters that need gradients
    requires_grad_params = []
    frozen_params = []
    trainable_param_count = 0
    
    # Make all parameters trainable except BatchNorm parameters
    for param in onnx_model.graph.initializer:
        # Skip BatchNorm parameters and running statistics
        if any(bn_param in param.name for bn_param in [
            'bn', 'running_mean', 'running_var', 'num_batches_tracked'
        ]):
            frozen_params.append(param.name)
        else:
            requires_grad_params.append(param.name)
            # Get the shape of the parameter tensor from the raw data
            tensor = onnx.numpy_helper.to_array(param)
            trainable_param_count += np.prod(tensor.shape)
    
    # Generate training artifacts with proper optimizer settings
    artifacts.generate_artifacts(
        onnx_model,
        requires_grad=requires_grad_params,
        frozen_params=frozen_params,
        loss=artifacts.LossType.MSELoss,  # Using MSE loss for regression
        optimizer=artifacts.OptimType.AdamW,
        optimizer_config={
            "lr": 1e-4,           # Match the learning rate from your Python trainer
            "betas": [0.9, 0.999],
            "eps": 1e-8,
            "weight_decay": 1e-4  # Match the weight decay from your Python trainer
        },
        artifact_directory=artifacts_output_dir,
        nominal_checkpoint=True
    )
    
    print(f"Training artifacts generated in {artifacts_output_dir}")
    print(f"Number of trainable parameters: {trainable_param_count:,}")
    print(f"Number of trainable parameter tensors: {len(requires_grad_params)}")
    print(f"Number of frozen parameter tensors: {len(frozen_params)}")
    
    # Verify that the training artifacts were created
    expected_files = [
        "training_model.onnx",
        "eval_model.onnx",
        "optimizer_model.onnx",
        "checkpoint"
    ]
    
    for file in expected_files:
        full_path = os.path.join(artifacts_output_dir, file)
        if os.path.exists(full_path):
            print(f"✓ {file} created successfully")
        else:
            print(f"✗ {file} was not created!")

if __name__ == "__main__":
    export_model_with_training_artifacts()
