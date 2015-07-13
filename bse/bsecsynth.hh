// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
#ifndef __BSE_CSYNTH_H__
#define __BSE_CSYNTH_H__

#include        <bse/bsesnet.hh>

G_BEGIN_DECLS

#define BSE_TYPE_CSYNTH              (BSE_TYPE_ID (BseCSynth))
#define BSE_CSYNTH(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), BSE_TYPE_CSYNTH, BseCSynth))
#define BSE_CSYNTH_CLASS(class)      (G_TYPE_CHECK_CLASS_CAST ((class), BSE_TYPE_CSYNTH, BseCSynthClass))
#define BSE_IS_CSYNTH(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), BSE_TYPE_CSYNTH))
#define BSE_IS_CSYNTH_CLASS(class)   (G_TYPE_CHECK_CLASS_TYPE ((class), BSE_TYPE_CSYNTH))
#define BSE_CSYNTH_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), BSE_TYPE_CSYNTH, BseCSynthClass))
#define BSE_CSYNTH_USER_SYNTH(src)   ((BSE_OBJECT_FLAGS (src) & BSE_CSYNTH_FLAG_USER_SYNTH) != 0)

struct BseCSynth : BseSNet {};
struct BseCSynthClass : BseSNetClass {};

G_END_DECLS

namespace Bse {

class CSynthImpl : public SNetImpl, public virtual CSynthIface {
protected:
  virtual    ~CSynthImpl                ();
public:
  explicit    CSynthImpl                (BseObject*);
};

} // Bse


#endif /* __BSE_CSYNTH_H__ */
