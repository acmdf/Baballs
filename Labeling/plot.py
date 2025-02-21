import pandas as pd
import matplotlib.pyplot as plt

# Load CSV file
csv_file = "ndc_data.csv"  # Replace with your actual CSV file name
df = pd.read_csv(csv_file)

# Extract time for X-axis (normalize time to start from 0)
time = df["timestamp"] - df["timestamp"].iloc[0]

# Create a figure with 3 subplots for X, Y, and Z values
fig, axes = plt.subplots(3, 1, figsize=(10, 8), sharex=True)

# Function to plot with Y-axis limited from -1 to 1
def plot_ndc(axis, labels, title, ylabel):
    axis.set_title(title)
    for label in labels:
        axis.plot(time, df[label], label=label)
    axis.set_ylabel(ylabel)
    axis.set_ylim(-1, 1)  # Set Y-axis limits
    axis.legend()

# Plot X, Y, and Z coordinates with -1 to 1 limits
plot_ndc(axes[0], ["left_ndc_x", "right_ndc_x", "main_ndc_x"], "NDC X Coordinates Over Time", "X Coordinate")
plot_ndc(axes[1], ["left_ndc_y", "right_ndc_y", "main_ndc_y"], "NDC Y Coordinates Over Time", "Y Coordinate")
plot_ndc(axes[2], ["left_ndc_z", "right_ndc_z", "main_ndc_z"], "NDC Z Coordinates Over Time", "Z Coordinate")

axes[2].set_xlabel("Time (seconds)")  # Label only the last plot for clarity

plt.tight_layout()
plt.show()
