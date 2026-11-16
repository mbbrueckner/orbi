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
#include "storage_record.h"
#include "tick_scheduler.h"

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
  printf(
      "    %-14s hunger=%3u  energy=%3u  happiness=%3u  age=%-6u sleeping=%d\n",
      label,
      state->hunger,
      state->energy,
      state->happiness,
      state->age_seconds,
      state->sleeping);
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

static void test_sleep(void) {
  printf("test_sleep\n");
  pet_state_t pet;
  pet_init(&pet);

  pet_tick(&pet, 900);
  CHECK_EQ(pet.hunger, 35, "awake 15 min: hunger 20 + 900/60");
  CHECK_EQ(pet.energy, 93, "awake 15 min: energy 100 - 900/120");
  CHECK_EQ(pet.happiness, 75, "awake 15 min: happiness 80 - 900/180");
  CHECK_EQ(pet.sleeping, false, "a fresh pet is awake");

  pet_sleep(&pet, true);
  CHECK_EQ(pet.sleeping, true, "pet_sleep stores the flag");

  pet_tick(&pet, 900);
  CHECK_EQ(pet.age_seconds, 1800, "age keeps counting while asleep");
  CHECK_EQ(pet.hunger, 36, "asleep 15 min: hunger only 900/600");
  CHECK_EQ(pet.happiness, 74, "asleep 15 min: happiness only 900/900");
  CHECK_EQ(pet.energy, 100, "asleep 15 min: energy recovers to full");
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
static void test_sleep_button_is_not_free(void) {
  printf("test_sleep_button_is_not_free\n");

  // Asleep: tapping "sleep" again must not slow anything down.
  pet_state_t honest;
  pet_state_t spammer;
  pet_init(&honest);
  pet_init(&spammer);
  pet_sleep(&honest, true);
  pet_sleep(&spammer, true);
  for (int i = 0; i < 100; i++) {
    pet_tick(&honest, 300);
    pet_tick(&spammer, 300);
    pet_sleep(&spammer, true);
  }
  CHECK_EQ(spammer.hunger,
           honest.hunger,
           "asleep: tapping sleep does not stop hunger");
  CHECK_EQ(spammer.happiness,
           honest.happiness,
           "asleep: tapping sleep does not stop the happiness decay");

  // Awake: the same trick with the wake call.
  pet_state_t quiet;
  pet_state_t tapper;
  pet_init(&quiet);
  pet_init(&tapper);
  for (int i = 0; i < 300; i++) {
    pet_tick(&quiet, 30);
    pet_tick(&tapper, 30);
    pet_sleep(&tapper, false);
  }
  CHECK_EQ(
      tapper.hunger, quiet.hunger, "awake: tapping wake does not stop hunger");
  CHECK_EQ(tapper.happiness,
           quiet.happiness,
           "awake: tapping wake does not stop the happiness decay");
}

/** @brief Checks that the scheduler turns timestamps into pet_tick() calls. */
static void test_scheduler(void) {
  printf("test_scheduler\n");

  // Normal advance: 60 s awake is one hunger point.
  pet_state_t pet;
  tick_scheduler_t sched;
  pet_init(&pet);
  tick_scheduler_init(&sched, 1000);
  uint32_t applied = tick_scheduler_update(&sched, &pet, 1060);
  CHECK_EQ(applied, 60, "returns the seconds applied");
  CHECK_EQ(pet.hunger, 21, "60 s advances hunger by 1");

  // No time passed.
  pet_state_t before = pet;
  applied = tick_scheduler_update(&sched, &pet, 1060);
  CHECK_EQ(applied, 0, "same timestamp applies nothing");
  CHECK_EQ(pet.age_seconds, before.age_seconds, "same timestamp: age unchanged");

  // Clock goes backwards: no underflow, resync to the new time.
  before = pet;
  applied = tick_scheduler_update(&sched, &pet, 500);
  CHECK_EQ(applied, 0, "backwards clock applies nothing");
  CHECK_EQ(pet.hunger, before.hunger, "backwards clock: hunger unchanged");
  CHECK_EQ(pet.age_seconds, before.age_seconds, "backwards clock: age unchanged");
  applied = tick_scheduler_update(&sched, &pet, 560);
  CHECK_EQ(applied, 60, "ticks from the new baseline after a resync");

  // Long absence matches a direct pet_tick() with the same span.
  pet_state_t via_sched;
  pet_state_t direct;
  pet_init(&via_sched);
  pet_init(&direct);
  tick_scheduler_init(&sched, 0);
  tick_scheduler_update(&sched, &via_sched, 100000);
  pet_tick(&direct, 100000);
  CHECK_EQ(via_sched.hunger, direct.hunger, "long absence: hunger matches");
  CHECK_EQ(via_sched.energy, direct.energy, "long absence: energy matches");
  CHECK_EQ(via_sched.happiness,
           direct.happiness,
           "long absence: happiness matches");

  // Many small updates equal one big update (carries survive).
  pet_state_t small_steps;
  pet_state_t one_step;
  pet_init(&small_steps);
  pet_init(&one_step);
  tick_scheduler_init(&sched, 0);
  for (uint32_t t = 7; t <= 7000; t += 7) {
    tick_scheduler_update(&sched, &small_steps, t);
  }
  tick_scheduler_init(&sched, 0);
  tick_scheduler_update(&sched, &one_step, 7000);
  CHECK_EQ(small_steps.hunger, one_step.hunger, "small steps: hunger matches");
  CHECK_EQ(small_steps.energy, one_step.energy, "small steps: energy matches");
  CHECK_EQ(small_steps.happiness,
           one_step.happiness,
           "small steps: happiness matches");
  CHECK_EQ(small_steps.age_seconds,
           one_step.age_seconds,
           "small steps: age matches");
}

/** @brief Checks the persisted byte format. */
static void test_storage_record(void) {
  printf("test_storage_record\n");

  storage_record_t in;
  pet_init(&in.pet);
  in.pet.hunger = 55;
  in.pet.energy = 33;
  in.pet.happiness = 77;
  in.pet.sleeping = true;
  in.pet.age_seconds = 0x01020304;
  in.pet.hunger_carry_s = 0x0A0B;
  in.pet.energy_carry_s = 59;
  in.pet.happiness_carry_s = 179;
  in.last_time_s = 0xDEADBEEF;

  uint8_t buf[STORAGE_RECORD_SIZE];
  size_t len = storage_record_pack(&in, buf);
  CHECK_EQ(len, STORAGE_RECORD_SIZE, "pack writes 19 bytes");
  CHECK_EQ(buf[0], STORAGE_RECORD_VERSION, "byte 0 is the version");
  CHECK_EQ(buf[5], 0x04, "age_seconds is little-endian (low byte first)");
  CHECK_EQ(buf[8], 0x01, "age_seconds is little-endian (high byte last)");

  storage_record_t out;
  CHECK_EQ(storage_record_unpack(buf, len, &out), 1, "round trip unpacks");
  CHECK_EQ(out.pet.hunger, 55, "round trip: hunger");
  CHECK_EQ(out.pet.energy, 33, "round trip: energy");
  CHECK_EQ(out.pet.happiness, 77, "round trip: happiness");
  CHECK_EQ(out.pet.sleeping, 1, "round trip: sleeping");
  CHECK_EQ(out.pet.age_seconds, 0x01020304, "round trip: age");
  CHECK_EQ(out.pet.hunger_carry_s, 0x0A0B, "round trip: hunger carry");
  CHECK_EQ(out.pet.energy_carry_s, 59, "round trip: energy carry");
  CHECK_EQ(out.pet.happiness_carry_s, 179, "round trip: happiness carry");
  CHECK_EQ((long)out.last_time_s, (long)0xDEADBEEF, "round trip: last_time_s");

  // Rejections leave the output untouched.
  storage_record_t untouched;
  pet_init(&untouched.pet);
  untouched.last_time_s = 42;

  CHECK_EQ(storage_record_unpack(buf, len - 1, &untouched), 0, "truncated rejected");
  CHECK_EQ(storage_record_unpack(buf, len + 1, &untouched), 0, "too long rejected");

  uint8_t bad[STORAGE_RECORD_SIZE];
  for (size_t i = 0; i < sizeof(bad); i++) {
    bad[i] = buf[i];
  }
  bad[0] = 99;
  CHECK_EQ(storage_record_unpack(bad, len, &untouched), 0, "unknown version rejected");
  bad[0] = buf[0];
  bad[1] = 101;
  CHECK_EQ(storage_record_unpack(bad, len, &untouched), 0, "hunger > 100 rejected");
  bad[1] = buf[1];
  bad[4] = 2;
  CHECK_EQ(storage_record_unpack(bad, len, &untouched), 0, "sleeping not 0/1 rejected");
  CHECK_EQ(untouched.last_time_s, 42, "failed unpack leaves output untouched");
}

/** @brief Checks the periodic save timing. */
static void test_save_timing(void) {
  printf("test_save_timing\n");
  tick_scheduler_t sched;
  tick_scheduler_init(&sched, 1000);

  CHECK_EQ(tick_scheduler_save_due(&sched, 1000, 300), 0, "not due right after init");
  CHECK_EQ(tick_scheduler_save_due(&sched, 1299, 300), 0, "not due before interval");
  CHECK_EQ(tick_scheduler_save_due(&sched, 1300, 300), 1, "due at interval");

  tick_scheduler_mark_saved(&sched, 1300);
  CHECK_EQ(tick_scheduler_save_due(&sched, 1400, 300), 0, "mark_saved restarts the interval");
  CHECK_EQ(tick_scheduler_save_due(&sched, 1600, 300), 1, "due again after the next interval");

  pet_state_t pet;
  pet_init(&pet);
  tick_scheduler_update(&sched, &pet, 100);  // clock jumps backwards
  CHECK_EQ(tick_scheduler_save_due(&sched, 100, 300), 0, "backwards clock: no immediate save");
  CHECK_EQ(tick_scheduler_save_due(&sched, 400, 300), 1, "backwards clock: interval counts from new time");
}

int main(void) {
  test_init_values();
  test_tick_is_additive();
  test_happiness_decays();
  test_long_absence();
  test_sleep();
  test_sleep_button_is_not_free();
  test_scheduler();
  test_storage_record();
  test_save_timing();

  printf("\n%d check(s) failed\n", failures);
  return failures == 0 ? 0 : 1;
}
