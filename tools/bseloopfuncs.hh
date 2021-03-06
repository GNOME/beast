// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
#ifndef __BSE_LOOPFUNCS_H__
#define __BSE_LOOPFUNCS_H__

#include <bse/gsldatautils.hh>
#include <bse/gslcommon.hh>


typedef struct {
  /* block containing loop */
  GslLong       block_start;
  GslLong       block_length;
  /* performance/quality parameters */
  GslLong       analysis_points;
  /* assumed repetitions within block */
  GslLong       repetitions;
  GslLong       min_loop;
  /* resulting loop */
  gdouble       score;
  GslLong       loop_start;
  GslLong       loop_length;
  /* score details */
  guint         n_details;
  const char   *detail_names[64];
  double        detail_scores[64];
} GslDataLoopConfig;

/* mem-cached loop position and size finder. tests through all possible
 * loop sizes around center points determined by block/(analysis_points+1).
 * uses full-block comparisons (centering comparison area around the
 * loop) and tight neighbourhood comparisons. the full-block and
 * neighbourhood compraisons are normalized by the sample count to produce
 * a single error score. the progress counter is slightly off and will
 * often count to values lesser than 100%.
 */
gboolean        gsl_data_find_loop5             (GslDataHandle          *dhandle,
                                                 GslDataLoopConfig      *config,
                                                 gpointer                pdata,
                                                 GslProgressFunc         pfunc);
/* mem-cached loop position and size finder. tests through all possible
 * loop sizes around center points determined by block/(analysis_points+1).
 * uses full-block comparisons (centering comparison area around the
 * loop) and can produce non-acceptable results. looping 6 seconds in
 * 61 samples runs roughly 6 hours on 2500MHz.
 */
gboolean        gsl_data_find_loop4             (GslDataHandle          *dhandle,
                                                 GslDataLoopConfig      *config,
                                                 gpointer                pdata,
                                                 GslProgressFunc         pfunc);
/* fully mem-cached loop position/size finder.
 * attempts to determine optimum loop position and size by trying all
 * possible combinations (n^2) and doing a full block comparison on
 * each (*n). performance is n^3 and not suitable for practical
 * application. (and even then, full block comparisons are not a good
 * enough criterion to always reveal acceptable loop transitions).
 * 44100*3=132300, 132300*132300=17503290000
 */
gboolean        gsl_data_find_loop3             (GslDataHandle     *dhandle,
                                                 GslDataLoopConfig *config,
                                                 gpointer           pdata,
                                                 GslProgressFunc    pfunc);
/* quick hack to dump gnuplot file with diffenrent loop
 * positions or loop sizes
 */
gboolean        gsl_data_find_loop2             (GslDataHandle     *dhandle,
                                                 GslDataLoopConfig *config,
                                                 gpointer           pdata,
                                                 GslProgressFunc    pfunc);
/* dcache based head-compare loop finder,
 * 1) finds optimum loop length
 * 2) find optimum loop position
 * 3) reruns loop length finder around positions
 *    with shrinking neighbourhood comparisons
 */
gboolean        gsl_data_find_loop1              (GslDataHandle     *dhandle,
                                                  GslDataLoopConfig *config,
                                                  gpointer           pdata,
                                                  GslProgressFunc    pfunc);


typedef enum
{
  GSL_DATA_TAIL_LOOP_CMP_LEAST_SQUARE,
  GSL_DATA_TAIL_LOOP_CMP_CORRELATION,
} GslDataTailLoopCmpType;
typedef struct {
  GslLong	         min_loop;		/* minimum size */
  GslLong	         max_loop;		/* maximum size (-1 for n_values) */
  GslLong	         pre_loop_compare;	/* area size */
  GslDataTailLoopCmpType cmp_strategy;
  /* |-----|----------------|-------------------------------|
   *        pre_loop_compare loop area (min_loop..max_loop)
   *                        ^                               ^
   *                     loop_start                      loop_end
   */
} GslDataTailLoop;
/* dcache based tail loop finder derived from code from stefan */
gdouble		gsl_data_find_loop0		(GslDataHandle          *dhandle,
						 const GslDataTailLoop	*cfg,
						 GslLong                *loop_start_p,
						 GslLong                *loop_end_p);
/* gsl_data_find_loop0(): good loops, bad size:
 * # Tail Loop Finding
 * #  --loop <cfg>  loop finder configuration, consisting of ':'-seperated values:
 * #                n_samples indicating minimum loop size [4410]
 * #                n_samples indicating maximum loop size [-1]
 * #                n_samples compare area size (ahead of loop start) [8820]
 * TL_CFG="--loop=5000:-1:5000"
 */

#endif  /* __BSE_LOOPFUNCS_H__ */
