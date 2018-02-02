cmake_minimum_required(VERSION 3.9)
set (CMAKE_CXX_STANDARD 11)

project(clippingfilter)

find_package(VTKm REQUIRED QUIET
             OPTIONAL_COMPONENTS Serial CUDA OpenGL Rendering GLUT
            )

if(VTKm_OpenGL_FOUND AND VTKm_Rendering_FOUND AND VTKm_GLUT_FOUND AND VTKm_CUDA_FOUND)
# For the clipping and isovolume operator
  add_executable(clippingfilter ClippingTrial.cxx)
  target_include_directories(clippingfilter PRIVATE ${VTKm_INCLUDE_DIRS})
  target_link_libraries(clippingfilter ${VTKm_LIBRARIES})
  target_compile_options(clippingfilter PRIVATE ${VTKm_COMPILE_OPTIONS})

# For the clipping and isovolume operator
  add_executable(clippingfilteroffscreen ClippingTrialOffScreen.cxx)
  target_include_directories(clippingfilteroffscreen PRIVATE ${VTKm_INCLUDE_DIRS})
  target_link_libraries(clippingfilteroffscreen ${VTKm_LIBRARIES})
  target_compile_options(clippingfilteroffscreen PRIVATE ${VTKm_COMPILE_OPTIONS})

# For getting the statistics
  add_executable(splitcellproc SplitCellReader.cxx)
  target_include_directories(splitcellproc PRIVATE ${VTKm_INCLUDE_DIRS})
  target_link_libraries(splitcellproc ${VTKm_LIBRARIES})
  target_compile_options(splitcellproc PRIVATE ${VTKm_COMPILE_OPTIONS})

  # Cuda compiles do not respect target_include_directories
  cuda_include_directories(${VTKm_INCLUDE_DIRS})
  cuda_add_executable(clippingfilter_CUDA ClippingTrial.cu)
  target_include_directories(clippingfilter_CUDA PRIVATE ${VTKm_INCLUDE_DIRS})
  target_link_libraries(clippingfilter_CUDA PRIVATE ${VTKm_LIBRARIES})
  target_compile_options(clippingfilter_CUDA PRIVATE ${VTKm_COMPILE_OPTIONS})
endif()
