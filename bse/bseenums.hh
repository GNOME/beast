// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
#ifndef __BSE_ENUMS_H__
#define __BSE_ENUMS_H__

#include <bse/gsldefs.hh>
#include <bse/bsetype.hh>
#include <bse/bseserverapi.hh>


G_BEGIN_DECLS


/* --- enum definitions --- */
typedef enum
{
  BSE_IIR_FILTER_BUTTERWORTH = 1,
  BSE_IIR_FILTER_CHEBYCHEFF1,
  BSE_IIR_FILTER_CHEBYCHEFF2
} BseIIRFilterAlgorithm;
typedef enum
{
  BSE_IIR_FILTER_LOW_PASS = 1,
  BSE_IIR_FILTER_HIGH_PASS,
  BSE_IIR_FILTER_BAND_PASS,
  BSE_IIR_FILTER_BAND_STOP
} BseIIRFilterType;
typedef enum
{
  BSE_REGISTER_PLUGIN   = 1,
  BSE_REGISTER_SCRIPT   = 2,
  BSE_REGISTER_DONE	= 256
} BseRegistrationType;
typedef enum
{
  BSE_INTERPOL_NONE,		/*< nick=None >*/
  BSE_INTERPOL_LINEAR,		/*< nick=Linear >*/
  BSE_INTERPOL_CUBIC		/*< nick=Cubic >*/
} BseInterpolType;
typedef enum
{
  BSE_LOOP_NONE,
  BSE_LOOP_PATTERN,
  BSE_LOOP_PATTERN_ROWS,
  BSE_LOOP_SONG,
  BSE_LOOP_LAST				/*< skip >*/
} BseLoopType;
typedef enum
{
  BSE_MAGIC_BSE_BIN_EXTENSION   = 1 << 0,
  BSE_MAGIC_BSE_SONG            = 1 << 2
} BseMagicFlags;
typedef enum
{
  BSE_ERROR_NONE		                = Bse::ERROR_NONE,
  BSE_ERROR_INTERNAL                            = Bse::ERROR_INTERNAL,
  BSE_ERROR_UNKNOWN		                = Bse::ERROR_UNKNOWN,
  /* general errors */
  BSE_ERROR_IO		                        = Bse::ERROR_IO,
  BSE_ERROR_PERMS		                = Bse::ERROR_PERMS,
  /* file errors */
  BSE_ERROR_FILE_BUSY		                = Bse::ERROR_FILE_BUSY,
  BSE_ERROR_FILE_EXISTS		                = Bse::ERROR_FILE_EXISTS,
  BSE_ERROR_FILE_EOF		                = Bse::ERROR_FILE_EOF,
  BSE_ERROR_FILE_EMPTY		                = Bse::ERROR_FILE_EMPTY,
  BSE_ERROR_FILE_NOT_FOUND		        = Bse::ERROR_FILE_NOT_FOUND,
  BSE_ERROR_FILE_IS_DIR		                = Bse::ERROR_FILE_IS_DIR,
  BSE_ERROR_FILE_OPEN_FAILED		        = Bse::ERROR_FILE_OPEN_FAILED,
  BSE_ERROR_FILE_SEEK_FAILED		        = Bse::ERROR_FILE_SEEK_FAILED,
  BSE_ERROR_FILE_READ_FAILED		        = Bse::ERROR_FILE_READ_FAILED,
  BSE_ERROR_FILE_WRITE_FAILED		        = Bse::ERROR_FILE_WRITE_FAILED,
  /* out of resource conditions */
  BSE_ERROR_MANY_FILES		                = Bse::ERROR_MANY_FILES,
  BSE_ERROR_NO_FILES		                = Bse::ERROR_NO_FILES,
  BSE_ERROR_NO_SPACE		                = Bse::ERROR_NO_SPACE,
  BSE_ERROR_NO_MEMORY		                = Bse::ERROR_NO_MEMORY,
  /* content errors */
  BSE_ERROR_NO_HEADER		                = Bse::ERROR_NO_HEADER,
  BSE_ERROR_NO_SEEK_INFO	        	= Bse::ERROR_NO_SEEK_INFO,
  BSE_ERROR_NO_DATA             		= Bse::ERROR_NO_DATA,
  BSE_ERROR_DATA_CORRUPT	        	= Bse::ERROR_DATA_CORRUPT,
  BSE_ERROR_WRONG_N_CHANNELS	        	= Bse::ERROR_WRONG_N_CHANNELS,
  BSE_ERROR_FORMAT_INVALID      		= Bse::ERROR_FORMAT_INVALID,
  BSE_ERROR_FORMAT_UNKNOWN	        	= Bse::ERROR_FORMAT_UNKNOWN,
  BSE_ERROR_DATA_UNMATCHED      		= Bse::ERROR_DATA_UNMATCHED,
  /* miscellaneous errors */
  BSE_ERROR_TEMP	                	= Bse::ERROR_TEMP,
  BSE_ERROR_WAVE_NOT_FOUND	        	= Bse::ERROR_WAVE_NOT_FOUND,
  BSE_ERROR_CODEC_FAILURE	        	= Bse::ERROR_CODEC_FAILURE,
  BSE_ERROR_UNIMPLEMENTED	        	= Bse::ERROR_UNIMPLEMENTED,
  BSE_ERROR_INVALID_PROPERTY    		= Bse::ERROR_INVALID_PROPERTY,
  BSE_ERROR_INVALID_MIDI_CONTROL		= Bse::ERROR_INVALID_MIDI_CONTROL,
  BSE_ERROR_PARSE_ERROR	                 	= Bse::ERROR_PARSE_ERROR,
  BSE_ERROR_SPAWN	        	        = Bse::ERROR_SPAWN,
  /* Device errors */
  BSE_ERROR_DEVICE_NOT_AVAILABLE		= Bse::ERROR_DEVICE_NOT_AVAILABLE,
  BSE_ERROR_DEVICE_ASYNC	        	= Bse::ERROR_DEVICE_ASYNC,
  BSE_ERROR_DEVICE_BUSY         		= Bse::ERROR_DEVICE_BUSY,
  BSE_ERROR_DEVICE_FORMAT       		= Bse::ERROR_DEVICE_FORMAT,
  BSE_ERROR_DEVICE_BUFFER       		= Bse::ERROR_DEVICE_BUFFER,
  BSE_ERROR_DEVICE_LATENCY      		= Bse::ERROR_DEVICE_LATENCY,
  BSE_ERROR_DEVICE_CHANNELS     		= Bse::ERROR_DEVICE_CHANNELS,
  BSE_ERROR_DEVICE_FREQUENCY    		= Bse::ERROR_DEVICE_FREQUENCY,
  BSE_ERROR_DEVICES_MISMATCH    		= Bse::ERROR_DEVICES_MISMATCH,
  /* BseSource errors */
  BSE_ERROR_SOURCE_NO_SUCH_MODULE		= Bse::ERROR_SOURCE_NO_SUCH_MODULE,
  BSE_ERROR_SOURCE_NO_SUCH_ICHANNEL		= Bse::ERROR_SOURCE_NO_SUCH_ICHANNEL,
  BSE_ERROR_SOURCE_NO_SUCH_OCHANNEL		= Bse::ERROR_SOURCE_NO_SUCH_OCHANNEL,
  BSE_ERROR_SOURCE_NO_SUCH_CONNECTION		= Bse::ERROR_SOURCE_NO_SUCH_CONNECTION,
  BSE_ERROR_SOURCE_PRIVATE_ICHANNEL		= Bse::ERROR_SOURCE_PRIVATE_ICHANNEL,
  BSE_ERROR_SOURCE_ICHANNEL_IN_USE		= Bse::ERROR_SOURCE_ICHANNEL_IN_USE,
  BSE_ERROR_SOURCE_CHANNELS_CONNECTED		= Bse::ERROR_SOURCE_CHANNELS_CONNECTED,
  BSE_ERROR_SOURCE_CONNECTION_INVALID		= Bse::ERROR_SOURCE_CONNECTION_INVALID,
  BSE_ERROR_SOURCE_PARENT_MISMATCH		= Bse::ERROR_SOURCE_PARENT_MISMATCH,
  BSE_ERROR_SOURCE_BAD_LOOPBACK		        = Bse::ERROR_SOURCE_BAD_LOOPBACK,
  BSE_ERROR_SOURCE_BUSY		                = Bse::ERROR_SOURCE_BUSY,
  BSE_ERROR_SOURCE_TYPE_INVALID		        = Bse::ERROR_SOURCE_TYPE_INVALID,
  /* BseProcedure errors */
  BSE_ERROR_PROC_NOT_FOUND		        = Bse::ERROR_PROC_NOT_FOUND,
  BSE_ERROR_PROC_BUSY		                = Bse::ERROR_PROC_BUSY,
  BSE_ERROR_PROC_PARAM_INVAL		        = Bse::ERROR_PROC_PARAM_INVAL,
  BSE_ERROR_PROC_EXECUTION		        = Bse::ERROR_PROC_EXECUTION,
  BSE_ERROR_PROC_ABORT		                = Bse::ERROR_PROC_ABORT,
  /* various procedure errors */
  BSE_ERROR_NO_ENTRY		                = Bse::ERROR_NO_ENTRY,
  BSE_ERROR_NO_EVENT		                = Bse::ERROR_NO_EVENT,
  BSE_ERROR_NO_TARGET		                = Bse::ERROR_NO_TARGET,
  BSE_ERROR_NOT_OWNER		                = Bse::ERROR_NOT_OWNER,
  BSE_ERROR_INVALID_OFFSET		        = Bse::ERROR_INVALID_OFFSET,
  BSE_ERROR_INVALID_DURATION		        = Bse::ERROR_INVALID_DURATION,
  BSE_ERROR_INVALID_OVERLAP		        = Bse::ERROR_INVALID_OVERLAP,
} BseErrorType;


/* --- convenience functions --- */
const gchar*	bse_error_name			(BseErrorType	 error_value);
const gchar*	bse_error_nick			(BseErrorType	 error_value);
const gchar*	bse_error_blurb			(BseErrorType	 error_value);
BseErrorType	bse_error_from_errno		(gint		 v_errno,
						 BseErrorType    fallback);

#define bse_assert_ok(error)    G_STMT_START{                           \
     if G_UNLIKELY (error)                                              \
       {                                                                \
         g_log (G_LOG_DOMAIN, G_LOG_LEVEL_ERROR,                        \
                "%s:%d: unexpected error: %s",                          \
                __FILE__, __LINE__, bse_error_blurb (error));           \
       }                                                                \
}G_STMT_END

G_END_DECLS


#endif /* __BSE_ENUMS_H__ */
