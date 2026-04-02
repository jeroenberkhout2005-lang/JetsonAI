cmake_minimum_required(VERSION 3.16)
project(FatParticleDetector LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    set(CMAKE_PREFIX_PATH
        "C:/Qt/5.15.2/msvc2019_64"
        "C:/libtorch"
    )
    set(OpenCV_DIR "C:/opencv/opencv/build")
endif()

#stop warnings from libtorch
if(MSVC)
    add_compile_options(/wd4996)
else()
    add_compile_options(-Wno-deprecated-declarations)
endif()

#Qt5
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

#Avoid Qt Keyword macro collisions with LibTorch Headers
add_definitions(-DQT_NO_KEYWORDS)

find_package(OpenCV REQUIRED)

# ── LibTorch ──────────────────────────────────────────────────────────────────
# Option A (recommended): set via cmake flag:
#   cmake .. -DCMAKE_PREFIX_PATH=/path/to/libtorch
# Option B: set TORCH_DIR environment variable.
# On Jetson you can also use the system PyTorch:
#   export TORCH_DIR=$(python3 -c "import torch; print(torch.utils.cmake_prefix_path)")
find_package(Torch REQUIRED)


set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/CameraWorker.cpp
    src/VideoViewWidget.cpp
    src/VideoStageWidget.cpp
    src/BackendWorker.cpp
)

set(HEADERS
    include/MainWindow.h
    include/CameraWorker.h
    include/VideoViewWidget.h
    include/VideoStageWidget.h
    include/BackendWorker.h
)

include_directories(include)
set(APP_ICON_RESOURCE resources/app_icon.rc)
add_executable(ParticleTrackerGUI ${SOURCES} ${HEADERS} ${APP_ICON_RESOURCE})

target_link_libraries(ParticleTrackerGUI
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    ${OpenCV_LIBS}
    ${TORCH_LIBRARIES}
)

# LibTorch requires these flags
set_property(TARGET ParticleTrackerGUI PROPERTY CXX_STANDARD 17)

# Copy the model file next to the binary after build (edit path as needed)
# add_custom_command(TARGET FatParticleDetector POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy
#         ${CMAKE_SOURCE_DIR}/models/fat_detector.pt
#         $<TARGET_FILE_DIR:FatParticleDetector>/fat_detector.pt
# )
