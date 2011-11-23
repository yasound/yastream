cmake_minimum_required(VERSION 2.4)

project(yastrm)

include_directories(
  ../../nui3/include/
)

add_definitions(
  -fpermissive
)

add_executable (streamer src/Application.cpp)

target_link_libraries(streamer mpg123 expat jpeg png freetype ungif nui3 ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES})
