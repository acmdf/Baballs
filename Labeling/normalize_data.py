import csv

def normalize_value(value, min_val, max_val):
    """Normalize a value to [0, 1] given min and max values.
    Returns 0.5 if no variation is present."""
    if max_val - min_val == 0:
        return 0.5
    return (value - min_val) / (max_val - min_val)

def main(input_csv='ndc_data.csv', output_csv='ndc_data_normalized.csv'):
    rows = []
    
    # Read data from the CSV file
    with open(input_csv, newline='') as infile:
        reader = csv.DictReader(infile)
        for row in reader:
            try:
                # Convert NDC values to float for calculations
                row['ndc_x'] = float(row['ndc_x'])
                row['ndc_y'] = float(row['ndc_y'])
                row['ndc_z'] = float(row['ndc_z'])
                rows.append(row)
            except ValueError as e:
                print(f"Skipping a row due to conversion error: {e}")
    
    if not rows:
        print("No data found in CSV.")
        return

    # Determine min and max for each NDC component
    min_x = min(row['ndc_x'] for row in rows)
    max_x = max(row['ndc_x'] for row in rows)
    min_y = min(row['ndc_y'] for row in rows)
    max_y = max(row['ndc_y'] for row in rows)
    min_z = min(row['ndc_z'] for row in rows)
    max_z = max(row['ndc_z'] for row in rows)

    print("NDX X - min:", min_x, "max:", max_x)
    print("NDX Y - min:", min_y, "max:", max_y)
    print("NDX Z - min:", min_z, "max:", max_z)

    # Write normalized data to a new CSV file
    with open(output_csv, 'w', newline='') as outfile:
        fieldnames = rows[0].keys()  # Assuming all rows have the same keys (frame, timestamp, ndc_x, ndc_y, ndc_z)
        writer = csv.DictWriter(outfile, fieldnames=fieldnames)
        writer.writeheader()
        
        for row in rows:
            row['ndc_x'] = normalize_value(row['ndc_x'], min_x, max_x)
            row['ndc_y'] = normalize_value(row['ndc_y'], min_y, max_y)
            row['ndc_z'] = normalize_value(row['ndc_z'], min_z, max_z)
            writer.writerow(row)
    
    print(f"Normalized data saved to {output_csv}")

if __name__ == '__main__':
    main()
