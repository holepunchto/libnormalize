add_fuzz_executables(
  normalize.c
  PREFIX fuzz_
  DURATION 600
  LINK
    normalize::static
    utf::static
)
