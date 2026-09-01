cmake_minimum_required(VERSION 3.14)
project(lhcui CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

# Add HUI_USE_SDL1 build option
option(HUI_USE_SDL1 "Use SDL 1.2 instead of SDL2" OFF)

# Add HUI_ENABLE_KEYBOARD_FALLBACK build option
# Default to ON when NDEBUG is not defined
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    set(FALLBACK_DEFAULT ON)
else()
    set(FALLBACK_DEFAULT OFF)
endif()

option(HUI_ENABLE_KEYBOARD_FALLBACK "Enable keyboard fallback" ${FALLBACK_DEFAULT})

# hui (static library)
add_library(hui STATIC
    src/renderer/SDL1Renderer.cpp
    src/renderer/SDL2Renderer.cpp
    src/ViewStack.cpp
    src/KeyRepeatDriver.cpp
    src/ChordDetector.cpp
    src/UISystem.cpp
    src/sdl/ButtonMapping.cpp
    src/sdl/SDLGamepadHelper.cpp
    src/sdl/KeyboardFallback.cpp
    src/Helpers.cpp
    src/ListSource.cpp
    src/ListItemWidget.cpp
    src/GridCellWidget.cpp
    src/ProgressBar.cpp
    src/Slider.cpp
    src/SortModeIndicator.cpp
    src/ShuffleToggle.cpp
    src/RepeatModeToggle.cpp
    src/ListHeaderWidget.cpp
    src/SeekableProgressBar.cpp
    src/PlaybackControlsRow.cpp
    src/HintBarWidget.cpp
    src/StatusBarWidget.cpp
    src/ToastNotification.cpp
    src/ListView.cpp
    src/GridView.cpp
    src/TabBarWidget.cpp
    src/QueueList.cpp
    src/LetterWheel.cpp
    src/OnScreenKeyboard.cpp
    src/ContextMenuView.cpp
    src/ConfirmationDialogView.cpp
    src/TrackInfoPanelView.cpp
    src/GuideOverlayView.cpp
)

target_include_directories(hui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(hui PRIVATE -Wall -Wextra)

if(HUI_USE_SDL1)
    target_compile_definitions(hui PUBLIC HUI_USE_SDL1)
    pkg_check_modules(SDL REQUIRED sdl SDL_ttf)
else()
    pkg_check_modules(SDL REQUIRED sdl2 SDL2_ttf)
endif()

target_include_directories(hui PRIVATE ${SDL_INCLUDE_DIRS})

if(HUI_ENABLE_KEYBOARD_FALLBACK)
    target_compile_definitions(hui PUBLIC HUI_ENABLE_KEYBOARD_FALLBACK)
endif()

# hui_example (executable)
add_executable(hui_example example/main.cpp)
target_include_directories(hui_example PRIVATE ${SDL_INCLUDE_DIRS})
target_link_libraries(hui_example PRIVATE hui ${SDL_LDFLAGS})
target_compile_options(hui_example PRIVATE -Wall -Wextra ${SDL_CFLAGS})

# Testing
enable_testing()

add_executable(hui_tests
    tests/main.cpp
    tests/test_types.cpp
    tests/test_focus.cpp
    tests/test_view.cpp
    tests/test_input.cpp
    tests/test_uisystem.cpp
    tests/test_sdl.cpp
    tests/test_helpers.cpp
    tests/test_atoms.cpp
    tests/test_molecules.cpp
    tests/test_organisms.cpp
)
target_include_directories(hui_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests ${SDL_INCLUDE_DIRS})
target_link_libraries(hui_tests PRIVATE hui ${SDL_LDFLAGS})
target_compile_options(hui_tests PRIVATE -Wall -Wextra ${SDL_CFLAGS})

add_test(NAME CoreTypesTest COMMAND hui_tests)

