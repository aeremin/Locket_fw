add_library(radio
  cc1101.cpp
  radio_lvl1.cpp
)

target_link_libraries(radio kl_lib)

target_include_directories(radio
  PUBLIC
    .
  PRIVATE
    .. # This is here to include board.h
)
