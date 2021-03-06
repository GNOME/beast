// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
#ifndef __BSE_MIDI_NOTIFIER_H__
#define __BSE_MIDI_NOTIFIER_H__

#include <bse/bseitem.hh>
#include <bse/bsemidireceiver.hh>


/* --- object type macros --- */
#define BSE_TYPE_MIDI_NOTIFIER              (BSE_TYPE_ID (BseMidiNotifier))
#define BSE_MIDI_NOTIFIER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), BSE_TYPE_MIDI_NOTIFIER, BseMidiNotifier))
#define BSE_MIDI_NOTIFIER_CLASS(class)      (G_TYPE_CHECK_CLASS_CAST ((class), BSE_TYPE_MIDI_NOTIFIER, BseMidiNotifierClass))
#define BSE_IS_MIDI_NOTIFIER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), BSE_TYPE_MIDI_NOTIFIER))
#define BSE_IS_MIDI_NOTIFIER_CLASS(class)   (G_TYPE_CHECK_CLASS_TYPE ((class), BSE_TYPE_MIDI_NOTIFIER))
#define BSE_MIDI_NOTIFIER_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), BSE_TYPE_MIDI_NOTIFIER, BseMidiNotifierClass))

struct BseMidiNotifier : BseItem {
  BseMidiReceiver *midi_receiver;
};
struct BseMidiNotifierClass : BseItemClass {
  void	(*midi_event)	(BseMidiNotifier *self, BseMidiEvent *event);
};

void bse_midi_notifier_set_receiver     (BseMidiNotifier      *self,
                                         BseMidiReceiver      *midi_receiver);
void bse_midi_notifier_dispatch         (BseMidiNotifier      *self);
void bse_midi_notifiers_attach_source   (void);
void bse_midi_notifiers_wakeup          (void);

namespace Bse {

class MidiNotifierImpl : public ItemImpl, public virtual MidiNotifierIface {
protected:
  virtual  ~MidiNotifierImpl ();
public:
  explicit  MidiNotifierImpl (BseObject*);
};

} // Bse

#endif /* __BSE_MIDI_NOTIFIER_H__ */
