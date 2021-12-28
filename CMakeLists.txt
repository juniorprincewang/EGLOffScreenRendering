cmake_minimum_required( VERSION 3.5 )
project( EGLOffScreenRendering )

include_directories( include )
link_directories(.)

add_executable(offscreen_test  offscreen_egl.cpp )
if(MSVC)
target_link_libraries( offscreen_test libEGL.dll.lib libGLESv2.dll.lib )
endif()