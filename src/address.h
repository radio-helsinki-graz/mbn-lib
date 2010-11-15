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

#ifndef ADDRESS_H
#define ADDRESS_H

#include "mbn.h"

void init_addresses(struct mbn_handler *);
void *node_timeout_thread(void *);
int process_address_message(struct mbn_handler *, struct mbn_message *, void *);
void free_addresses(struct mbn_handler *);

#endif

