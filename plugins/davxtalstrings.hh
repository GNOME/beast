// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
#ifndef __DAV_XTAL_STRINGS_H__
#define __DAV_XTAL_STRINGS_H__
#include <bse/bseplugin.hh>
#include <bse/bsesource.hh>
#define DAV_TYPE_XTAL_STRINGS		   (dav_xtal_strings_get_type())
#define DAV_XTAL_STRINGS(object)	   (G_TYPE_CHECK_INSTANCE_CAST ((object), DAV_TYPE_XTAL_STRINGS, DavXtalStrings))
#define DAV_XTAL_STRINGS_CLASS(class)	   (G_TYPE_CHECK_CLASS_CAST ((class), DAV_TYPE_XTAL_STRINGS, DavXtalStringsClass))
#define DAV_IS_XTAL_STRINGS(object)	   (G_TYPE_CHECK_INSTANCE_TYPE ((object), DAV_TYPE_XTAL_STRINGS))
#define DAV_IS_XTAL_STRINGS_CLASS(class)   (G_TYPE_CHECK_CLASS_TYPE ((class), DAV_TYPE_XTAL_STRINGS))
#define DAV_XTAL_STRINGS_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), DAV_TYPE_XTAL_STRINGS, DavXtalStringsClass))

struct DavXtalStringsParams {
  double      transpose_factor;
  gfloat      freq;
  gfloat      trigger_vel;
  gfloat      note_decay;
  gfloat      tension_decay;
  gfloat      metallic_factor;
  gfloat      snap_factor;
  gint        fine_tune;
  guint	      trigger_now : 1;
};

struct DavXtalStrings : BseSource {
  DavXtalStringsParams params;
  int                  transpose;
};
struct DavXtalStringsClass : BseSourceClass
{};

enum
{
  DAV_XTAL_STRINGS_ICHANNEL_FREQ,
  DAV_XTAL_STRINGS_ICHANNEL_TRIGGER,
  DAV_XTAL_STRINGS_N_ICHANNELS
};
enum
{
  DAV_XTAL_STRINGS_OCHANNEL_MONO,
  DAV_XTAL_STRINGS_N_OCHANNELS
};



#endif /* __DAV_XTAL_STRINGS_H__ */
