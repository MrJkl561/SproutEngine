// The test program's main() comes from doctest. Every other test_*.cpp file
// just contains TEST_CASEs - doctest finds and runs all of them.
//
// Useful command line options (run ./sprout_tests --help for all of them):
//   ./sprout_tests -tc="*Rect*"      only run tests whose name matches
//   ./sprout_tests -s                also show passing checks
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
