add_library(kl_lib
  SimpleSensors.cpp
  buttons.cpp
  kl_i2c.cpp
  kl_lib.cpp
  shell.cpp
  uart.cpp
)

target_link_libraries(kl_lib
  chibios
)

target_include_directories(kl_lib
  PUBLIC
    .
  PRIVATE
    .. # This is here to include board.h
)
