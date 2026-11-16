if(TARGET normalize_shared)
  set(library normalize::shared)
else()
  set(library normalize::static)
endif()

add_test_executables(
  max-length.c
  normalize-forms.c
  normalize-primitives.c
  normalize-quick-check.c
  normalize-starter-composition.c
  LINK
    ${library}
  TIMEOUT 60
)

add_test_executable(
  conformance
  conformance.c
  LINK
    ${library}
  ARGS
    "${ucd_data}/NormalizationTest.txt"
  TIMEOUT 120
)

add_subdirectory(fuzz)
