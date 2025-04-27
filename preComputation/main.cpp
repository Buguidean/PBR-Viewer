#include "IMG.h"
#include <iostream>
#include <string>

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <command> <input_cubemap_dir> <output_path> [options]" << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "    diffuse  - Generate diffuse irradiance map" << std::endl;
    std::cout << "    specular - Generate specular environment map" << std::endl;
    std::cout << std::endl;
    std::cout << "Diffuse options:" << std::endl;
    std::cout << "    output_size: Size of the output irradiance map (e.g., 64)" << std::endl;
    std::cout << "    num_samples: Total number of samples to use (e.g., 512)" << std::endl;
    std::cout << std::endl;
    std::cout << "Specular options:" << std::endl;
    std::cout << "    output_size: Base size of the output environment map (e.g., 64)" << std::endl;
    std::cout << "    num_samples: Total number of samples to use (e.g., 512)" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 5) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string command = argv[1];
    std::string inputCubemapDir = argv[2];
    std::string outputPath = argv[3];
    
    if (command == "diffuse") {
        // Diffuse irradiance map generation
        if (argc < 6) {
            printUsage(argv[0]);
            return 1;
        }
        
        int outputSize = std::stoi(argv[4]);
        int numSamples = std::stoi(argv[5]);
        
        std::cout << "PBR Irradiance Map Generator" << std::endl;
        std::cout << "  Input Directory: " << inputCubemapDir << std::endl;
        std::cout << "  Output: " << outputPath << std::endl;
        std::cout << "  Output Size: " << outputSize << "x" << outputSize << std::endl;
        std::cout << "  Samples: " << numSamples << std::endl;
        
        IMG generator;
        
        if (!generator.loadCubemap(inputCubemapDir)) {
            std::cerr << "Failed to initialize diffuse generator" << std::endl;
            return 1;
        }
        
        std::cout << "Computing diffuse irradiance map..." << std::endl;
        if (!generator.computeIrradianceMap(outputSize, numSamples)) {
            std::cerr << "Failed to compute diffuse irradiance map" << std::endl;
            return 1;
        }
        
        std::cout << "Saving diffuse irradiance map to: " << outputPath << std::endl;
        if (!generator.saveIrradianceMap(outputPath)) {
            std::cerr << "Failed to save diffuse irradiance map" << std::endl;
            return 1;
        }
    }
    
    else if (command == "specular") {
        // Specular environment map generation
        if (argc < 6) {
            printUsage(argv[0]);
            return 1;
        }
        
        int outputSize = std::stoi(argv[4]);
        int numSamples = std::stoi(argv[5]);
        int numMipLevels = (argc > 6) ? std::stoi(argv[6]) : 5;
        int samplesPerFrame = (argc > 7) ? std::stoi(argv[7]) : 32;
        
        std::cout << "PBR Specular Environment Map Generator" << std::endl;
        std::cout << "  Input Directory: " << inputCubemapDir << std::endl;
        std::cout << "  Output: " << outputPath << std::endl;
        std::cout << "  Base Output Size: " << outputSize << "x" << outputSize << std::endl;
        
        IMG generator;
        
        if (!generator.loadCubemap(inputCubemapDir)) {
            std::cerr << "Failed to initialize specular generator" << std::endl;
            return 1;
        }
        
        std::cout << "Computing specular environment map..." << std::endl;
        if (!generator.computeSpecularIBL(outputSize, numSamples)) {
            std::cerr << "Failed to compute specular environment map" << std::endl;
            return 1;
        }
        
        std::cout << "Saving specular environment map to: " << outputPath << std::endl;
        if (!generator.saveSpecularIBL(outputPath)) {
            std::cerr << "Failed to save specular environment map" << std::endl;
            return 1;
        }
    }
    
    else {
        std::cerr << "Unknown command: " << command << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    std::cout << "Done!" << std::endl;
    return 0;
}
