add_library(pill
  pill.cpp
  pill_mgr.cpp
)

target_link_libraries(pill kl_lib)

target_include_directories(pill
  PUBLIC
    .
  PRIVATE
    .. # This is here to include board.h
)