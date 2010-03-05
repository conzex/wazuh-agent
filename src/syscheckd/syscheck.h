/* @(#) $Id$ */

/* Copyright (C) 2009 Trend Micro Inc.
 * All right reserved.
 *
 * This program is a free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation
 */
       

#ifndef __SYSCHECK_H

#define __SYSCHECK_H

#include "config/syscheck-config.h"
#define MAX_LINE PATH_MAX+256

/* Notify list size */
#define NOTIFY_LIST_SIZE    32


/* Global config */
config syscheck;


/** Function Prototypes **/

/* run_check: checks the integrity of the files against the
 * saved database
 */
void run_check();


/* start_daemon: Runs run_check periodically.
 */
void start_daemon();


/* Read the XML config */
int Read_Syscheck_Config(char * cfgfile);


/* create the database */
int create_db();


/* int check_db()
 * Checks database for new files.
 */
int check_db();
  
/** void os_winreg_check()
 * Checks the registry for changes.
 */  
void os_winreg_check();

/* starts real time */
int realtime_start();

/* Adds a directory to real time monitoring. */
int realtime_adddir(char *dir);

/* Process real time queue. */
int realtime_process();

/** Sends syscheck message.
 */
int send_syscheck_msg(char *msg);
int send_rootcheck_msg(char *msg);


#endif

/* EOF */
