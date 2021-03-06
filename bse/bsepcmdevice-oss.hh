// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl.html
#ifndef	__BSE_PCM_DEVICE_OSS_H__
#define	__BSE_PCM_DEVICE_OSS_H__

#include	<bse/bsepcmdevice.hh>



/* --- object type macros --- */
#define BSE_TYPE_PCM_DEVICE_OSS		     (BSE_TYPE_ID (BsePcmDeviceOSS))
#define BSE_PCM_DEVICE_OSS(object)	     (G_TYPE_CHECK_INSTANCE_CAST ((object), BSE_TYPE_PCM_DEVICE_OSS, BsePcmDeviceOSS))
#define BSE_PCM_DEVICE_OSS_CLASS(class)	     (G_TYPE_CHECK_CLASS_CAST ((class), BSE_TYPE_PCM_DEVICE_OSS, BsePcmDeviceOSSClass))
#define BSE_IS_PCM_DEVICE_OSS(object)	     (G_TYPE_CHECK_INSTANCE_TYPE ((object), BSE_TYPE_PCM_DEVICE_OSS))
#define BSE_IS_PCM_DEVICE_OSS_CLASS(class)   (G_TYPE_CHECK_CLASS_TYPE ((class), BSE_TYPE_PCM_DEVICE_OSS))
#define BSE_PCM_DEVICE_OSS_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS ((object), BSE_TYPE_PCM_DEVICE_OSS, BsePcmDeviceOSSClass))

struct BsePcmDeviceOSS : BsePcmDevice {
  gchar       *device_name;
};
struct BsePcmDeviceOSSClass : BsePcmDeviceClass
{};

#endif /* __BSE_PCM_DEVICE_OSS_H__ */
