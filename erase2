#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

volatile bool keep_running = true;

void handle_signal(int signal) {
    if (signal == SIGINT) {
        keep_running = false;
        std::cout << "\nProcess interrupted by user. Cleaning up..." << std::endl;
    }
}

void print_progress(double percentage, double speed) {
    std::cout << "\rProgress: " << percentage << "% | Speed: " << speed << " MB/s";
    std::cout.flush();
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <device> <erase_size_MB> <skip_size_MB>" << std::endl;
        return 1;
    }

    const char* device = argv[1];
    size_t erase_size = std::stoul(argv[2]) * 1024 * 1024; // Convert MB to bytes
    size_t skip_size = std::stoul(argv[3]) * 1024 * 1024; // Convert MB to bytes

    signal(SIGINT, handle_signal); // Register signal handler

    int fd = open(device, O_WRONLY);
    if (fd < 0) {
        perror("Error opening device");
        return 1;
    }

    // Get device size
    unsigned long long device_size = 0;
    if (ioctl(fd, BLKGETSIZE64, &device_size) < 0) {
        perror("Error getting device size");
        close(fd);
        return 1;
    }

    std::cout << "Device size: " << device_size / (1024 * 1024) << " MB" << std::endl;

    char* buffer = new char[erase_size];
    memset(buffer, 0, erase_size);

    size_t total_erased = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (unsigned long long offset = 0; offset < device_size && keep_running; offset += (erase_size + skip_size)) {
        if (lseek(fd, offset, SEEK_SET) < 0) {
            perror("Error seeking device");
            delete[] buffer;
            close(fd);
            return 1;
        }

        if (write(fd, buffer, erase_size) < 0) {
            perror("Error writing to device");
            delete[] buffer;
            close(fd);
            return 1;
        }

        total_erased += erase_size;

        auto current_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = current_time - start_time;
        double percentage = (double)total_erased / device_size * 100.0;
        double speed = (total_erased / (1024.0 * 1024.0)) / elapsed.count();

        print_progress(percentage, speed);
    }

    if (keep_running) {
        std::cout << "\nErasure completed successfully." << std::endl;
    } else {
        std::cout << "\nErasure interrupted by user." << std::endl;
    }

    delete[] buffer;
    close(fd);
    return 0;
}
