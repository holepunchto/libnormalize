list(APPEND fuzzers
  normalize
)

foreach(fuzzer IN LISTS fuzzers)
  string(REPLACE "/" "_" name ${fuzzer})

  add_fuzzer(
    fuzz_${name}
    ${fuzzer}.c
    DURATION 600
  )

  target_link_libraries(
    fuzz_${name}
    PRIVATE
      normalize_static
  )
endforeach()
