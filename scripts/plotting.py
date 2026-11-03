import matplotlib.pyplot as plt

# Function to read data from file
def read_data(filename):
    x_vals, y_vals = [], []
    with open(filename, 'r') as file:
        for line in file:
            x, y = map(float, line.strip().split())
            x_vals.append(x)
            y_vals.append(y)
    return x_vals, y_vals

# Read data from the two files
x1, y1 = read_data('plot1.txt')
x2, y2 = read_data('plot2.txt')

# Plot the data
plt.figure(figsize=(10, 6))
plt.plot(x1, y1, label='Plot 1 (Optimized Bellman-Ford)', marker='o')
plt.plot(x2, y2, label='Plot 2 (Regular Bellman-Ford)', marker='x')

# Customize the plot
plt.xlabel('Graph Size (V + E)')
plt.ylabel('Computation Time (seconds)')
plt.title('Computation Time vs Graph Size')
plt.legend()
plt.grid(True)

# Show the plot
plt.show()
