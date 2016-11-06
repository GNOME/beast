// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
#ifndef __BSE_MIDI_DEVICE_H__
#define __BSE_MIDI_DEVICE_H__

#include        <bse/bsedevice.hh>
#include        <bse/bsemidievent.hh>

/* --- object type macros --- */
#define BSE_TYPE_MIDI_DEVICE              (BSE_TYPE_ID (BseMidiDevice))
#define BSE_MIDI_DEVICE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), BSE_TYPE_MIDI_DEVICE, BseMidiDevice))
#define BSE_MIDI_DEVICE_CLASS(class)      (G_TYPE_CHECK_CLASS_CAST ((class), BSE_TYPE_MIDI_DEVICE, BseMidiDeviceClass))
#define BSE_IS_MIDI_DEVICE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), BSE_TYPE_MIDI_DEVICE))
#define BSE_IS_MIDI_DEVICE_CLASS(class)   (G_TYPE_CHECK_CLASS_TYPE ((class), BSE_TYPE_MIDI_DEVICE))
#define BSE_MIDI_DEVICE_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), BSE_TYPE_MIDI_DEVICE, BseMidiDeviceClass))

struct BseMidiHandle	/* this should be nuked, it's useless */
{
  guint			 readable : 1;
  guint			 writable : 1;
  guint			 running_thread : 1;
  BseMidiDecoder	*midi_decoder;
};
struct BseMidiDevice : BseDevice {
  BseMidiDecoder	*midi_decoder;
  /* operational handle */
  BseMidiHandle	        *handle;
};
struct BseMidiDeviceClass : BseDeviceClass
{};

void		bse_midi_handle_init		(BseMidiHandle		*handle);
#endif /* __BSE_MIDI_DEVICE_H__ */
