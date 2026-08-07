list(APPEND tests
  max-length
  normalize-forms
  normalize-primitives
  normalize-quick-check
  normalize-starter-composition
)

foreach(test IN LISTS tests)
  add_executable(${test} ${test}.c)

  target_link_libraries(
    ${test}
    PRIVATE
      normalize_static
  )

  add_test(
    NAME ${test}
    COMMAND ${test}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )

  set_tests_properties(
    ${test}
    PROPERTIES
    TIMEOUT 60
  )
endforeach()

add_executable(conformance conformance.c)

target_link_libraries(
  conformance
  PRIVATE
    normalize_static
)

add_test(
  NAME conformance
  COMMAND conformance "${ucd_data}/NormalizationTest.txt"
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

set_tests_properties(
  conformance
  PROPERTIES
  TIMEOUT 120
)

add_subdirectory(fuzz)
