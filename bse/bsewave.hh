// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
#ifndef __BSE_WAVE_H__
#define __BSE_WAVE_H__

#include	<bse/bsesource.hh>

G_BEGIN_DECLS

/* --- BSE type macros --- */
#define BSE_TYPE_WAVE		   (BSE_TYPE_ID (BseWave))
#define BSE_WAVE(object)	   (G_TYPE_CHECK_INSTANCE_CAST ((object), BSE_TYPE_WAVE, BseWave))
#define BSE_WAVE_CLASS(class)	   (G_TYPE_CHECK_CLASS_CAST ((class), BSE_TYPE_WAVE, BseWaveClass))
#define BSE_IS_WAVE(object)	   (G_TYPE_CHECK_INSTANCE_TYPE ((object), BSE_TYPE_WAVE))
#define BSE_IS_WAVE_CLASS(class)   (G_TYPE_CHECK_CLASS_TYPE ((class), BSE_TYPE_WAVE))
#define BSE_WAVE_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), BSE_TYPE_WAVE, BseWaveClass))

struct BseWaveEntry {
  GslWaveChunk *wchunk;
  gfloat        osc_freq;
  gfloat        velocity; /* 0..1 */
};
struct BseWaveIndex {
  guint		n_entries;
  BseWaveEntry  entries[1];     /* flexible array */
};
struct BseWave : BseSource {
  /* requested BseModule indices */
  guint		     request_count;
  GSList	    *index_list;
  guint		     index_dirty : 1;
  /* locator */
  guint		     locator_set : 1;
  gchar		    *file_name;
  gchar		    *wave_name;
  /* wave data */
  gchar            **xinfos;
  /* wave chunks */
  guint		     n_wchunks;
  SfiRing           *wave_chunks;       /* data=GslWaveChunk* */
  SfiRing           *open_handles;      /* data=GslDataHandle* */
};
struct BseWaveClass : BseSourceClass
{};

void		bse_wave_clear                  (BseWave	*wave);
Bse::ErrorType	bse_wave_load_wave_file		(BseWave	*wave,
						 const gchar	*file_name,
						 const gchar	*wave_name,
						 BseFreqArray	*list_array,
						 BseFreqArray	*skip_array,
                                                 gboolean        rename_wave);
void		bse_wave_add_chunk		(BseWave	*wave,
						 GslWaveChunk	*wchunk);
GslWaveChunk*   bse_wave_lookup_chunk           (BseWave        *wave,
						 gfloat		 mix_freq,
						 gfloat		 osc_freq,
                                                 gfloat          velocity);
void            bse_wave_remove_chunk           (BseWave        *wave,
						 GslWaveChunk   *wchunk);
void		bse_wave_request_index		(BseWave	*wave);
BseWaveIndex*	bse_wave_get_index_for_modules	(BseWave	*wave);
void		bse_wave_drop_index		(BseWave	*wave);

/* BseWaveIndex is safe to use from BseModules (self-contained constant structure) */
GslWaveChunk*	bse_wave_index_lookup_best	(BseWaveIndex	*windex,
						 gfloat		 osc_freq,
                                                 gfloat          velocity);

G_END_DECLS

namespace Bse {

class WaveImpl : public SourceImpl, public virtual WaveIface {
protected:
  virtual  ~WaveImpl ();
public:
  explicit  WaveImpl (BseObject*);
};

} // Bse

#endif /* __BSE_WAVE_H__ */
