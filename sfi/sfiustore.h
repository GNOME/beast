/* SFI - Synthesis Fusion Kit Interface
 * Copyright (C) 2002 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __SFI_USTORE_H__
#define __SFI_USTORE_H__

#include <sfi/sfitypes.h>

G_BEGIN_DECLS


/* --- typedefs --- */
typedef gboolean (*SfiUStoreForeach)	(gpointer	 data,
					 gulong		 unique_id,
					 gpointer	 value);
/* typedef struct _SfiUStore SfiUStore; */


/* --- functions --- */
SfiUStore*	sfi_ustore_new		(void);
gpointer	sfi_ustore_lookup	(SfiUStore	*store,
					 gulong		 unique_id);
void		sfi_ustore_insert	(SfiUStore	*store,
					 gulong		 unique_id,
					 gpointer	 value);
void		sfi_ustore_remove	(SfiUStore	*store,
					 gulong		 unique_id);
void		sfi_ustore_foreach	(SfiUStore	*store,
					 SfiUStoreForeach foreach,
					 gpointer	 data);
void		sfi_ustore_destroy	(SfiUStore	*store);


G_END_DECLS

#endif /* __SFI_USTORE_H__ */

/* vim:set ts=8 sts=2 sw=2: */
