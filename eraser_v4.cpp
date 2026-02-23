#include <iostream>
#include <fstream>
#include <chrono>
#include <cstring>
#include <csignal>
#include <thread>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

volatile bool keep_running = true;

// Obsługa sygnału Ctrl+C
void handle_signal(int signal) {
    if (signal == SIGINT) {
        keep_running = false;
        std::cout << "\nProcess interrupted by user. Exiting..." << std::endl;
    }
}

// Pobieranie modelu i producenta dysku
std::string get_disk_info(const std::string& device) {
    std::string model, vendor;
    std::ifstream vendor_file("/sys/block/" + device.substr(5) + "/device/vendor");
    std::ifstream model_file("/sys/block/" + device.substr(5) + "/device/model");

    if (vendor_file) {
        std::getline(vendor_file, vendor);
    }
    if (model_file) {
        std::getline(model_file, model);
    }

    return vendor + " " + model;
}

// Pobieranie rodzaju dysku (SATA, NVMe, USB)
std::string get_disk_type(const std::string& device) {
    if (device.find("nvme") != std::string::npos) return "NVMe";
    if (device.find("sd") != std::string::npos) return "SATA/USB";
    return "Unknown";
}

// Odliczanie przed startem
void countdown() {
    for (int i = 5; i > 0; --i) {
        if (!keep_running) return;
        std::cout << "\rStarting in " << i << "... ";
        std::cout.flush();
        sleep(1);
    }
    std::cout << "Start!" << std::endl;
}

// Wyświetlanie progresu
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
    size_t erase_size = std::stoul(argv[2]) * 1024 * 1024; // MB -> bytes
    size_t skip_size = std::stoul(argv[3]) * 1024 * 1024;  // MB -> bytes

    signal(SIGINT, handle_signal); // Obsługa Ctrl+C

    int fd = open(device, O_WRONLY);
    if (fd < 0) {
        perror("Error opening device");
        return 1;
    }

    // Pobranie rozmiaru dysku
    unsigned long long device_size = 0;
    if (ioctl(fd, BLKGETSIZE64, &device_size) < 0) {
        perror("Error getting device size");
        close(fd);
        return 1;
    }

    // Pobranie informacji o dysku
    std::string disk_info = get_disk_info(device);
    std::string disk_type = get_disk_type(device);

    // Wyświetlenie informacji
    std::cout << "Device: " << device << std::endl;
    std::cout << "Size: " << device_size / (1024 * 1024) << " MB" << std::endl;
    std::cout << "Type: " << disk_type << std::endl;
    std::cout << "Model: " << disk_info << std::endl;

    // Odliczanie przed startem
    countdown();
    if (!keep_running) {
        close(fd);
        return 0;
    }

    // Bufor do kasowania
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

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> total_time = end_time - start_time;
    double avg_speed = (total_erased / (1024.0 * 1024.0)) / total_time.count();

    if (keep_running) {
        std::cout << "\nErasure completed successfully." << std::endl;
    } else {
        std::cout << "\nErasure interrupted by user." << std::endl;
    }

    std::cout << "Total time: " << total_time.count() << " seconds" << std::endl;
    std::cout << "Average speed: " << avg_speed << " MB/s" << std::endl;

    delete[] buffer;
    close(fd);
    return 0;
}

