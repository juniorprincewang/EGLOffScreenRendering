cmake_minimum_required( VERSION 3.5 )
project( EGLOffScreenRendering )

include_directories( include )

if(MSVC)
link_directories(libs/win)
add_executable(offscreen_test  offscreen_egl.cpp )
target_link_libraries( offscreen_test libEGL.dll.lib libGLESv2.dll.lib )
endif()

if(UNIX AND NOT APPLE) # for Linux, BSD, Solaris, Minix
link_directories(libs/linux)
add_executable(offscreen_test  offscreen_egl.cpp )
target_link_libraries( offscreen_test EGL GLESv2 )

add_executable(multithreads multithreads.cpp)
target_link_libraries(multithreads EGL GLESv2 pthread)
endif()

