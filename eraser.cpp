#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <cctype>
#include <csignal>
#include <thread>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fs.h>

volatile bool keep_running = true;

enum ExitCode {
    EXIT_OK = 0,
    EXIT_INVALID_ARGUMENTS = 2,
    EXIT_OPEN_FAILED = 3,
    EXIT_SIZE_FAILED = 4,
    EXIT_SEEK_FAILED = 5,
    EXIT_WRITE_FAILED = 6,
    EXIT_USER_ABORTED = 7
};

struct VerificationStats {
    unsigned long long bytes_read = 0;
    unsigned long long zero_bytes = 0;
    unsigned long long non_zero_bytes = 0;
    unsigned long long first_non_zero_offset = std::numeric_limits<unsigned long long>::max();
    double duration_seconds = 0.0;
};

void print_verification_progress(double percentage, double speed_mb_s) {
    std::cout << "\r\033[KVerification: " << std::fixed << std::setprecision(2)
              << percentage << "% | Speed: " << speed_mb_s << " MB/s";
    std::cout.flush();
}

void print_verification_report(unsigned long long target_size, const VerificationStats& stats) {
    const double target_size_mb = target_size / (1024.0 * 1024.0);
    const double zero_mb = stats.zero_bytes / (1024.0 * 1024.0);
    const double non_zero_mb = stats.non_zero_bytes / (1024.0 * 1024.0);
    const double zero_percent = target_size > 0 ? (stats.zero_bytes * 100.0) / target_size : 0.0;
    const double non_zero_percent = target_size > 0 ? (stats.non_zero_bytes * 100.0) / target_size : 0.0;
    const double avg_verify_speed = stats.duration_seconds > 0.0
        ? (stats.bytes_read / (1024.0 * 1024.0)) / stats.duration_seconds
        : 0.0;

    std::cout << "\nVerification report:" << std::endl;
    std::cout << "  Total size: " << std::fixed << std::setprecision(2) << target_size_mb
              << " MB (" << target_size << " bytes)" << std::endl;
    std::cout << "  Free (00): " << zero_mb << " MB (" << stats.zero_bytes << " bytes, "
              << std::setprecision(4) << zero_percent << "%)" << std::endl;
    std::cout << "  Used (!00): " << std::setprecision(2) << non_zero_mb << " MB ("
              << stats.non_zero_bytes << " bytes, " << std::setprecision(4) << non_zero_percent << "%)" << std::endl;
    std::cout << "  Verification time: " << std::setprecision(4) << stats.duration_seconds << " s" << std::endl;
    std::cout << "  Average verification speed: " << std::setprecision(2) << avg_verify_speed << " MB/s" << std::endl;

    if (stats.first_non_zero_offset != std::numeric_limits<unsigned long long>::max()) {
        std::cout << "  First non-zero byte offset: " << stats.first_non_zero_offset << " bytes" << std::endl;
    } else {
        std::cout << "  First non-zero byte offset: not found (all bytes are 00)" << std::endl;
    }
}

bool verify_target_content(
    int fd,
    unsigned long long target_size,
    bool stop_on_first_non_zero,
    bool show_progress,
    VerificationStats& stats,
    std::string& error_message
) {
    constexpr size_t verify_chunk_size = 4 * 1024 * 1024;
    std::vector<unsigned char> read_buffer(verify_chunk_size, 0);
    stats = VerificationStats{};

    if (lseek(fd, 0, SEEK_SET) < 0) {
        error_message = "Could not seek to the beginning for verification.";
        return false;
    }

    auto verify_start = std::chrono::high_resolution_clock::now();
    unsigned long long offset = 0;
    while (offset < target_size && keep_running) {
        const size_t bytes_to_read = static_cast<size_t>(std::min<unsigned long long>(verify_chunk_size, target_size - offset));
        const ssize_t read_result = read(fd, read_buffer.data(), bytes_to_read);

        if (read_result < 0) {
            error_message = "Read error while verifying target content.";
            return false;
        }

        if (read_result == 0) {
            break;
        }

        unsigned long long chunk_non_zero = 0;
        for (ssize_t index = 0; index < read_result; ++index) {
            if (read_buffer[static_cast<size_t>(index)] != 0x00) {
                if (stats.first_non_zero_offset == std::numeric_limits<unsigned long long>::max()) {
                    stats.first_non_zero_offset = offset + static_cast<unsigned long long>(index);
                }
                ++chunk_non_zero;

                if (stop_on_first_non_zero) {
                    stats.non_zero_bytes += 1;
                    stats.bytes_read += static_cast<unsigned long long>(index + 1);
                    stats.zero_bytes = stats.bytes_read - stats.non_zero_bytes;
                    auto verify_end = std::chrono::high_resolution_clock::now();
                    std::chrono::duration<double> verify_duration = verify_end - verify_start;
                    stats.duration_seconds = verify_duration.count();
                    return true;
                }
            }
        }

        stats.bytes_read += static_cast<unsigned long long>(read_result);
        stats.non_zero_bytes += chunk_non_zero;
        stats.zero_bytes = stats.bytes_read - stats.non_zero_bytes;
        offset += static_cast<unsigned long long>(read_result);

        if (show_progress) {
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = now - verify_start;
            const double percentage = target_size > 0 ? (stats.bytes_read * 100.0) / target_size : 0.0;
            const double speed_mb_s = elapsed.count() > 0.0
                ? (stats.bytes_read / (1024.0 * 1024.0)) / elapsed.count()
                : 0.0;
            print_verification_progress(percentage, speed_mb_s);
        }
    }

    auto verify_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> verify_duration = verify_end - verify_start;
    stats.duration_seconds = verify_duration.count();
    return true;
}

bool ask_user_to_continue() {
    std::string answer;
    std::cout << "Detected non-zero bytes on target. Start erasing data? [y/N]: ";
    std::getline(std::cin, answer);

    if (!std::cin.good()) {
        return false;
    }

    std::transform(answer.begin(), answer.end(), answer.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });

    return answer == "y" || answer == "yes";
}

bool parse_size_mb(const char* raw_value, const char* field_name, bool allow_zero, size_t& out_bytes, std::string& error) {
    const std::string value = raw_value != nullptr ? raw_value : "";
    if (value.empty()) {
        error = std::string("Field '") + field_name + "' is empty.";
        return false;
    }

    if (value[0] == '-') {
        error = std::string("Field '") + field_name + "' cannot be negative.";
        return false;
    }

    size_t parsed_chars = 0;
    unsigned long long mb_value = 0;

    try {
        mb_value = std::stoull(value, &parsed_chars, 10);
    } catch (const std::invalid_argument&) {
        error = std::string("Field '") + field_name + "' must be an integer number in MB (example: 8).";
        return false;
    } catch (const std::out_of_range&) {
        error = std::string("Field '") + field_name + "' is too large.";
        return false;
    }

    if (parsed_chars != value.size()) {
        error = std::string("Field '") + field_name + "' contains invalid characters: '" + value + "'.";
        return false;
    }

    if (!allow_zero && mb_value == 0) {
        error = std::string("Field '") + field_name + "' must be greater than 0 MB.";
        return false;
    }

    const unsigned long long bytes_per_mb = 1024ULL * 1024ULL;
    if (mb_value > (std::numeric_limits<size_t>::max() / bytes_per_mb)) {
        error = std::string("Field '") + field_name + "' is too large for this system.";
        return false;
    }

    out_bytes = static_cast<size_t>(mb_value * bytes_per_mb);
    return true;
}

std::string format_bytes(unsigned long long bytes) {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    double value = static_cast<double>(bytes);
    size_t unit_index = 0;

    while (value >= 1024.0 && unit_index < (sizeof(units) / sizeof(units[0])) - 1) {
        value /= 1024.0;
        ++unit_index;
    }

    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value << " " << units[unit_index];
    return stream.str();
}

void print_help(const char* program_name) {
    std::cout << "Usage:\n";
    std::cout << "  " << program_name << " <device_or_file> <erase_size_MB> <skip_size_MB> [options]\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  <device_or_file>   Target block device (e.g. /dev/sdb) or file for simulation\n";
    std::cout << "  <erase_size_MB>    Size of zeroed chunk in MB (must be > 0)\n";
    std::cout << "  <skip_size_MB>     Size of skipped chunk in MB\n\n";
    std::cout << "Options:\n";
    std::cout << "  --simulate         Simulation mode (no writes are performed)\n";
    std::cout << "  --verify-zero      Scan target and verify bytes are 00 before erase\n";
    std::cout << "  --verify-only      Verify target and print usage report (no erase)\n";
    std::cout << "  -q, --quiet-errors Print one-line errors only (no full help on error)\n";
    std::cout << "  -h, --help         Show this help message\n\n";
    std::cout << "Exit codes:\n";
    std::cout << "  0  Success\n";
    std::cout << "  2  Invalid arguments or values\n";
    std::cout << "  3  Open target failed\n";
    std::cout << "  4  Could not determine target size\n";
    std::cout << "  5  Seek failed\n";
    std::cout << "  6  Write failed\n";
    std::cout << "  7  User aborted operation\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " /dev/sdb 8 0\n";
    std::cout << "  " << program_name << " /dev/sdb 8 8\n";
    std::cout << "  " << program_name << " /dev/sdb 8 0 --verify-zero\n";
    std::cout << "  " << program_name << " /dev/sdb 1 0 --verify-only\n";
    std::cout << "  " << program_name << " test.img 4 4 --simulate\n";
    std::cout << "  " << program_name << " test.img abc 4 --simulate --quiet-errors\n";
}

void print_error(const char* program_name, const std::string& message, bool quiet_errors) {
    std::cerr << "Error: " << message << std::endl;
    if (!quiet_errors) {
        print_help(program_name);
    }
}

// Obsługa sygnału Ctrl+C
void handle_signal(int signal) {
    if (signal == SIGINT) {
        keep_running = false;
        std::cout << "\nProcess interrupted by user. Exiting..." << std::endl;
    }
}

// Pobieranie modelu i producenta dysku
std::string get_disk_info(const std::string& device) {
    if (device.rfind("/dev/", 0) != 0) {
        return "N/A";
    }

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
void print_progress(double percentage, double speed, double erased_percentage) {
    std::cout << "\r\033[KProgress: " << std::fixed << std::setprecision(2) << percentage
              << "% | Erased: " << erased_percentage
              << "% | Speed: " << speed << " MB/s";
    std::cout.flush();
}

int main(int argc, char* argv[]) {
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        print_help(argv[0]);
        return EXIT_OK;
    }

    bool simulate_mode = false;
    bool quiet_errors = false;
    bool verify_zero_mode = false;
    bool verify_only_mode = false;

    if (argc < 4) {
        print_help(argv[0]);
        return EXIT_INVALID_ARGUMENTS;
    }

    for (int arg_index = 4; arg_index < argc; ++arg_index) {
        const std::string option = argv[arg_index];
        if (option == "--simulate") {
            simulate_mode = true;
        } else if (option == "--verify-zero") {
            verify_zero_mode = true;
        } else if (option == "--verify-only") {
            verify_only_mode = true;
        } else if (option == "--quiet-errors" || option == "-q") {
            quiet_errors = true;
        } else if (option == "--help" || option == "-h") {
            print_help(argv[0]);
            return EXIT_OK;
        } else {
            print_error(argv[0], std::string("Unknown option: ") + option, quiet_errors);
            return EXIT_INVALID_ARGUMENTS;
        }
    }

    const char* device = argv[1];
    size_t erase_size = 0;
    size_t skip_size = 0;
    std::string parse_error;

    if (!parse_size_mb(argv[2], "erase_size_MB", verify_only_mode, erase_size, parse_error)) {
        print_error(argv[0], parse_error, quiet_errors);
        return EXIT_INVALID_ARGUMENTS;
    }

    if (!parse_size_mb(argv[3], "skip_size_MB", true, skip_size, parse_error)) {
        print_error(argv[0], parse_error, quiet_errors);
        return EXIT_INVALID_ARGUMENTS;
    }

    if (simulate_mode && verify_zero_mode) {
        print_error(argv[0], "Option --verify-zero cannot be used together with --simulate.", quiet_errors);
        return EXIT_INVALID_ARGUMENTS;
    }

    if (simulate_mode && verify_only_mode) {
        print_error(argv[0], "Option --verify-only cannot be used together with --simulate.", quiet_errors);
        return EXIT_INVALID_ARGUMENTS;
    }

    if (verify_zero_mode && verify_only_mode) {
        print_error(argv[0], "Use either --verify-zero or --verify-only, not both.", quiet_errors);
        return EXIT_INVALID_ARGUMENTS;
    }

    signal(SIGINT, handle_signal); // Obsługa Ctrl+C

    int fd = open(device, (simulate_mode || verify_only_mode) ? O_RDONLY : O_RDWR);
    if (fd < 0) {
        perror("Error opening device");
        return EXIT_OPEN_FAILED;
    }

    // Pobranie rozmiaru dysku
    unsigned long long device_size = 0;
    if (ioctl(fd, BLKGETSIZE64, &device_size) < 0) {
        struct stat st {};
        if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode)) {
            device_size = static_cast<unsigned long long>(st.st_size);
        } else {
            perror("Error getting device size");
            close(fd);
            return EXIT_SIZE_FAILED;
        }
    }

    if (device_size == 0) {
        std::cerr << "Error: target size is 0 bytes." << std::endl;
        close(fd);
        return EXIT_SIZE_FAILED;
    }

    // Pobranie informacji o dysku
    std::string disk_info = get_disk_info(device);
    std::string disk_type = get_disk_type(device);

    // Wyświetlenie informacji
    std::cout << "Device: " << device << std::endl;
    std::cout << "Size: " << device_size / (1024 * 1024) << " MB" << std::endl;
    std::cout << "Type: " << disk_type << std::endl;
    std::cout << "Model: " << disk_info << std::endl;
    if (verify_only_mode) {
        std::cout << "Mode: VERIFY ONLY" << std::endl;
    } else {
        std::cout << "Mode: " << (simulate_mode ? "SIMULATION (no write)" : "ERASE") << std::endl;
    }

    if (verify_only_mode) {
        std::cout << "Verifying full target content and generating report..." << std::endl;
        VerificationStats verify_stats;
        std::string verify_error;

        if (!verify_target_content(fd, device_size, false, true, verify_stats, verify_error)) {
            print_error(argv[0], verify_error, quiet_errors);
            close(fd);
            return EXIT_SIZE_FAILED;
        }

        std::cout << std::endl;

        if (!keep_running) {
            std::cout << "Verification interrupted by user." << std::endl;
            close(fd);
            return EXIT_USER_ABORTED;
        }

        print_verification_report(device_size, verify_stats);
        close(fd);
        return EXIT_OK;
    }

    if (verify_zero_mode) {
        std::cout << "Verifying target content (expecting only 00 bytes)..." << std::endl;
        VerificationStats verify_stats;
        std::string verify_error;

        if (!verify_target_content(fd, device_size, true, false, verify_stats, verify_error)) {
            print_error(argv[0], verify_error, quiet_errors);
            close(fd);
            return EXIT_SIZE_FAILED;
        }

        if (verify_stats.first_non_zero_offset != std::numeric_limits<unsigned long long>::max()) {
            std::cout << "First non-zero byte detected at offset: " << verify_stats.first_non_zero_offset << " bytes" << std::endl;
            if (!ask_user_to_continue()) {
                std::cout << "Operation cancelled by user." << std::endl;
                close(fd);
                return EXIT_USER_ABORTED;
            }
        } else {
            std::cout << "Verification result: target already contains only 00 bytes." << std::endl;
        }
    }

    // Odliczanie przed startem
    countdown();
    if (!keep_running) {
        close(fd);
        return 0;
    }

    // Bufor do kasowania
    char* buffer = nullptr;
    if (!simulate_mode) {
        buffer = new char[erase_size];
        memset(buffer, 0, erase_size);
    }

    unsigned long long total_erased = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (unsigned long long offset = 0; offset < device_size && keep_running; offset += (erase_size + skip_size)) {
        if (lseek(fd, offset, SEEK_SET) < 0) {
            perror("Error seeking device");
            delete[] buffer;
            close(fd);
            return EXIT_SEEK_FAILED;
        }

        const size_t bytes_to_write = std::min<unsigned long long>(erase_size, device_size - offset);

        if (!simulate_mode) {
            size_t bytes_written = 0;

            while (bytes_written < bytes_to_write) {
                ssize_t write_result = write(fd, buffer + bytes_written, bytes_to_write - bytes_written);
                if (write_result < 0) {
                    perror("Error writing to device");
                    delete[] buffer;
                    close(fd);
                    return EXIT_WRITE_FAILED;
                }

                if (write_result == 0) {
                    std::cerr << "Error: no data written to device." << std::endl;
                    delete[] buffer;
                    close(fd);
                    return EXIT_WRITE_FAILED;
                }

                bytes_written += static_cast<size_t>(write_result);
            }
        }

        total_erased += bytes_to_write;

        auto current_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = current_time - start_time;
        const unsigned long long processed_bytes = std::min<unsigned long long>(offset + erase_size + skip_size, device_size);
        double percentage = (double)processed_bytes / device_size * 100.0;
        double erased_percentage = (double)total_erased / device_size * 100.0;
        double speed = elapsed.count() > 0.0 ? (total_erased / (1024.0 * 1024.0)) / elapsed.count() : 0.0;

        print_progress(percentage, speed, erased_percentage);

        if (simulate_mode) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> total_time = end_time - start_time;
    double avg_speed = total_time.count() > 0.0 ? (total_erased / (1024.0 * 1024.0)) / total_time.count() : 0.0;
    const double erased_percentage = device_size > 0 ? (total_erased * 100.0) / device_size : 0.0;

    if (keep_running) {
        if (simulate_mode) {
            std::cout << "\nSimulation completed successfully." << std::endl;
        } else {
            std::cout << "\nErasure completed successfully." << std::endl;
        }
    } else {
        std::cout << "\nErasure interrupted by user." << std::endl;
    }

    std::cout << "Erased data: " << format_bytes(total_erased)
              << " (" << total_erased << " bytes, "
              << std::fixed << std::setprecision(2) << erased_percentage << "%)" << std::endl;
    std::cout << "Total time: " << total_time.count() << " seconds" << std::endl;
    std::cout << "Average speed: " << avg_speed << " MB/s" << std::endl;

    if (buffer != nullptr) {
        delete[] buffer;
    }
    close(fd);
    return EXIT_OK;
}
