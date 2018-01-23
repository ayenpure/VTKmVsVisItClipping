cmake_minimum_required(VERSION 3.9)

project(clippingfilter)

find_package(VTKm REQUIRED QUIET
             OPTIONAL_COMPONENTS Serial OpenGL Rendering GLUT
            )

if(VTKm_OpenGL_FOUND AND VTKm_Rendering_FOUND AND VTKm_GLUT_FOUND)
  add_executable(clippingfilter ClippingTrial.cxx)
  target_include_directories(clippingfilter PRIVATE ${VTKm_INCLUDE_DIRS})
  target_link_libraries(clippingfilter ${VTKm_LIBRARIES})
  target_compile_options(clippingfilter PRIVATE ${VTKm_COMPILE_OPTIONS})
endif()



