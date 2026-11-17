add_test_executables(
  max-length.c
  normalize-forms.c
  normalize-primitives.c
  normalize-quick-check.c
  normalize-starter-composition.c
  LINK
    normalize::<TYPE>
  TIMEOUT 60
)

add_test_executable(
  conformance
  conformance.c
  LINK
    normalize::<TYPE>
  ARGS
    "${ucd_data}/NormalizationTest.txt"
  TIMEOUT 120
)

add_subdirectory(fuzz)
