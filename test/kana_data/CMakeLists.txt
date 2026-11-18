add_executable(KanaDataTest
  KanaDataTest.cpp
  ${REPO_ROOT}/src/kana/KanaData.cpp
)

target_compile_definitions(KanaDataTest PRIVATE CROSSPOINT_KANA_PRACTICE=1)

target_include_directories(KanaDataTest PRIVATE
  ${REPO_ROOT}/src
)

target_link_libraries(KanaDataTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(KanaDataTest)
