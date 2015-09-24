// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
#include <bse/bseblockutils.hh>
#include <sfi/sfitests.hh>
#include <bse/bsemain.hh>

static void
report_usecs (const char *what, double usecs)
{
  TMSG ("    %-28s : %+.14f µseconds", what, usecs);
}

template<typename T> static bool
block_check (guint    n,
             const T *block,
             T        value)
{
  while (n--)
    if (block[n] != value)
      {
        TCMP (block[n], ==, value);
        return false;
      }
  return true;
}

/**
 * Shuffles a block, using the O(n) algorithm called the Knuth shuffle
 * or Fisher-Yates shuffle, for instance explained on
 *
 *   http://en.wikipedia.org/wiki/Shuffle#Shuffling_algorithms
 *
 * This creates a random permutation of the elements, where each permutation
 * is equally likely (given that the random generator is good).
 *
 * Since number of possible permutations is n_elements!, which grows rather
 * fast, it can easily happen that some permutations can never be generated
 * due to limited periodicity of the random number generator or weak seeding.
 */
template<typename T> static void
block_shuffle (guint n_elements,
               T    *elements)
{
  for (guint i = 0; i < n_elements; i++)
    {
      guint exchange = g_random_int_range (i, n_elements);
      T tmp = elements[exchange];
      elements[exchange] = elements[i];
      elements[i] = tmp;
    }
}

static void
build_ascending_random_block (guint  n_values,
                              float *fblock)
{
  /* build block with ascending random values approximately in the range -1..1 */
  fblock[0] = g_random_double_range (-1.05, -0.95);
  for (guint i = 1; i < n_values; i++)
    fblock[i] = fblock[i-1] + g_random_double_range (1e-10, 4.0 / n_values);
}
static void
test_fill (void)
{
  TSTART ("BlockFill");
  float fblock1[1024];
  bse_block_fill_uint32 (1024, (uint32*) (void*) fblock1, 0);
  TASSERT (block_check (1024, fblock1, 0.f) == true);
  bse_block_fill_float (1024, fblock1, 17.786);
  TASSERT (block_check (1024, fblock1, 17.786f) == true);
  Bse::Block::fill (1024, fblock1, 17.786f);
  TASSERT (block_check (1024, fblock1, 17.786f) == true);
  Bse::Block::fill (1024, (uint32*) (void*) fblock1, 0);
  TASSERT (block_check (1024, fblock1, 0.f) == true);
  TDONE();
}
static void
test_copy (void)
{
  TSTART ("BlockCopy");
  float fblock1[1024], fblock2[1024];
  Bse::Block::fill (1024, fblock2, -213e+3f);
  TASSERT (block_check (1024, fblock2, -213e+3f) == true);
  Bse::Block::fill (1024, fblock1, -8763e-4f);
  bse_block_copy_float (1024, fblock1, fblock2);
  TASSERT (block_check (1024, fblock1, -213e+3F) == true);
  Bse::Block::fill (1024, fblock1, -8763e-4f);
  bse_block_copy_uint32 (1024, (uint32*) (void*) fblock1, (uint32*) (void*) fblock2);
  TASSERT (block_check (1024, fblock1, -213e+3F) == true);
  Bse::Block::fill (1024, fblock1, -8763e-4f);
  Bse::Block::copy (1024, fblock1, fblock2);
  TASSERT (block_check (1024, fblock1, -213e+3F) == true);
  Bse::Block::fill (1024, fblock1, -8763e-4f);
  Bse::Block::copy (1024, (uint32*) (void*) fblock1, (uint32*) (void*) fblock2);
  TASSERT (block_check (1024, fblock1, -213e+3F) == true);
  TDONE();
}
static void
test_add (void)
{
  TSTART ("BlockAdd");
  float fblock1[1024], fblock2[1024];
  Bse::Block::fill (1024, fblock1, 2.f);
  Bse::Block::fill (1024, fblock2, 3.f);
  bse_block_add_floats (1024, fblock1, fblock2);
  TASSERT (block_check (1024, fblock1, 5.f) == true);
  TASSERT (block_check (1024, fblock2, 3.f) == true);
  Bse::Block::fill (1024, fblock1, 2.f);
  Bse::Block::add (1024, fblock1, fblock2);
  TASSERT (block_check (1024, fblock1, 5.f) == true);
  TASSERT (block_check (1024, fblock2, 3.f) == true);
  TDONE();
}
static void
test_sub (void)
{
  TSTART ("BlockSub");
  float fblock1[1024], fblock2[1024];
  Bse::Block::fill (1024, fblock1, 2.f);
  Bse::Block::fill (1024, fblock2, 3.f);
  bse_block_sub_floats (1024, fblock1, fblock2);
  TASSERT (block_check (1024, fblock1, -1.f) == true);
  TASSERT (block_check (1024, fblock2, 3.f) == true);
  Bse::Block::fill (1024, fblock1, 2.f);
  Bse::Block::sub (1024, fblock1, fblock2);
  TASSERT (block_check (1024, fblock1, -1.f) == true);
  TASSERT (block_check (1024, fblock2, 3.f) == true);
  TDONE();
}
static void
test_mul (void)
{
  TSTART ("BlockMul");
  float fblock1[1024], fblock2[1024];
  Bse::Block::fill (1024, fblock1, 2.f);
  Bse::Block::fill (1024, fblock2, 3.f);
  bse_block_mul_floats (1024, fblock1, fblock2);
  TASSERT (block_check (1024, fblock1, 6.f) == true);
  TASSERT (block_check (1024, fblock2, 3.f) == true);
  Bse::Block::fill (1024, fblock1, 2.f);
  Bse::Block::mul (1024, fblock1, fblock2);
  TASSERT (block_check (1024, fblock1, 6.f) == true);
  TASSERT (block_check (1024, fblock2, 3.f) == true);
  TDONE();
}
static void
test_square_sum (void)
{
  TSTART ("BlockSquareSum");
  float fblock[1024];
  float min_value, max_value;
  for (int i = 0; i < 10; i++)
    {
      float energy, energy_db;
      for (int i = 0; i < 1024; i++)
	fblock[i] = sin (i * 2 * M_PI / 1024);
      energy = bse_block_calc_float_square_sum (1024, fblock) / 1024.;
      energy_db = 10 * log10 (energy);
      TOUT ("sine wave: energy = %f, energy_db = %f\n", energy, energy_db);
      TASSERT (fabs (energy - 0.5) < 0.0000001);
      energy = bse_block_calc_float_range_and_square_sum (1024, fblock, &min_value, &max_value) / 1024.;
      TASSERT (fabs (energy - 0.5) < 0.0000001);
      for (int i = 0; i < 1024; i++)
	fblock[i] = i < 512 ? -1 : 1;
      energy = bse_block_calc_float_square_sum (1024, fblock) / 1024.;
      energy_db = 10 * log10 (energy);
      TOUT ("square wave: energy = %f, energy_db = %f\n", energy, energy_db);
      TASSERT (fabs (energy - 1.0) < 0.0000001);
      energy = bse_block_calc_float_range_and_square_sum (1024, fblock, &min_value, &max_value) / 1024.;
      TASSERT (fabs (energy - 1.0) < 0.0000001);
      /* square sum (and energy) should not depend on ordering of the elements */
      block_shuffle (1024, fblock);
    }
  TDONE();
}
static void
test_range (void)
{
  TSTART ("BlockRange");
  float fblock[1024];
  build_ascending_random_block (1024, fblock);
  float correct_min_value = fblock[0];
  float correct_max_value = fblock[1023];
  for (int i = 0; i < 10; i++)
    {
      /* shuffle block into quasi random order */
      block_shuffle (1024, fblock);
      /* check that correct minimum and maximum is still found */
      float min_value = 0, max_value = 0;
      bse_block_calc_float_range (1024, fblock, &min_value, &max_value);
      TASSERT (min_value == correct_min_value);
      TASSERT (max_value == correct_max_value);
      bse_block_calc_float_range_and_square_sum (1024, fblock, &min_value, &max_value);
      TASSERT (min_value == correct_min_value);
      TASSERT (max_value == correct_max_value);
    }
  TDONE();
}
static void
test_scale (void)
{
  TSTART ("BlockScale");
  float fblock1[1024], fblock2[1024];
  Bse::Block::fill (1024, fblock1, 0.f);
  Bse::Block::fill (1024, fblock2, 3.f);
  bse_block_scale_floats (1024, fblock1, fblock2, 2.f);
  TASSERT (block_check (1024, fblock1, 6.f) == true);
  TASSERT (block_check (1024, fblock2, 3.f) == true);
  Bse::Block::fill (1024, fblock1, 0.f);
  Bse::Block::scale (1024, fblock1, fblock2, 2.f);
  TASSERT (block_check (1024, fblock1, 6.f) == true);
  TASSERT (block_check (1024, fblock2, 3.f) == true);
  TDONE();
}
#define RUNS    11
const int BLOCK_SIZE = 1024;
/*
 * to make benchmarks with different blocksizes comparable,
 * results will be scaled to a standard block size (1024)
 */
const double BENCH_SCALE = 1024. / BLOCK_SIZE;
static inline void
bench_fill (void)
{
  float fblock[BLOCK_SIZE];
  GTimer *timer = g_timer_new();
  g_timer_start (timer);
  const guint dups = TEST_CALIBRATION (50.0, Bse::Block::fill (BLOCK_SIZE, fblock, 2.f));
  g_timer_stop (timer);
  double c = g_timer_elapsed (timer, NULL);

  double m = 9e300;
  for (guint i = 0; i < RUNS; i++)
    {
      g_timer_start (timer);
      for (guint j = 0; j < dups; j++)
        Bse::Block::fill (BLOCK_SIZE, fblock, 2.f);
      g_timer_stop (timer);
      double e = g_timer_elapsed (timer, NULL);
      if (e < m)
        m = e;
    }
  report_usecs ("Block::fill", 1000000.0 * m / dups * BENCH_SCALE);
  if (0)
    printout ("FillBench:            %.6f msecs (test-duration: %.6f calibration: %.6f)\n",
              1000.0 * m / dups * BENCH_SCALE, m * RUNS, c);
}

static inline void
bench_copy (void)
{
  float src_fblock[BLOCK_SIZE], dest_fblock[BLOCK_SIZE];
  Bse::Block::fill (BLOCK_SIZE, src_fblock, 2.f);
  Bse::Block::fill (BLOCK_SIZE, dest_fblock, 0.f);
  GTimer *timer = g_timer_new();
  g_timer_start (timer);
  const guint dups = TEST_CALIBRATION (50.0, Bse::Block::copy (BLOCK_SIZE, dest_fblock, src_fblock));
  g_timer_stop (timer);
  double c = g_timer_elapsed (timer, NULL);

  double m = 9e300;
  for (guint i = 0; i < RUNS; i++)
    {
      g_timer_start (timer);
      for (guint j = 0; j < dups; j++)
        Bse::Block::copy (BLOCK_SIZE, dest_fblock, src_fblock);
      g_timer_stop (timer);
      double e = g_timer_elapsed (timer, NULL);
      if (e < m)
        m = e;
    }
  assert (dest_fblock[0] == 2.f);
  report_usecs ("Block::copy", 1000000.0 * m / dups * BENCH_SCALE);
  if (0)
    printout ("CopyBench:            %.6f msecs (test-duration: %.6f calibration: %.6f)\n",
              1000.0 * m / dups * BENCH_SCALE, m * RUNS, c);
}

static inline void
bench_add (void)
{
  float fblock1[BLOCK_SIZE], fblock2[BLOCK_SIZE];
  Bse::Block::fill (BLOCK_SIZE, fblock1, 2.f);
  Bse::Block::fill (BLOCK_SIZE, fblock2, 3.f);
  GTimer *timer = g_timer_new();
  g_timer_start (timer);
  const guint dups = TEST_CALIBRATION (50.0, Bse::Block::add (BLOCK_SIZE, fblock1, fblock2));
  g_timer_stop (timer);
  double c = g_timer_elapsed (timer, NULL);

  double m = 9e300;
  for (guint i = 0; i < RUNS; i++)
    {
      g_timer_start (timer);
      for (guint j = 0; j < dups; j++)
        Bse::Block::add (BLOCK_SIZE, fblock1, fblock2);
      g_timer_stop (timer);
      double e = g_timer_elapsed (timer, NULL);
      if (e < m)
        m = e;
    }
  report_usecs ("Block::add", 1000000.0 * m / dups * BENCH_SCALE);
  if (0)
    printout ("AddBench:             %.6f msecs (test-duration: %.6f calibration: %.6f)\n",
              1000.0 * m / dups * BENCH_SCALE, m * RUNS, c);
}

static inline void
bench_sub (void)
{
  float fblock1[BLOCK_SIZE], fblock2[BLOCK_SIZE];
  Bse::Block::fill (BLOCK_SIZE, fblock1, 2.f);
  Bse::Block::fill (BLOCK_SIZE, fblock2, 3.f);
  GTimer *timer = g_timer_new();
  g_timer_start (timer);
  const guint dups = TEST_CALIBRATION (50.0, Bse::Block::sub (BLOCK_SIZE, fblock1, fblock2));
  g_timer_stop (timer);
  double c = g_timer_elapsed (timer, NULL);

  double m = 9e300;
  for (guint i = 0; i < RUNS; i++)
    {
      g_timer_start (timer);
      for (guint j = 0; j < dups; j++)
        Bse::Block::sub (BLOCK_SIZE, fblock1, fblock2);
      g_timer_stop (timer);
      double e = g_timer_elapsed (timer, NULL);
      if (e < m)
        m = e;
    }
  report_usecs ("Block::sub", 1000000.0 * m / dups * BENCH_SCALE);
  if (0)
    printout ("SubBench:             %.6f msecs (test-duration: %.6f calibration: %.6f)\n",
              1000.0 * m / dups * BENCH_SCALE, m * RUNS, c);
}

static inline void
bench_mul (void)
{
  float fblock1[BLOCK_SIZE], fblock2[BLOCK_SIZE];
  Bse::Block::fill (BLOCK_SIZE, fblock1, 2.f);
  Bse::Block::fill (BLOCK_SIZE, fblock2, 1.0000001); /* use a small factor to avoid inf after many block multiplications */
  GTimer *timer = g_timer_new();
  g_timer_start (timer);
  const guint dups = TEST_CALIBRATION (50.0, Bse::Block::mul (BLOCK_SIZE, fblock1, fblock2));
  g_timer_stop (timer);
  double c = g_timer_elapsed (timer, NULL);

  double m = 9e300;
  for (guint i = 0; i < RUNS; i++)
    {
      g_timer_start (timer);
      for (guint j = 0; j < dups; j++)
        Bse::Block::mul (BLOCK_SIZE, fblock1, fblock2);
      g_timer_stop (timer);
      double e = g_timer_elapsed (timer, NULL);
      if (e < m)
        m = e;
    }
  assert (fblock1[0] < 1e30); /* not close to infinity */
  report_usecs ("Block::mul", 1000000.0 * m / dups * BENCH_SCALE);
  if (0)
    printout ("MulBench:             %.6f msecs (test-duration: %.6f calibration: %.6f)\n",
              1000.0 * m / dups * BENCH_SCALE, m * RUNS, c);
}

static inline void
bench_scale (void)
{
  float fblock1[BLOCK_SIZE], fblock2[BLOCK_SIZE];
  Bse::Block::fill (BLOCK_SIZE, fblock1, 0.f);
  Bse::Block::fill (BLOCK_SIZE, fblock2, 3.f);
  GTimer *timer = g_timer_new();
  g_timer_start (timer);
  const guint dups = TEST_CALIBRATION (50.0, Bse::Block::scale (BLOCK_SIZE, fblock1, fblock2, 2.f));
  g_timer_stop (timer);
  double c = g_timer_elapsed (timer, NULL);

  double m = 9e300;
  for (guint i = 0; i < RUNS; i++)
    {
      g_timer_start (timer);
      for (guint j = 0; j < dups; j++)
        Bse::Block::scale (BLOCK_SIZE, fblock1, fblock2, 2.f);
      g_timer_stop (timer);
      double e = g_timer_elapsed (timer, NULL);
      if (e < m)
        m = e;
    }
  report_usecs ("Block::scale", 1000000.0 * m / dups * BENCH_SCALE);
  if (0)
    printout ("ScaleBench:           %.6f msecs (test-duration: %.6f calibration: %.6f)\n",
              1000.0 * m / dups * BENCH_SCALE, m * RUNS, c);
}

static inline void
bench_range (void)
{
  float fblock[BLOCK_SIZE];
  build_ascending_random_block (BLOCK_SIZE, fblock);

  float correct_min_value = fblock[0];
  float correct_max_value = fblock[BLOCK_SIZE - 1];
  float min_value, max_value;

  /* shuffle block into quasi random order */
  block_shuffle (BLOCK_SIZE, fblock);

  GTimer *timer = g_timer_new();
  g_timer_start (timer);
  const guint dups = TEST_CALIBRATION (50.0, Bse::Block::range (BLOCK_SIZE, fblock, min_value, max_value));
  g_timer_stop (timer);
  double c = g_timer_elapsed (timer, NULL);

  double m = 9e300;
  for (guint i = 0; i < RUNS; i++)
    {
      g_timer_start (timer);
      for (guint j = 0; j < dups; j++)
        Bse::Block::range (BLOCK_SIZE, fblock, min_value, max_value);
      g_timer_stop (timer);
      double e = g_timer_elapsed (timer, NULL);
      if (e < m)
        m = e;
    }
  assert (min_value == correct_min_value);
  assert (max_value == correct_max_value);
  report_usecs ("Block::range", 1000000.0 * m / dups * BENCH_SCALE);
  if (0)
    printout ("RangeBench:           %.6f msecs (test-duration: %.6f calibration: %.6f)\n",
              1000.0 * m / dups * BENCH_SCALE, m * RUNS, c);
}

static inline void
bench_square_sum (void)
{
  float fblock[BLOCK_SIZE];
  Bse::Block::fill (BLOCK_SIZE, fblock, 2.f);
  GTimer *timer = g_timer_new();
  g_timer_start (timer);
  const guint dups = TEST_CALIBRATION (50.0, Bse::Block::square_sum (BLOCK_SIZE, fblock));
  g_timer_stop (timer);
  double c = g_timer_elapsed (timer, NULL);

  double m = 9e300;
  for (guint i = 0; i < RUNS; i++)
    {
      g_timer_start (timer);
      for (guint j = 0; j < dups; j++)
        Bse::Block::square_sum (BLOCK_SIZE, fblock);
      g_timer_stop (timer);
      double e = g_timer_elapsed (timer, NULL);
      if (e < m)
        m = e;
    }
  report_usecs ("Block::square_sum", 1000000.0 * m / dups * BENCH_SCALE);
  if (0)
    printout ("SquareSumBench:       %.6f msecs (test-duration: %.6f calibration: %.6f)\n",
              1000.0 * m / dups * BENCH_SCALE, m * RUNS, c);
}

static inline void
bench_range_and_square_sum (void)
{
  float fblock[BLOCK_SIZE];
  build_ascending_random_block (BLOCK_SIZE, fblock);

  float correct_min_value = fblock[0];
  float correct_max_value = fblock[BLOCK_SIZE - 1];
  float min_value, max_value;

  /* shuffle block into quasi random order */
  block_shuffle (BLOCK_SIZE, fblock);

  GTimer *timer = g_timer_new();
  g_timer_start (timer);
  const guint dups = TEST_CALIBRATION (50.0, Bse::Block::range_and_square_sum (BLOCK_SIZE, fblock, min_value, max_value));
  g_timer_stop (timer);
  double c = g_timer_elapsed (timer, NULL);

  double m = 9e300;
  for (guint i = 0; i < RUNS; i++)
    {
      g_timer_start (timer);
      for (guint j = 0; j < dups; j++)
        Bse::Block::range_and_square_sum (BLOCK_SIZE, fblock, min_value, max_value);
      g_timer_stop (timer);
      double e = g_timer_elapsed (timer, NULL);
      if (e < m)
        m = e;
    }
  assert (min_value == correct_min_value);
  assert (max_value == correct_max_value);
  report_usecs ("Block::range_and_square_sum", 1000000.0 * m / dups * BENCH_SCALE);
  if (0)
    printout ("Range+SquareSumBench: %.6f msecs (test-duration: %.6f calibration: %.6f)\n",
              1000.0 * m / dups * BENCH_SCALE, m * RUNS, c);
}

static void
run_tests()
{
  test_fill();
  test_copy();
  test_add();
  test_sub();
  test_mul();
  test_scale();
  /* the next two functions test the range_and_square_sum function, too */
  test_range();
  test_square_sum();
  if (Rapicorn::Test::slow())
    {
      bench_fill();
      bench_copy();
      bench_add();
      bench_sub();
      bench_mul();
      bench_scale();
      bench_range();
      bench_square_sum();
      bench_range_and_square_sum();
    }
}

int
main (int   argc,
      char *argv[])
{
  // usually we'd call bse_init_test() here, but we have tests to rnu before plugins are loaded
  Rapicorn::init_core_test (RAPICORN_PRETTY_FILE, &argc, argv);
  Rapicorn::StringVector sv = Rapicorn::string_split (Rapicorn::cpu_info(), " ");
  Rapicorn::String machine = sv.size() >= 2 ? sv[1] : "Unknown";
  TMSG ("  NOTE   Running on: %s+%s", machine.c_str(), bse_block_impl_name()); // usually done by bse_init_test

  TSTART ("Running Default Block Ops");
  TASSERT (Bse::Block::default_singleton() == Bse::Block::current_singleton());
  TDONE();
  run_tests(); /* run tests on FPU */
  /* load plugins */
  bse_init_test (&argc, argv, Bse::cstrings_to_vector ("load-core-plugins=1", NULL));
  /* check for possible specialization */
  if (Bse::Block::default_singleton() == Bse::Block::current_singleton())
    return 0;   /* nothing changed */
  TSTART ("Running Intrinsic Block Ops");
  TASSERT (Bse::Block::default_singleton() != Bse::Block::current_singleton());
  TDONE();
  run_tests(); /* run tests with intrinsics */
  return 0;
}
