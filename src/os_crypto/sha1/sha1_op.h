/* @(#) $Id$ */

/* Copyright (C) 2006 Daniel B. Cid <dcid@ossec.net>
 * All right reserved.
 *
 * This program is a free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 3) as published by the FSF - Free Software
 * Foundation
 */


#ifndef __SHA1_OP_H

#define __SHA1_OP_H


typedef char os_sha1[65];

int OS_SHA1_File(char *fname, char * output);

#endif

/* EOF */
