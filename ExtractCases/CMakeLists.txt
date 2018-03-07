cmake_minimum_required(VERSION 3.9)
set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set (CUDA_LIBRARIES PUBLIC ${CUDA_LIBRARIES})

project(caseextractor)

find_package(VTKm REQUIRED QUIET
             OPTIONAL_COMPONENTS Serial CUDA
            )

# For the clipping and isovolume operator
add_executable(caseextractor extractcases.cxx)
target_include_directories(caseextractor PRIVATE ${VTKm_INCLUDE_DIRS})
target_link_libraries(caseextractor PRIVATE ${VTKm_LIBRARIES} )
target_compile_options(caseextractor PRIVATE ${VTKm_COMPILE_OPTIONS})

add_executable(vanilla vanilla.cxx)
target_include_directories(vanilla PRIVATE ${VTKm_INCLUDE_DIRS})
target_link_libraries(vanilla PRIVATE ${VTKm_LIBRARIES} )
target_compile_options(vanilla PRIVATE ${VTKm_COMPILE_OPTIONS})

if(VTKm_CUDA_FOUND)
  cuda_include_directories(${VTKm_INCLUDE_DIRS})
  cuda_add_executable(caseextractorCU extractcases.cu)
  target_include_directories(caseextractorCU PRIVATE ${VTKm_INCLUDE_DIRS})
  target_link_libraries(caseextractorCU PRIVATE ${VTKm_LIBRARIES} )
  target_compile_options(caseextractorCU PRIVATE ${VTKm_COMPILE_OPTIONS})
  cuda_add_executable(vanillaCU vanilla.cu)
  target_include_directories(vanillaCU PRIVATE ${VTKm_INCLUDE_DIRS})
  target_link_libraries(vanillaCU PRIVATE ${VTKm_LIBRARIES} )
  target_compile_options(vanillaCU PRIVATE ${VTKm_COMPILE_OPTIONS})
endif()

