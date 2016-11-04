// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
#include "bsepcmwriter.hh"
#include "bseserver.hh"
#include "gsldatautils.hh"
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
/* --- prototypes --- */
static void	   bse_pcm_writer_init			(BsePcmWriter      *pdev);
static void	   bse_pcm_writer_class_init		(BsePcmWriterClass *klass);
static void	   bse_pcm_writer_finalize		(GObject           *object);
/* --- variables --- */
static gpointer parent_class = NULL;
/* --- functions --- */
BSE_BUILTIN_TYPE (BsePcmWriter)
{
  static const GTypeInfo pcm_writer_info = {
    sizeof (BsePcmWriterClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) bse_pcm_writer_class_init,
    (GClassFinalizeFunc) NULL,
    NULL /* class_data */,
    sizeof (BsePcmWriter),
    0 /* n_preallocs */,
    (GInstanceInitFunc) bse_pcm_writer_init,
  };
  return bse_type_register_static (BSE_TYPE_ITEM,
				   "BsePcmWriter",
				   "PCM writer",
                                   __FILE__, __LINE__,
                                   &pcm_writer_info);
}
static void
bse_pcm_writer_class_init (BsePcmWriterClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  parent_class = g_type_class_peek_parent (klass);
  gobject_class->finalize = bse_pcm_writer_finalize;
}

static void
bse_pcm_writer_init (BsePcmWriter *self)
{
  new (&self->mutex) Bse::Mutex();
}

static void
bse_pcm_writer_finalize (GObject *object)
{
  BsePcmWriter *self = BSE_PCM_WRITER (object);
  if (self->open)
    {
      g_warning ("%s: pcm writer still opened", G_STRLOC);
      bse_pcm_writer_close (self);
    }
  /* chain parent class' handler */
  G_OBJECT_CLASS (parent_class)->finalize (object);
  self->mutex.~Mutex();
}

Bse::Error
bse_pcm_writer_open (BsePcmWriter *self,
		     const gchar  *file,
		     guint         n_channels,
		     guint         sample_freq,
                     uint64        recorded_maximum)
{
  gint fd;
  assert_return (BSE_IS_PCM_WRITER (self), Bse::Error::INTERNAL);
  assert_return (!self->open, Bse::Error::INTERNAL);
  assert_return (file != NULL, Bse::Error::INTERNAL);
  assert_return (n_channels > 0, Bse::Error::INTERNAL);
  assert_return (sample_freq >= 1000, Bse::Error::INTERNAL);
  self->mutex.lock();
  self->n_bytes = 0;
  self->recorded_maximum = recorded_maximum;
  fd = open (file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
    {
      self->mutex.unlock();
      return bse_error_from_errno (errno, Bse::Error::FILE_OPEN_FAILED);
    }

  errno = bse_wave_file_dump_header (fd, 0x7fff0000, 16, n_channels, sample_freq);
  if (errno)
    {
      close (fd);
      self->mutex.unlock();
      return bse_error_from_errno (errno, Bse::Error::FILE_OPEN_FAILED);
    }
  self->fd = fd;
  self->open = TRUE;
  self->broken = FALSE;
  self->mutex.unlock();
  return Bse::Error::NONE;
}
void
bse_pcm_writer_close (BsePcmWriter *self)
{
  assert_return (BSE_IS_PCM_WRITER (self));
  assert_return (self->open);
  self->mutex.lock();
  bse_wave_file_patch_length (self->fd, self->n_bytes);
  close (self->fd);
  self->fd = -1;
  self->open = FALSE;
  self->mutex.unlock();
  errno = 0;
}

static gboolean
bsethread_halt_recording (gpointer data)
{
  bse_server_stop_recording (bse_server_get());
  return false;
}

void
bse_pcm_writer_write (BsePcmWriter *self,
		      gsize         n_values,
		      const gfloat *values)
{
  assert_return (BSE_IS_PCM_WRITER (self));
  assert_return (self->open);
  if (n_values)
    assert_return (values != NULL);
  else
    return;
  self->mutex.lock();
  const uint bw = 2; /* 16bit */
  if (!self->broken && (!self->recorded_maximum || self->n_bytes < bw * self->recorded_maximum))
    {
      guint j;
      guint8 *dest = g_new (guint8, n_values * bw);
      uint n_bytes = gsl_conv_from_float_clip (GSL_WAVE_FORMAT_SIGNED_16,
                                               G_BYTE_ORDER,
                                               values,
                                               dest,
                                               n_values);
      if (self->recorded_maximum)
        n_bytes = bw * MIN (n_bytes / bw, self->recorded_maximum - self->n_bytes / bw);
      do
	j = write (self->fd, dest, n_bytes);
      while (j < 0 && errno == EINTR);
      if (j > 0)
        {
          self->n_bytes += j;
          if (self->recorded_maximum && self->n_bytes >= bw * self->recorded_maximum)
            bse_idle_next (bsethread_halt_recording, NULL);
        }
      g_free (dest);
      if (j < 0 && errno)
	{
 	  sfi_diag ("failed to write %u bytes to WAV file: %s", n_bytes, g_strerror (errno));
	  self->broken = TRUE;
	}
    }
  self->mutex.unlock();
}

namespace Bse {

PcmWriterImpl::PcmWriterImpl (BseObject *bobj) :
  ItemImpl (bobj)
{}

PcmWriterImpl::~PcmWriterImpl ()
{}

} // Bse
