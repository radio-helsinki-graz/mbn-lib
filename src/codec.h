/****************************************************************************
**
** Copyright (C) 2009 D&R Electronica Weesp B.V. All rights reserved.
**
** This file is part of the Axum/MambaNet digital mixing system.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#ifndef CODEC_H
#define CODEC_H

#include "mbn.h"

int parse_message(struct mbn_message *);
void free_message(struct mbn_message *);
int create_message(struct mbn_message *, char);
void copy_message(const struct mbn_message *, struct mbn_message *);

#endif
