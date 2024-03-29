/* 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Fluendo MPEG Demuxer plugin.
 *
 * The Initial Developer of the Original Code is Fluendo, S.L.
 * Portions created by Fluendo, S.L. are Copyright (C) 2005
 * Fluendo, S.L. All Rights Reserved.
 *
 * Contributor(s): Jan Schmidt <jan@fluendo.com>
 */

#ifndef __FLUTS_PAT_INFO_H__
#define __FLUTS_PAT_INFO_H__

#include <glib.h>

G_BEGIN_DECLS typedef struct FluTsPatInfoClass
{
  GObjectClass parent_class;
} FluTsPatInfoClass;

typedef struct FluTsPatInfo
{
  GObject parent;

  guint16 pid;
  guint16 program_no;
} FluTsPatInfo;

#define FLUTS_TYPE_PAT_INFO (fluts_pat_info_get_type ())
#define FLUTS_IS_PAT_INFO(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), FLUTS_TYPE_PAT_INFO))
#define FLUTS_PAT_INFO(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),FLUTS_TYPE_PAT_INFO, FluTsPatInfo))

GType fluts_pat_info_get_type (void);

FluTsPatInfo *fluts_pat_info_new (guint16 program_no, guint16 pid);

G_END_DECLS
#endif
