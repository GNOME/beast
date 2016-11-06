// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
#ifndef __BSE_IIR_FILTER_H__
#define __BSE_IIR_FILTER_H__
#include <bse/bsesource.hh>
#include <bse/bseenums.hh>
#define BSE_TYPE_IIR_FILTER              (bse_iir_filter_get_type())
#define BSE_IIR_FILTER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), BSE_TYPE_IIR_FILTER, BseIIRFilter))
#define BSE_IIR_FILTER_CLASS(class)      (G_TYPE_CHECK_CLASS_CAST ((class), BSE_TYPE_IIR_FILTER, BseIIRFilterClass))
#define BSE_IS_IIR_FILTER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), BSE_TYPE_IIR_FILTER))
#define BSE_IS_IIR_FILTER_CLASS(class)   (G_TYPE_CHECK_CLASS_TYPE ((class), BSE_TYPE_IIR_FILTER))
#define BSE_IIR_FILTER_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), BSE_TYPE_IIR_FILTER, BseIIRFilterClass))
#define	BSE_IIR_FILTER_MAX_ORDER	 (18)

struct BseIIRFilterVars {
  gdouble a[BSE_IIR_FILTER_MAX_ORDER];
  gdouble b[BSE_IIR_FILTER_MAX_ORDER];
};
struct BseIIRFilter : BseSource {
  BseIIRFilterKind      filter_algo;
  BseIIRFilterType      filter_type;
  guint		        algo_type_change : 1;
  guint		order;
  gdouble	epsilon;
  gfloat	cut_off_freq1;
  gfloat	cut_off_freq2;	/* band pass/stop */
  gdouble	a[BSE_IIR_FILTER_MAX_ORDER + 1];
  gdouble	b[BSE_IIR_FILTER_MAX_ORDER + 1];
};
struct BseIIRFilterClass : BseSourceClass
{};

enum
{
  BSE_IIR_FILTER_ICHANNEL_MONO,
  BSE_IIR_FILTER_N_ICHANNELS
};
enum
{
  BSE_IIR_FILTER_OCHANNEL_MONO,
  BSE_IIR_FILTER_N_OCHANNELS
};

#endif /* __BSE_IIR_FILTER_H__ */
