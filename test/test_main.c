/**
 * @file test_main.c
 * @brief Host-side unit tests for the hardware independent pet_core logic.
 *
 * Built with plain CMake instead of ESP-IDF, so the game rules can be checked
 * on a PC in under a second without flashing the board. See
 * test/CMakeLists.txt.
 *
 * Run:
 * @code
 * cmake -S test -B test/build
 * cmake --build test/build
 * ./test/build/orbi_test
 * @endcode
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "pet_state.h"

/** Number of failed checks, turned into the process exit code by main(). */
static int failures = 0;

/**
 * @brief Prints all four state values as one aligned line.
 *
 * Used for traces where the point is to read the numbers, not to assert them.
 *
 * @param label Short prefix describing the moment, e.g. "after 1h".
 * @param state State to print. Not modified.
 */
static void print_state(const char* label, const pet_state_t* state) {
  printf("    %-14s hunger=%3u  energy=%3u  happiness=%3u  age=%u\n",
         label,
         state->hunger,
         state->energy,
         state->happiness,
         state->age_seconds);
}

/**
 * @brief Reports one expectation and counts it if it failed.
 *
 * Never aborts, so a single broken rule does not hide the ones after it.
 *
 * Use this for conditions that are not plain equality. Because the condition
 * is already reduced to a bool by the time we get here, the operands are gone
 * and cannot be printed -- use CHECK_EQ() when you need to see the values.
 *
 * @param passed Result of the condition under test.
 * @param what Human readable description of the rule being checked.
 */
static void check(bool passed, const char* what) {
  printf("  %s %s\n", passed ? "ok  " : "FAIL", what);
  if (!passed) {
    failures++;
  }
}

/**
 * @brief Backs CHECK_EQ(); do not call directly.
 *
 * Takes both operands separately so it can still report them when they differ.
 * Keeping the comparison in a function (rather than in the macro) also means
 * each argument expression is evaluated exactly once.
 *
 * @param actual Value produced by the code under test.
 * @param expected Value the test requires.
 * @param what Human readable description of the rule being checked.
 * @param file Source file of the call site, supplied by the macro.
 * @param line Line of the call site, supplied by the macro.
 */
static void check_eq_impl(
    long actual, long expected, const char* what, const char* file, int line) {
  if (actual == expected) {
    printf("  ok   %s\n", what);
  } else {
    printf("  FAIL %s: expected %ld, got %ld  (%s:%d)\n",
           what,
           expected,
           actual,
           file,
           line);
    failures++;
  }
}

/**
 * @brief Asserts that two integers are equal, reporting both on failure.
 *
 * A macro rather than a function so that __FILE__ and __LINE__ expand at the
 * call site instead of inside the helper. Both operands are cast to long so
 * one macro covers every integer type in pet_state_t.
 *
 * @param actual Value produced by the code under test.
 * @param expected Value the test requires.
 * @param what Human readable description of the rule being checked.
 */
#define CHECK_EQ(actual, expected, what) \
  check_eq_impl((long)(actual), (long)(expected), (what), __FILE__, __LINE__)

/** @brief Checks that a fresh pet starts with the documented values. */
static void test_init_values(void) {
  printf("test_init_values\n");
  pet_state_t pet;
  pet_init(&pet);

  CHECK_EQ(pet.hunger, 20, "hunger starts at 20");
  CHECK_EQ(pet.energy, 100, "energy starts at 100");
  CHECK_EQ(pet.happiness, 80, "happiness starts at 80");
  CHECK_EQ(pet.age_seconds, 0, "age starts at 0");
}

/**
 * @brief Checks that pet_tick() does not lose time when called in small steps.
 *
 * Letting 60 seconds pass must give the same result whether they are reported
 * in one call or second by second. This matters because the firmware ticks
 * roughly once per second.
 */
static void test_tick_is_additive(void) {
  printf("test_tick_is_additive\n");
  pet_state_t once;
  pet_state_t stepwise;
  pet_init(&once);
  pet_init(&stepwise);

  pet_tick(&once, 60);
  for (int i = 0; i < 60; i++) {
    pet_tick(&stepwise, 1);
  }

  CHECK_EQ(stepwise.hunger, once.hunger, "hunger is the same either way");
  CHECK_EQ(stepwise.energy, once.energy, "energy is the same either way");
  CHECK_EQ(
      stepwise.age_seconds, once.age_seconds, "age is the same either way");
}

/**
 * @brief Checks that a neglected pet loses happiness over time.
 *
 * Deliberately asserts the direction only, not a concrete value, so that
 * retuning the decay rate does not break the test.
 */
static void test_happiness_decays(void) {
  printf("test_happiness_decays\n");
  pet_state_t pet;
  pet_init(&pet);
  uint8_t before = pet.happiness;

  pet_tick(&pet, 3600);  // one hour, untouched

  print_state("after 1h", &pet);
  check(pet.happiness < before, "happiness drops when ignored");
}

/**
 * @brief Checks that a very long absence saturates the stats instead of
 *        wrapping them around.
 *
 * This is the overnight case: the device was off, so pet_tick() gets a single
 * call with a huge span. Every stat has to stop at its limit. Doing the
 * arithmetic in unsigned types would make a starved pet come out at 100.
 */
static void test_long_absence(void) {
  printf("test_long_absence\n");
  pet_state_t pet;
  pet_init(&pet);

  pet_tick(&pet, 100000);  // a bit over 27 hours

  print_state("after 27h", &pet);
  CHECK_EQ(pet.hunger, 100, "hunger saturates at 100");
  CHECK_EQ(pet.energy, 0, "energy bottoms out at 0");
  CHECK_EQ(pet.happiness, 0, "happiness bottoms out at 0");
}

/**
 * @brief Prints a readable trace of the three interactions.
 *
 * Asserts nothing; this is a scratchpad for balancing the decay rates by eye.
 */
static void demo_interactions(void) {
  printf("demo_interactions\n");
  pet_state_t pet;
  pet_init(&pet);

  print_state("fresh", &pet);
  pet_tick(&pet, 120);
  print_state("+120s", &pet);
  pet_feed(&pet);
  print_state("fed", &pet);
  pet_play(&pet);
  print_state("played", &pet);
}

/**
 * @brief Runs every test and summarises the result.
 *
 * @return 0 when all checks passed, 1 otherwise, so the shell and any future
 *         CI can tell success from failure.
 */
int main(void) {
  test_init_values();
  test_tick_is_additive();
  test_happiness_decays();
  test_long_absence();
  demo_interactions();

  printf("\n%d check(s) failed\n", failures);
  return failures == 0 ? 0 : 1;
}
