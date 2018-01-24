cmake_minimum_required(VERSION 3.9)

project(clippingfilter)
project(splitcellproc)

find_package(VTKm REQUIRED QUIET
             OPTIONAL_COMPONENTS Serial OpenGL Rendering GLUT
            )

if(VTKm_OpenGL_FOUND AND VTKm_Rendering_FOUND AND VTKm_GLUT_FOUND)
# For the clipping and isovolume operator
  add_executable(clippingfilter ClippingTrial.cxx)
  target_include_directories(clippingfilter PRIVATE ${VTKm_INCLUDE_DIRS})
  target_link_libraries(clippingfilter ${VTKm_LIBRARIES})
  target_compile_options(clippingfilter PRIVATE ${VTKm_COMPILE_OPTIONS})

# For getting the statistics
  add_executable(splitcellproc SplitCellReader.cxx)
  target_include_directories(splitcellproc PRIVATE ${VTKm_INCLUDE_DIRS})
  target_link_libraries(splitcellproc ${VTKm_LIBRARIES})
  target_compile_options(splitcellproc PRIVATE ${VTKm_COMPILE_OPTIONS})

endif()



