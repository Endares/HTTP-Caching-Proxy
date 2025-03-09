# Use an official GCC image
FROM ubuntu:22.04

# Set the working directory
WORKDIR /app

# Copy the current directory contents into the container
COPY . .
RUN mkdir -p ./logs
RUN  mkdir -p /var/log/erss
RUN  chmod -R 777 /var/log/erss
# Install dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    build-essential \
    libboost-all-dev && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Build the application
RUN make

# Run the proxy server
CMD ["./proxy"]