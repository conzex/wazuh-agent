/* Copyright (C) 2015-2019, Wazuh Inc.
 * Copyright (C) 2009 Trend Micro Inc.
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
#include "external/cJSON/cJSON.h"

#define MAX_LINE PATH_MAX+256

/* Notify list size */
#define NOTIFY_LIST_SIZE    32

// Number of attributes in the hash table
#define SK_DB_NATTR 11
#define SK_DB_REPORT_CHANG 10

#define WDATA_DEFAULT_INTERVAL_SCAN 300

#ifdef WIN32
#define FIM_REGULAR _S_IFREG
#define FIM_DIRECTORY _S_IFDIR
#else
#define FIM_REGULAR S_IFREG
#define FIM_DIRECTORY S_IFDIR
#define FIM_LINK S_IFLNK
#endif

/* Global config */
extern syscheck_config syscheck;
extern int sys_debug_level;

/** Function Prototypes **/

/* Win32 does not have lstat */
#ifdef WIN32
    #define w_stat(x, y) stat(x, y)
#else
    #define w_stat(x, y) lstat(x, y)
#endif

/* Check the integrity of the files against the saved database */
void run_check(void);

/* Run run_check periodically */
void start_daemon(void) __attribute__((noreturn));

/* Read the XML config */
int Read_Syscheck_Config(const char *cfgfile) __attribute__((nonnull));

/* Parse readed config into JSON format */
cJSON *getSyscheckConfig(void);
cJSON *getSyscheckInternalOptions(void);

// TODO: Add description to functions

// Create the database
//
int fim_scan();

//
int fim_scheduled_scan();

//
int fim_directory (char * path, int dir_position, whodata_evt * w_evt, int max_depth);

//
int fim_check_file (char * file_name, int dir_position, int mode, whodata_evt * w_evt);

//
int fim_process_event(char * file, int mode, whodata_evt *w_evt);

//
int fim_configuration_directory (char * path);

//
int fim_check_depth(char * path, int dir_position);

//
fim_entry_data * fim_get_data (const char * file_name, struct stat file_stat, int mode, int options);

//
char * fim_get_checksum (fim_entry_data * data);

//
int fim_insert (char *file_name, fim_entry_data * data);

//
int fim_update (char * file, fim_entry_data * data);

//
int fim_delete (char * file_name);

//
int check_deleted_files();

//
void delete_inode_item(char *inode, char *file_name);

//
cJSON *fim_json_event(char *file_name, fim_entry_data *old_data, fim_entry_data *new_data, int dir_position, int type, int mode, whodata_evt *w_evt);

//
cJSON * fim_json_alert (char * file_name, fim_entry_data * data, int dir_position, int type, int mode, whodata_evt * w_evt);

//
cJSON * fim_json_alert_changes (char * file_name, fim_entry_data * old_data, fim_entry_data * new_data, int dir_position, int type, int mode, whodata_evt * w_evt);

//
void set_integrity_index(char * file_name, fim_entry_data * data);

//
void free_entry_data(fim_entry_data * data);

//
void free_inode_data(fim_inode_data * data);

/* Check the registry for changes */
void os_winreg_check(void);

/* Start real time */
int realtime_start(void);

/* Add a directory to real time monitoring */
int realtime_adddir(const char *dir, int whodata) __attribute__((nonnull(1)));

/* Initializes the whodata scan mode */
int run_whodata_scan(void);

/* Process real time queue */
int realtime_process(void);

/* Process the content of the file changes */
char *seechanges_addfile(const char *filename) __attribute__((nonnull));

/* Generate the whodata csum */
int extract_whodata_sum(whodata_evt *evt, char *wd_sum, int size);
void init_whodata_event(whodata_evt *w_evt);
void free_whodata_event(whodata_evt *w_evt);

/* Get checksum changes */
int c_read_file(const char *file_name, const char *linked_file, const char *oldsum, char *newsum, whodata_evt * evt) __attribute__((nonnull(1,3,4)));

int send_syscheck_msg(const char *msg) __attribute__((nonnull));
int send_rootcheck_msg(const char *msg) __attribute__((nonnull));

//int realtime_checksumfile(const char *file_name, whodata_evt *evt) __attribute__((nonnull(1)));

/* Return the version with symbolic link */
void replace_linked_path(const char *file_name, int dir_position, char *linked_file);

/* Returns the real path associated with a position securely */
char *get_converted_link_path(int position);

#ifdef __linux__
#define READING_MODE 0
#define HEALTHCHECK_MODE 1
int audit_init(void);
void audit_read_events(int *audit_sock, int reading_mode);
void audit_set_db_consistency(void);
int check_auditd_enabled(void);
int set_auditd_config(void);
int init_auditd_socket(void);
int audit_add_rule(const char *path, const char *key);
int audit_delete_rule(const char *path, const char *key);
void *audit_main(int *audit_sock);
void *audit_reload_thread();
void *audit_healthcheck_thread(int *audit_sock);
void audit_reload_rules(void);
int audit_health_check(int audit_socket);
void clean_rules(void);
int filterkey_audit_events(char *buffer);
extern W_Vector *audit_added_dirs;
extern volatile int audit_thread_active;
extern volatile int whodata_alerts;
extern volatile int audit_db_consistency_flag;
extern pthread_mutex_t audit_mutex;
extern pthread_cond_t audit_thread_started;
extern pthread_cond_t audit_hc_started;
extern pthread_cond_t audit_db_consistency;
#elif WIN32
int whodata_audit_start();
int set_winsacl(const char *dir, int position);
long unsigned int WINAPI state_checker(__attribute__((unused)) void *_void);
#endif

extern pthread_mutex_t __lastcheck_mutex;
int fim_initialize();

/* Check for restricts and ignored files */
int fim_check_ignore(const char *file_name);
int fim_check_restrict(const char *file_name, OSMatch *restriction);

#ifndef WIN32
// Com request thread dispatcher
void * syscom_main(void * arg);
// Checking links to follow
int read_links(const char *dir_name, int dir_position, int max_depth, unsigned int is_link);
#endif
size_t syscom_dispatch(char *command, char ** output);
size_t syscom_getconfig(const char * section, char ** output);
void symlink_checker_init();

#ifdef WIN_WHODATA
int w_update_sacl(const char *obj_path);
#endif

int print_hash_table();
int fim_delete_hashes(char *file_name);

#ifdef WIN32
#define check_removed_file(x) ({ strstr(x, ":\\$recycle.bin") ? 1 : 0; })
#endif

#endif
