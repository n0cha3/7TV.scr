cmake_minimum_required(VERSION 4.0)
project("7TVClock" C)
set (CMAKE_C_STANDARD 99)

add_executable(${PROJECT_NAME} WIN32 main.c res.rc)

set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".scr")

if (MSVC)
  add_compile_definitions(_UNICODE=1 UNICODE=1)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  target_link_libraries(${PROJECT_NAME} winmm.lib gdi32.lib scrnsavw.lib comctl32.lib advapi32.lib) 
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else ()
  add_compile_definitions(_WIN32_WINNT_WIN2K)
  target_link_libraries(${PROJECT_NAME} winmm gdi32 scrnsavw comctl32 advapi32) 
  target_compile_options(${PROJECT_NAME} PRIVATE -municode -Wall -Wextra -Wpedantic -Werror -O2)
endif ()
