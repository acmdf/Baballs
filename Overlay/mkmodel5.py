import os
import torch
import torch.nn as nn
import torch.nn.functional as F
import onnx
from onnxruntime.training import artifacts
import numpy as np

class BaballsModel(nn.Module):
    def __init__(self):
        super(BaballsModel, self).__init__()

        # Convolutional layers with batch normalization
        self.conv1 = nn.Conv2d(2, 16, 5, 2)
        #self.bn1 = nn.BatchNorm2d(16)
        
        self.conv2 = nn.Conv2d(16, 32, 3, 2)
        #self.bn2 = nn.BatchNorm2d(32)
        
        self.conv3 = nn.Conv2d(32, 64, 3, 2)
        #self.bn3 = nn.BatchNorm2d(64)
        
        self.conv4 = nn.Conv2d(64, 128, 3, 2)
        #self.bn4 = nn.BatchNorm2d(128)

        # Residual block with batch normalization
        self.conv_rn1 = nn.Conv2d(128, 128, 3, 1, padding=1)
        #self.bn_rn1 = nn.BatchNorm2d(128)
        
        self.conv_rn2 = nn.Conv2d(128, 128, 3, 1, padding=1)
        #self.bn_rn2 = nn.BatchNorm2d(128)

        # Fully connected layers with batch normalization
        self.fc1 = nn.Linear(4608, 128)
        #self.bn_fc1 = nn.BatchNorm1d(128)

        # Output layer for 7 predictions
        self.fc2 = nn.Linear(128, 7)
        
        self.flatten = nn.Flatten()
        self.dummy_scale = nn.Parameter(torch.ones(1), requires_grad=True)

    def forward(self, x):
        # Convolutional layers with batchnorm and ReLU
        x = F.relu(self.conv1(x))
        x = F.relu(self.conv2(x))
        x = F.relu(self.conv3(x))
        x = F.relu(self.conv4(x))

        # Residual block with batchnorm
        _y = x
        x = F.relu(self.conv_rn1(x))
        x = self.conv_rn2(x)
        x = F.relu(_y + x)  # Residual connection after batchnorm

        x = self.flatten(x)
        
        # Fully connected layer with batchnorm
        x = F.relu(self.fc1(x))
        
        # Output layer
        x = self.fc2(x)
        x = F.tanh(x) * self.dummy_scale
        
        return x

def count_parameters(model):
    """Count the number of trainable parameters in the model"""
    return sum(p.numel() for p in model.parameters() if p.requires_grad)

def export_model_with_training_artifacts():
    # Create model instance
    model = BaballsModel()
    model.train()  # Set to training mode for export (CRITICAL!)
    
    # Count and print trainable parameters in PyTorch model
    total_params = count_parameters(model)
    print(f"Total trainable parameters in PyTorch model: {total_params:,}")
    
    # Create a dummy input tensor (batch_size=8, channels=2, height=128, width=128)
    dummy_input = torch.randn(8, 2, 128, 128)

    # Verify model works
    out = model(dummy_input).float()
    print(f"Model output shape: {out.shape}")
    
    # Create output directories
    os.makedirs("onnx_artifacts", exist_ok=True)
    os.makedirs("onnx_artifacts/training", exist_ok=True)
    
    onnx_model_path = "onnx_artifacts/baball_model.onnx"
    artifacts_output_dir = "onnx_artifacts/training"
    
    # Export the model to ONNX format with training mode enabled
    torch.onnx.export(
        model,                      # PyTorch model
        dummy_input,                # Dummy input
        onnx_model_path,            # Output path
        export_params=True,         # Store the trained weights
        opset_version=15,           # ONNX version
        do_constant_folding=False,  # Don't fold constants for training
        input_names=['input'],      # Input names
        output_names=['output'],    # Output names
        dynamic_axes={              # Variable length axes
            'input': {0: 'batch_size'},
            'output': {0: 'batch_size'}
        },
        training=torch.onnx.TrainingMode.TRAINING  # CRITICAL for training
    )
    
    print(f"ONNX model exported to {onnx_model_path}")
    
    # Load the exported ONNX model
    onnx_model = onnx.load(onnx_model_path)
    
    # Identify parameters that need gradients
    requires_grad_params = []
    frozen_params = []
    trainable_param_count = 0
    
    # Make all parameters trainable except those with "dummy" in the name
    for param in onnx_model.graph.initializer:
        if "dummy" in param.name:
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
            "lr": 0.001,          # Learning rate
            "betas": [0.9, 0.999],
            "eps": 1e-8,
            "weight_decay": 0.01  # L2 regularization
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