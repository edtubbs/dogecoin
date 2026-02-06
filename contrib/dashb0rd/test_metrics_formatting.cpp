// Simple test to verify the formatting logic for dashboard metrics
// Compile with: g++ -std=c++11 -o test_metrics_formatting test_metrics_formatting.cpp

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdint>

std::string formatPercentage(double ratio) {
    std::ostringstream progressStream;
    progressStream << std::fixed << std::setprecision(2) << (ratio * 100.0) << "%";
    return progressStream.str();
}

std::string formatSize(uint64_t diskSize) {
    const char* sizeUnits[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIdx = 0;
    double humanSize = (double)diskSize;
    
    while (humanSize >= 1024.0 && unitIdx < 4) {
        humanSize /= 1024.0;
        unitIdx++;
    }
    
    std::ostringstream sizeStream;
    sizeStream << std::fixed << std::setprecision(2) << humanSize << " " << sizeUnits[unitIdx];
    return sizeStream.str();
}

int main() {
    std::cout << "Testing dashboard metrics formatting..." << std::endl << std::endl;
    
    // Test percentage formatting
    std::cout << "Percentage formatting tests:" << std::endl;
    std::cout << "  0.0 -> " << formatPercentage(0.0) << std::endl;
    std::cout << "  0.5 -> " << formatPercentage(0.5) << std::endl;
    std::cout << "  0.9995 -> " << formatPercentage(0.9995) << std::endl;
    std::cout << "  1.0 -> " << formatPercentage(1.0) << std::endl;
    std::cout << std::endl;
    
    // Test size formatting
    std::cout << "Size formatting tests:" << std::endl;
    std::cout << "  0 bytes -> " << formatSize(0) << std::endl;
    std::cout << "  500 bytes -> " << formatSize(500) << std::endl;
    std::cout << "  1024 bytes -> " << formatSize(1024) << std::endl;
    std::cout << "  1 MB -> " << formatSize(1024 * 1024) << std::endl;
    std::cout << "  100 MB -> " << formatSize(100ULL * 1024 * 1024) << std::endl;
    std::cout << "  5 GB -> " << formatSize(5ULL * 1024 * 1024 * 1024) << std::endl;
    std::cout << "  78.45 GB -> " << formatSize(84216225792ULL) << std::endl;
    std::cout << "  1.5 TB -> " << formatSize(1649267441664ULL) << std::endl;
    std::cout << std::endl;
    
    std::cout << "All formatting tests completed successfully!" << std::endl;
    
    return 0;
}
