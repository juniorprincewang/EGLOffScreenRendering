cmake_minimum_required( VERSION 3.5 )
project( EGLOffScreenRendering )

include_directories( include )

add_executable(offscreen_test  offscreen_egl.cpp )
if(MSVC)
link_directories(libs/win)
target_link_libraries( offscreen_test libEGL.dll.lib libGLESv2.dll.lib )
endif()

if(UNIX AND NOT APPLE) # for Linux, BSD, Solaris, Minix
link_directories(${CMAKE_SOURCE_DIR}/libs/linux)
target_link_libraries( offscreen_test EGL GLESv2 )
endif()