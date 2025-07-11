FROM ubuntu:24.10

# Install build-essential for make, gcc, g++
RUN apt-get update && \
    apt-get install -y build-essential && \
    apt-get clean

WORKDIR /APL2

# Copy the entire APL2 directory (including all ejercicioN)
COPY . .

# Build all ejercicios by running 'make' in each subdirectory
RUN for dir in ejercicio*; do \
      echo "Building $dir"; \
      if [ -f "$dir/Makefile" ]; then \
        make -C "$dir" VERBOSE=1 || (cat "$dir/Makefile" && false); \
        echo "Contents of $dir:"; \
        ls -l "$dir"; \
      fi; \
    done

# Default to an interactive shell, or you can customize the entrypoint as needed
CMD ["/bin/bash"]
