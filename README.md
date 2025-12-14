# Sobel filter with multiple processor

Applying sobel filter with multiple processors

**To run:**
mpicc sobel-paralelo.c -o paralelo -lm
mpirun -np [number of processors] ./paralelo
