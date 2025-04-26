#include "IMG.h"
#include <iostream>
#include <string>

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <command> <input_cubemap_dir> <output_path> [options]" << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  diffuse  - Generate diffuse irradiance map" << std::endl;
    std::cout << "  specular - Generate specular environment map" << std::endl;
    std::cout << std::endl;
    std::cout << "Diffuse options:" << std::endl;
    std::cout << "  <output_size> <num_samples> [samples_per_frame] [adaptive_sampling]" << std::endl;
    std::cout << "    output_size: Size of the output irradiance map (e.g., 64 for 64x64)" << std::endl;
    std::cout << "    num_samples: Total number of samples to use (e.g., 4096)" << std::endl;
    std::cout << std::endl;
    std::cout << "Specular options:" << std::endl;
    std::cout << "  <output_size> <num_samples> [num_mip_levels] [samples_per_frame]" << std::endl;
    std::cout << "    output_size: Base size of the output environment map (e.g., 256)" << std::endl;
    std::cout << "    num_samples: Total number of samples to use (e.g., 1024)" << std::endl;
    std::cout << "    num_mip_levels: Number of roughness levels (default: 5)" << std::endl;
    std::cout << "    samples_per_frame: Samples to process in one batch (default: 32)" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << programName << " diffuse hdri_cubemap output_irradiance 64 4096 32 1" << std::endl;
    std::cout << "  " << programName << " specular hdri_cubemap output_specular 256 1024 5 32" << std::endl;
    std::cout << std::endl;
    std::cout << "For PBR rendering, use HDR input and output with both diffuse and specular maps." << std::endl;
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
    /*
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
        std::cout << "  Mip Levels: " << numMipLevels << std::endl;
        std::cout << "  Samples: " << numSamples << std::endl;
        std::cout << "  Samples Per Frame: " << samplesPerFrame << std::endl;
        
        SpecularEnvironmentMapGenerator generator;
        
        if (!generator.initialize(inputCubemapDir)) {
            std::cerr << "Failed to initialize specular generator" << std::endl;
            return 1;
        }
        
        std::cout << "Computing specular environment map..." << std::endl;
        if (!generator.computePreFilteredMap(outputSize, numSamples, numMipLevels, samplesPerFrame)) {
            std::cerr << "Failed to compute specular environment map" << std::endl;
            return 1;
        }
        
        std::cout << "Saving specular environment map to: " << outputPath << std::endl;
        if (!generator.savePreFilteredMap(outputPath)) {
            std::cerr << "Failed to save specular environment map" << std::endl;
            return 1;
        }
    }
    */
    else {
        std::cerr << "Unknown command: " << command << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    std::cout << "Done!" << std::endl;
    return 0;
}
