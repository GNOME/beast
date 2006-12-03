/* BSE - Bedevilled Sound Engine
 * Copyright (C) 2006 Stefan Westerfeld
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <bse/bsedefs.h>
// #define TEST_VERBOSE
#include <sfi/sfitests.h>
#include <bse/gsldatahandle.h>
#include <bse/gsldatautils.h>
#include <bse/bsemain.h>
#include "topconfig.h"
#include <math.h>
#include <complex>
#include <vector>

using std::vector;
using std::min;
using std::max;

static void
read_through (GslDataHandle *handle)
{
  int64 n_values = gsl_data_handle_n_values (handle);
  int64 offset = 0;

  while (offset < n_values)
    {
      // we don't use 1024 here, because we know that it is the FIR handle internal buffer size
      gfloat values[700];
      int64 values_read = gsl_data_handle_read (handle, offset, 700, values);
      g_assert (values_read > 0);
      offset += values_read;
    }

  g_assert (offset == n_values);
}

static double
phase_diff (double p1,
            double p2)
{
  double diff = p1 - p2;

  // normalize to range [-pi..pi]
  while (diff > M_PI)
    diff -= 2 * M_PI;

  while (diff < -M_PI)
    diff += 2 * M_PI;

  return diff;
}

void
test_highpass_with_sine_sweep()
{
  TSTART ("Highpass Handle (sweep)");
  vector<float> sweep_sin (50000);
  vector<float> sweep_cos (50000);
  vector<double> sweep_freq (50000);

  const double start_freq = 50;
  const double end_freq = 24000;
  const double mix_freq = 48000;
  const double octaves = log (end_freq / start_freq) / log (2);

  double phase = 0; 

  for (size_t i = 0; i < sweep_sin.size(); i++)
    {
      sweep_sin[i] = sin (phase);
      sweep_cos[i] = cos (phase);
      sweep_freq[i] = pow (2.0, (i * octaves) / sweep_sin.size()) * start_freq;
      phase += sweep_freq[i] / mix_freq * 2.0 * M_PI;
      if (phase > 2.0 * M_PI)
	phase -= 2.0 * M_PI;
    }

  GslDataHandle *ihandle_sin = gsl_data_handle_new_mem (1, 32, mix_freq, 440, sweep_sin.size(), &sweep_sin[0], NULL);
  GslDataHandle *ihandle_cos = gsl_data_handle_new_mem (1, 32, mix_freq, 440, sweep_cos.size(), &sweep_cos[0], NULL);

  const int order = 64;
  GslDataHandle *fir_handle_sin = bse_data_handle_new_fir_highpass (ihandle_sin, 9000.0, order);
  GslDataHandle *fir_handle_cos = bse_data_handle_new_fir_highpass (ihandle_cos, 9000.0, order);

  BseErrorType error;
  error = gsl_data_handle_open (fir_handle_sin);
  TASSERT (error == 0);
  error = gsl_data_handle_open (fir_handle_cos);
  TASSERT (error == 0);

  GslDataPeekBuffer peek_buffer_sin = { +1 /* incremental direction */, 0, };
  GslDataPeekBuffer peek_buffer_cos = { +1 /* incremental direction */, 0, };

  double stop_min_db = 1e19, stop_max_db = -1e19;
  double trans_min_db = 1e19, trans_max_db = -1e19;
  double pass1_min_db = 1e19, pass1_max_db = -1e19;
  double pass2_min_db = 1e19, pass2_max_db = -1e19;
  double phase_diff_max = 0;
  for (size_t i = ((order + 2) / 2); i < sweep_sin.size() - ((order + 2) / 2); i++)
    {
      double filtered_sin = gsl_data_handle_peek_value (fir_handle_sin, i, &peek_buffer_sin);
      double filtered_cos = gsl_data_handle_peek_value (fir_handle_cos, i, &peek_buffer_cos);
      std::complex<double> filtered (filtered_sin, filtered_cos);
      double level = abs (filtered);
      double level_db = bse_db_from_factor (level, -200);

      // check frequency response
      // printf ("%f %.17g\n", sweep_freq[i], level_db);
      if (sweep_freq[i] < 7050)
	{
	  stop_min_db = min (stop_min_db, level_db);
	  stop_max_db = max (stop_max_db, level_db);
	}
      if (sweep_freq[i] > 7050 && sweep_freq[i] < 9500)
	{
	  trans_min_db = min (trans_min_db, level_db);
	  trans_max_db = max (trans_max_db, level_db);
	}
      if (sweep_freq[i] > 9500 && sweep_freq[i] < 11000)
	{
	  pass1_min_db = min (pass1_min_db, level_db);
	  pass1_max_db = max (pass1_max_db, level_db);
	}
      if (sweep_freq[i] > 11000)
	{
	  pass2_min_db = min (pass2_min_db, level_db);
	  pass2_max_db = max (pass2_max_db, level_db);
	}
      
      // check phase response in passband
      std::complex<double> orig (sweep_sin[i], sweep_cos[i]);
      double abs_phase_diff = fabs (phase_diff (arg (orig), arg (filtered)));
      if (sweep_freq[i] > 11000)
	{
	  phase_diff_max = max (phase_diff_max, abs_phase_diff);
	  // printf ("%f %.17g\n", sweep_freq[i], abs_phase_diff);
	}
    }
#if 0
  printf ("stop = %f..%f dB\n", stop_min_db, stop_max_db);
  printf ("trans = %f..%f dB\n", trans_min_db, trans_max_db);
  printf ("pass1 = %f..%f dB\n", pass1_min_db, pass1_max_db);
  printf ("pass2 = %f..%f dB\n", pass2_min_db, pass2_max_db);
  printf ("max phase diff = %f\n", phase_diff_max);
#endif
  TASSERT (stop_max_db < -75);
  TASSERT (trans_min_db > -77 && trans_max_db < -2.8);
  TASSERT (pass1_min_db > -2.82 && pass1_max_db < -0.002);
  TASSERT (pass2_min_db > -0.004 && pass2_max_db < 0.002);
  TASSERT (phase_diff_max < 0.0002);
  TDONE();

  /* test speed */
  double samples_per_second = 0;
  if (sfi_init_settings().test_perf)
    {
      const guint RUNS = 10;
      GTimer *timer = g_timer_new();
      const guint dups = TEST_CALIBRATION (50.0, read_through (fir_handle_sin));
      
      double m = 9e300;
      for (guint i = 0; i < RUNS; i++)
        {
          g_timer_start (timer);
          for (guint j = 0; j < dups; j++)
            read_through (fir_handle_sin);
          g_timer_stop (timer);
          double e = g_timer_elapsed (timer, NULL);
          if (e < m)
            m = e;
        }
      samples_per_second = sweep_sin.size() / (m / dups);
      treport_maximized ("Highpass O64 mono", samples_per_second, TUNIT (SAMPLE, SECOND));
      treport_maximized ("CPU Highpass mono", samples_per_second / 44100.0, TUNIT_STREAM);
    }
}

double
raised_cosine_fade (int64 pos,
		    int64 length,
		    int64 fade_length)
{
  double fade_delta  = 1.0 / fade_length;
  double fade_factor = fade_delta * min (pos, length - pos);
  if (fade_factor >= 1.0)
    return 1.0;
  else
    return (0.5 - cos (fade_factor * PI) * 0.5);
}

void
test_highpass_multi_channel()
{
  TSTART ("Highpass Handle (multichannel)");
  for (int n_channels = 1; n_channels <= 10; n_channels++)
    {
      const double    mix_freq = 48000;
      const double    cutoff_freq = 7500;
      const double    test_freqs[] = {
	50, 100, 234.567, 557, 901, 1350, 1780, 2345, 3745, 4500,	     // below cutoff
	11000, 12000, 13945, 14753, 15934, 16734, 17943, 18930, 19320, 20940 // above cutoff
      };
      vector<float>   input (2500 * n_channels);
      vector<double>  expected (input.size());
      vector<double>  freq (n_channels);
      vector<double>  phase (n_channels);

      for (int c = 0; c < n_channels; c++)
	freq[c] = test_freqs [g_random_int_range (0, sizeof (test_freqs) / sizeof (test_freqs[0]))];

      for (size_t i = 0; i < input.size(); i++)
	{
	  const int	c           = i % n_channels;
	  const double  fade_factor = raised_cosine_fade (i / n_channels, input.size() / n_channels, 500);
	  const double  invalue     = sin (phase[c]) * fade_factor;

	  input[i] = invalue;
	  expected[i] = (freq[c] > cutoff_freq) ? invalue : 0.0;

	  phase[c] += freq[c] / mix_freq * 2.0 * M_PI;
	  if (phase[c] > 2.0 * M_PI)
	    phase[c] -= 2.0 * M_PI;
	}

      GslDataHandle *ihandle = gsl_data_handle_new_mem (n_channels, 32, mix_freq, 440, input.size(), &input[0], NULL);
      const int order = 116;
      GslDataHandle *fir_handle = bse_data_handle_new_fir_highpass (ihandle, cutoff_freq, order);

      BseErrorType error;
      error = gsl_data_handle_open (fir_handle);
      TASSERT (error == 0);

      for (int repeat = 1; repeat <= 2; repeat++)
	{
	  GslDataPeekBuffer peek_buffer = { +1 /* incremental direction */, 0, };
	  double worst_diff = 0.0;
	  for (int64 i = 0; i < fir_handle->setup.n_values; i++)
	    {
	      double filtered = gsl_data_handle_peek_value (fir_handle, i, &peek_buffer);
	      worst_diff = max (filtered - expected[i], worst_diff);
	    }
	  double worst_diff_db = bse_db_from_factor (worst_diff, -200);
	  TPRINT ("n_channels = %d: linear(%dst read) read worst_diff = %f (%f dB)\n",
	          n_channels, repeat, worst_diff, worst_diff_db);
	  TASSERT (worst_diff_db < -90);
	}
    }
  TDONE();
}

int
main (int    argc,
      char **argv)
{
  bse_init_test (&argc, &argv, NULL);
  test_highpass_with_sine_sweep();
  test_highpass_multi_channel();
  return 0;
}