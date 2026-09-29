add_executable(managed_point_edit main.cpp)
serializer_generate(TARGET managed_point_edit SCHEMAS point.serializer)
target_link_libraries(managed_point_edit PRIVATE Serializer::managed)
