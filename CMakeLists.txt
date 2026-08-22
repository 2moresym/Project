cmake_minimum_required(VERSION 3.16)
project(AIChatNative LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(ai_chat_native
    cpp/native_ui.cpp
    cpp/ChatBubble.hpp
)

target_link_libraries(ai_chat_native PRIVATE Qt6::Widgets)

target_compile_options(ai_chat_native PRIVATE
    -Wall -Wextra -Wpedantic
)
