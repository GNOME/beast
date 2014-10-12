// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
#ifndef __BSE_UTILS_H__
#define __BSE_UTILS_H__

#include <bse/bseclientapi.hh>
#include <rapicorn-core.hh>

namespace Bse {

/// IDL API base class until Rapicorn supports ImplicitBaseP out of the box.
class ImplicitBase : public virtual Rapicorn::Aida::ImplicitBase,
                     public virtual std::enable_shared_from_this<ImplicitBase> {
public:
  template<class Class, typename std::enable_if<std::is_base_of<ImplicitBase, Class>::value>::type* = nullptr>
  static std::shared_ptr<Class> shared_ptr (Class *object) ///< Wrap ImplicitBase or derived type into a std::shared_ptr<>().
  {
    return object ? std::shared_ptr<Class> (object->shared_from_this()) : std::shared_ptr<Class>();
  }
};

} // Bse

#include <bse/bseserverapi.hh>

#include <bse/bseenums.hh>
#include <bse/bseglobals.hh>
#include <bse/bsecompat.hh>

G_BEGIN_DECLS

/* --- C++ helper declaration --- */
void    bse_cxx_init      (void);
/* --- record utils --- */
BseNoteDescription* bse_note_description             (BseMusicalTuningType   musical_tuning,
                                                      int                    note,
                                                      int                    fine_tune);
BsePartNote*        bse_part_note                    (guint                  id,
                                                      guint                  channel,
                                                      guint                  tick,
                                                      guint                  duration,
                                                      gint                   note,
                                                      gint                   fine_tune,
                                                      gfloat                 velocity,
                                                      gboolean               selected);
void                bse_part_note_seq_take_append    (BsePartNoteSeq        *seq,
                                                      BsePartNote           *element);
BsePartControl*     bse_part_control                 (guint                  id,
                                                      guint                  tick,
                                                      BseMidiSignalType      ctype,
                                                      gfloat                 value,
                                                      gboolean               selected);
void                bse_part_control_seq_take_append (BsePartControlSeq     *seq,
                                                      BsePartControl        *element);
void                bse_note_sequence_resize         (BseNoteSequence       *rec,
                                                      guint                  length);
guint               bse_note_sequence_length         (BseNoteSequence       *rec);
void                bse_property_candidate_relabel   (BsePropertyCandidates *pc,
                                                      const gchar           *label,
                                                      const gchar           *tooltip);
void                bse_item_seq_remove              (BseItemSeq            *iseq,
                                                      BseItem               *item);
SfiRing*            bse_item_seq_to_ring             (BseItemSeq            *iseq);
BseItemSeq*         bse_item_seq_from_ring           (SfiRing               *ring);


/* --- debugging --- */
void    bse_debug_dump_floats   (guint   debug_stream,
                                 guint   n_channels,
                                 guint   mix_freq,
                                 guint   n_values,
                                 gfloat *values);


/* --- balance calculation --- */
/* levels are 0..100, balance is -100..+100 */
double  bse_balance_get         (double  level1,
                                 double  level2);
void    bse_balance_set         (double  balance,
                                 double *level1,
                                 double *level2);


/* --- icons --- */
BseIcon* bse_icon_from_pixstream (const guint8     *pixstream);


/* --- ID allocator --- */
gulong	bse_id_alloc	(void);
void	bse_id_free	(gulong	id);


/* --- string array manipulation --- */
gchar**       bse_xinfos_add_value              (gchar          **xinfos,
                                                 const gchar     *key,
                                                 const gchar     *value);
gchar**       bse_xinfos_add_float              (gchar          **xinfos,
                                                 const gchar     *key,
                                                 gfloat           fvalue);
gchar**       bse_xinfos_add_num                (gchar          **xinfos,
                                                 const gchar     *key,
                                                 SfiNum           num);
gchar**       bse_xinfos_parse_assignment       (gchar          **xinfos,
                                                 const gchar     *assignment);
gchar**       bse_xinfos_del_value              (gchar          **xinfos,
                                                 const gchar     *key);
const gchar*  bse_xinfos_get_value              (gchar          **xinfos,
                                                 const gchar     *key);
gfloat        bse_xinfos_get_float              (gchar          **xinfos,
                                                 const gchar     *key);
SfiNum        bse_xinfos_get_num                (gchar          **xinfos,
                                                 const gchar     *key);
gchar**       bse_xinfos_dup_consolidated       (gchar          **xinfos,
                                                 gboolean         copy_interns);
gint          bse_xinfo_stub_compare            (const gchar     *xinfo1,  /* must contain '=' */
                                                 const gchar     *xinfo2); /* must contain '=' */


/* --- miscellaeous --- */
guint		bse_string_hash			(gconstpointer   string);
gint		bse_string_equals		(gconstpointer	 string1,
						 gconstpointer	 string2);
G_END_DECLS

#endif /* __BSE_UTILS_H__ */
