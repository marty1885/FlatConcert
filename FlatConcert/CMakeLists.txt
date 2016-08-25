project(flatConcert)

add_executable(flatConcert src/main.cpp)
target_link_libraries(flatConcert -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-network -lsfml-system)
target_link_libraries(flatConcert -lportaudio -lportaudiocpp)
