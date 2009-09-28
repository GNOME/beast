/* BSE - Bedevilled Sound Engine
 * Copyright (C) 2006 Stefan Westerfeld
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
/*
 * The notification stuff was adapted from glib's gmain.c (also LGPL,
 * see glib's AUTHORS file for individual authors).
 */
#include "bsewin32.h"
#include <windows.h>
#include "topconfig.h"

struct _BseWin32Waiter
{
  HANDLE wake_up_semaphore;
};

extern "C" BseWin32Waiter *
bse_win32_waiter_new ()
{
  BseWin32Waiter *waiter = g_new0 (BseWin32Waiter, 1);

  waiter->wake_up_semaphore = CreateSemaphore (NULL, 0, 100, NULL);
  if (waiter->wake_up_semaphore == NULL)
    g_error ("Cannot create wake-up semaphore: %s",
             g_win32_error_message (GetLastError ()));

  return waiter;
}

extern "C" void
bse_win32_waiter_destroy (BseWin32Waiter *waiter)
{
  CloseHandle (waiter->wake_up_semaphore);
}

extern "C" void
bse_win32_waiter_wakeup (BseWin32Waiter *waiter)
{
  ReleaseSemaphore (waiter->wake_up_semaphore, 1, NULL);
}

extern "C" int
bse_win32_waiter_wait (BseWin32Waiter *waiter,
                       int             timeout)
{
  HANDLE handles[1] = { waiter->wake_up_semaphore }; 

  guint ready = WaitForMultipleObjects (1, handles, FALSE, timeout < 0 ? INFINITE : timeout);
  if (ready == WAIT_FAILED)
    {
      gchar *emsg = g_win32_error_message (GetLastError ());
      g_warning (G_STRLOC ": WaitForMultipleObjects() failed: %s", emsg);
      g_free (emsg);

      return -1;
    }
  if (ready == WAIT_TIMEOUT)
    return 0;
  return 1;
}

static void
transform_bslash_to_slash (char *buffer)
{
  while (*buffer)
    {
      if (*buffer == '\\')
	*buffer = '/';
      buffer++;
    }
}

static void
cut_last_name_component (char *buffer, int times)
{
  while (times--)
    {
      char *delim = buffer;
      for (char *i = buffer; *i; i++)
	{
	  if (*i == '/')
	    delim = i;
	}
      *delim = 0;
    }
}

extern "C" const char*
bse_path_relocate (const char *path)
{
  const char *result = path;
  const int prefix_len = strlen ("/c/beast");
  if (strncmp (path, "/c/beast", prefix_len) == 0)
    {
      char buffer[512];
      GetModuleFileName(NULL, buffer, sizeof(buffer));
      transform_bslash_to_slash (buffer);
      cut_last_name_component (buffer, 2);
      char *relocated = g_strdup_printf ("%s%s", buffer, path + prefix_len);
      result = g_intern_string (relocated);
      g_free (relocated);
    }
  return result;
}
