cmake_minimum_required(VERSION 3.13)
project(example)

find_package(cppzmq)

foreach(TGT server client)
  add_executable(${TGT} "${TGT}.cc")
  target_link_libraries(${TGT} cppzmq)
  set_property(TARGET ${TGT} PROPERTY CXX_STANDARD 17)
endforeach()

install(TARGETS server client RUNTIME DESTINATION bin)
