cmake_minimum_required(VERSION 3.9)
set (CMAKE_CXX_STANDARD 11)

project(datasetbuild)

find_package(VTKm REQUIRED QUIET
             OPTIONAL_COMPONENTS Serial CUDA
            )

add_executable(datasetbuild DataSetCreator.cxx)
target_include_directories(datasetbuild PRIVATE ${VTKm_INCLUDE_DIRS})
target_link_libraries(datasetbuild ${VTKm_LIBRARIES})
target_compile_options(datasetbuild PRIVATE ${VTKm_COMPILE_OPTIONS})
