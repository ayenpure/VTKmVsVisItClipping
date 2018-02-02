cmake_minimum_required(VERSION 3.9)
set (CMAKE_CXX_STANDARD 11)

project(clippingfilter)

find_package(VTKm REQUIRED QUIET
             OPTIONAL_COMPONENTS TBB Serial CUDA Rendering
            )

add_executable(clippingfilter ClippingTrialOffScreen.cxx)
target_include_directories(clippingfilter PRIVATE ${VTKm_INCLUDE_DIRS})
target_link_libraries(clippingfilter ${VTKm_LIBRARIES})
target_compile_options(clippingfilter PRIVATE ${VTKm_COMPILE_OPTIONS})

if(VTKm_OpenGL_FOUND AND VTKm_Rendering_FOUND AND VTKm_TBB_FOUND)
  # For the clipping and isovolume operator
  add_executable(clippingfilterTBB ClippingTrialOffScreenTBB.cxx)
  target_include_directories(clippingfilterTBB PRIVATE ${VTKm_INCLUDE_DIRS})
  target_link_libraries(clippingfilterTBB ${VTKm_LIBRARIES})
  target_compile_options(clippingfilterTBB PRIVATE ${VTKm_COMPILE_OPTIONS})
endif()
 
if(VTKm_OpenGL_FOUND AND VTKm_Rendering_FOUND AND VTKm_CUDA_FOUND)
  # Cuda compiles do not respect target_include_directories
  cuda_include_directories(${VTKm_INCLUDE_DIRS})
  cuda_add_executable(clippingfilterCUDA ClippingTrialOffScreenCUDA.cu)
  target_include_directories(clippingfilterCUDA PRIVATE ${VTKm_INCLUDE_DIRS})
  target_link_libraries(clippingfilterCUDA PRIVATE ${VTKm_LIBRARIES})
  target_compile_options(clippingfilterCUDA PRIVATE ${VTKm_COMPILE_OPTIONS})
endif()
