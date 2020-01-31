cmake_minimum_required(VERSION 3.14)
project(Locket_fw VERSION 1.0.0 LANGUAGES C CXX ASM)

set(CMAKE_CXX_FLAGS "-mthumb -mcpu=cortex-m3 -std=gnu++14 -O0 -ffunction-sections -fdata-sections -fno-rtti -fno-exceptions -g -fstack-usage -Wall -fno-threadsafe-statics -MMD -MP -specs=nano.specs")
set(CMAKE_C_FLAGS   "-mthumb -mcpu=cortex-m3 -std=gnu11 -O0 -ffunction-sections -fdata-sections -g -fstack-usage -Wall -MMD -MP -specs=nano.specs")
set(CMAKE_ASM_FLAGS "-mthumb -mcpu=cortex-m3 -g -Wa,--warn -x assembler-with-cpp -specs=nano.specs")

add_subdirectory(os)
add_subdirectory(kl_lib)
add_subdirectory(Radio)
add_subdirectory(Pill)

add_executable(Locket_fw.elf
  ability.cpp
  eventHandlers.cpp
  health.cpp
  main.cpp
  player_type.cpp
  qhsm.c
)

target_link_libraries(Locket_fw.elf
  kl_lib
  radio
  pill
)

target_include_directories(Locket_fw.elf
  PRIVATE .
)

set_target_properties(Locket_fw.elf PROPERTIES LINK_FLAGS
  "-T ${CMAKE_CURRENT_SOURCE_DIR}/STM32L151x8.ld -static -Wl,-cref,-u,Reset_Handler -Wl,-Map=Locket_fw.map -Wl,--gc-sections -Wl,--defsym=malloc_getpagesize_P=0x1000 -Wl,--start-group -lc -lm -lstdc++ -lsupc++ -Wl,--end-group")
