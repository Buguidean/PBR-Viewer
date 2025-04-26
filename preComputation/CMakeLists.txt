cmake_minimum_required(VERSION 3.10)
project(IMG)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find GLM package
find_package(glm REQUIRED)
find_package(OpenMP REQUIRED)

# Add executable
add_executable(irradiance_generator
  main.cpp
  IMG.cpp
)

# Add stb_image header search path
target_include_directories(irradiance_generator PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}/libs
)

# Link libraries
target_link_libraries(irradiance_generator PRIVATE 
  glm::glm
  m
  OpenMP::OpenMP_CXX
)
