/* Copyright (C) 2015-2019, Wazuh Inc.
 * Copyright (C) 2009 Trend Micro Inc.
 * All right reserved.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation
 */

#include "shared.h"
#include "syscheck.h"

volatile int audit_thread_active;
volatile int whodata_alerts;
volatile int audit_db_consistency_flag;

#include "fs_op.h"
#include "hash_op.h"
#include "debug_op.h"
#include "syscheck.h"
#include "syscheck_op.h"

#ifdef INOTIFY_ENABLED
#include <sys/inotify.h>

#define REALTIME_MONITOR_FLAGS  IN_MODIFY|IN_ATTRIB|IN_MOVED_FROM|IN_MOVED_TO|IN_CREATE|IN_DELETE|IN_DELETE_SELF
#define REALTIME_EVENT_SIZE     (sizeof (struct inotify_event))
#define REALTIME_EVENT_BUFFER   (2048 * (REALTIME_EVENT_SIZE + 16))

int realtime_start()
{
    os_calloc(1, sizeof(rtfim), syscheck.realtime);

    syscheck.realtime->dirtb = OSHash_Create();
    if (syscheck.realtime->dirtb == NULL) {
        merror(MEM_ERROR, errno, strerror(errno));
        return (-1);
    }

    OSHash_SetFreeDataPointer(syscheck.realtime->dirtb, (void (*)(void *))free);

    syscheck.realtime->fd = inotify_init();
    if (syscheck.realtime->fd < 0) {
        merror(FIM_ERROR_INOTIFY_INITIALIZE);
        return (-1);
    }

    return (0);
}

/* Add a directory to real time checking */
int realtime_adddir(const char *dir, __attribute__((unused)) int whodata)
{
    if (whodata && audit_thread_active) {
        // Save dir into saved rules list
        w_mutex_lock(&audit_mutex);

        if(!W_Vector_insert_unique(audit_added_dirs, dir)){
            mdebug1(FIM_WHODATA_NEWDIRECTORY, dir);
        }

        w_mutex_unlock(&audit_mutex);

    } else {
        if (!syscheck.realtime) {
            realtime_start();
        }

        /* Check if it is ready to use */
        if (syscheck.realtime->fd < 0) {
            return (-1);
        } else {
            int wd = 0;

            wd = inotify_add_watch(syscheck.realtime->fd,
                                   dir,
                                   REALTIME_MONITOR_FLAGS);
            if (wd < 0) {
                if (errno == 28) {
                    merror(FIM_ERROR_INOTIFY_ADD_MAX_REACHED, dir, wd, errno);
                } else {
                    mdebug1(FIM_INOTIFY_ADD_WATCH, dir, wd, errno, strerror(errno));
                }
            } else {
                char wdchar[33];
                char *data;
                int retval;
                snprintf(wdchar, 33, "%d", wd);
                os_strdup(dir, data);

                if (!OSHash_Get_ex(syscheck.realtime->dirtb, wdchar)) {
                    if (retval = OSHash_Add_ex(syscheck.realtime->dirtb, wdchar, data), retval == 0) {
                        os_free(data);
                        merror_exit(FIM_CRITICAL_ERROR_OUT_MEM);
                    } else if (retval == 1) {
                        mdebug2(FIM_REALTIME_HASH_DUP, data);
                        os_free(data);
                    }
                    mdebug1(FIM_REALTIME_NEWDIRECTORY, dir);
                } else {
                    if (retval = OSHash_Update_ex(syscheck.realtime->dirtb, wdchar, data), retval == 0) {
                        merror("Unable to update 'dirtb'. Directory not found: '%s'", data);
                        os_free(data);
                        return (-1);
                    }
                }
            }
        }
    }

    return (1);
}

/* Process events in the real time queue */
void realtime_process()
{
    ssize_t len;
    char buf[REALTIME_EVENT_BUFFER + 1];
    struct inotify_event *event;

    buf[REALTIME_EVENT_BUFFER] = '\0';

    len = read(syscheck.realtime->fd, buf, REALTIME_EVENT_BUFFER);
    if (len < 0) {
        merror(FIM_ERROR_REALTIME_READ_BUFFER);
    } else if (len > 0) {
        rb_tree * tree = rbtree_init();

        for (size_t i = 0; i < (size_t) len; i += REALTIME_EVENT_SIZE + event->len) {
            event = (struct inotify_event *) (void *) &buf[i];

            if (event->wd == -1 && event->mask == IN_Q_OVERFLOW) {
                mwarn("Real-time inotify kernel queue is full. Some events may be lost. Next scheduled scan will recover lost data.");
                send_log_msg("ossec: Real-time inotify kernel queue is full. Some events may be lost. Next scheduled scan will recover lost data.");
            } else {
                char wdchar[33];
                char final_name[MAX_LINE + 1];
                char *entry;

                final_name[MAX_LINE] = '\0';

                snprintf(wdchar, 33, "%d", event->wd);

                //The configured paths can end at / or not, we must check it.
                entry = (char *)OSHash_Get(syscheck.realtime->dirtb, wdchar);
                if (entry) {
                    // Check file entries with realtime
                    if (event->len == 0) {
                        snprintf(final_name, MAX_LINE, "%s", entry);
                    } else {
                        // Check directories entries with realtime
                        if (entry[strlen(entry) - 1] == PATH_SEP) {
                            snprintf(final_name, MAX_LINE, "%s%s",
                                    entry,
                                    event->name);
                        } else {
                            snprintf(final_name, MAX_LINE, "%s/%s",
                                    entry,
                                    event->name);
                        }
                    }

                    if (rbtree_insert(tree, final_name, (void *)1) == NULL) {
                        mdebug2("Duplicate event in real-time buffer: %s", final_name);
                    }

                    switch(event->mask) {
                    case(IN_DELETE_SELF):
                        w_mutex_lock(&syscheck.fim_realtime_mutex);
                        char * data = OSHash_Delete_ex(syscheck.realtime->dirtb, wdchar);
                        os_free(data);
                        w_mutex_unlock(&syscheck.fim_realtime_mutex);
                        break;
                    }
                }
            }
        }

        /* Need a sleep here to avoid triggering on vim
         * (and finding the file removed)
         */
        struct timeval timeout = {0, syscheck.rt_delay * 1000};
        select(0, NULL, NULL, NULL, &timeout);

        char ** paths = rbtree_keys(tree);

        for (int i = 0; paths[i] != NULL; i++) {
            fim_realtime_event(paths[i], NULL);
        }

        free_strarray(paths);
        rbtree_destroy(tree);
    }
}

void free_syscheck_dirtb_data(char *data) {
    free(data);
}

#elif defined(WIN32)

static pthread_mutex_t adddir_mutex;

typedef struct _win32rtfim {
    HANDLE h;
    OVERLAPPED overlap;

    char *dir;
    TCHAR buffer[65536];
} win32rtfim;

int realtime_win32read(win32rtfim *rtlocald);

void CALLBACK RTCallBack(DWORD dwerror, DWORD dwBytes, LPOVERLAPPED overlap)
{
    int lcount;
    size_t offset = 0;
    char wdchar[260 + 1];
    char final_path[MAX_LINE + 1];
    win32rtfim *rtlocald;
    PFILE_NOTIFY_INFORMATION pinfo;
    TCHAR finalfile[MAX_PATH];

    memset(final_path, '\0', MAX_LINE + 1);

    if (dwBytes == 0) {
        mwarn(FIM_WARN_REALTIME_OVERFLOW);
    }

    if (dwerror != ERROR_SUCCESS) {
        LPSTR messageBuffer = NULL;
        LPSTR end;

        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dwerror, 0, (LPTSTR) &messageBuffer, 0, NULL);

        if (end = strchr(messageBuffer, '\r'), end) {
            *end = '\0';
        }

        merror(FIM_ERROR_REALTIME_WINDOWS_CALLBACK, messageBuffer, dwerror);
        LocalFree(messageBuffer);

        return;
    }

    /* Get hash to parse the data */
    wdchar[260] = '\0';
    snprintf(wdchar, 260, "%s", (char*)overlap->Pointer);
    rtlocald = OSHash_Get(syscheck.realtime->dirtb, wdchar);
    if (rtlocald == NULL) {
        merror(FIM_ERROR_REALTIME_WINDOWS_CALLBACK_EMPTY);
        return;
    }

    if (dwBytes) {
        fim_element *item;
        os_calloc(1, sizeof(fim_element), item);

        do {
            pinfo = (PFILE_NOTIFY_INFORMATION) &rtlocald->buffer[offset];
            offset += pinfo->NextEntryOffset;

            lcount = WideCharToMultiByte(CP_ACP, 0, pinfo->FileName,
                                         pinfo->FileNameLength / sizeof(WCHAR),
                                         finalfile, MAX_PATH - 1, NULL, NULL);
            finalfile[lcount] = TEXT('\0');

            final_path[MAX_LINE] = '\0';

            if (rtlocald->dir) {
                if (rtlocald->dir[strlen(rtlocald->dir) - 1] == PATH_SEP) {
                    snprintf(final_path, MAX_LINE, "%s%s",
                            rtlocald->dir,
                            finalfile);
                } else {
                    snprintf(final_path, MAX_LINE, "%s\\%s",
                            rtlocald->dir,
                            finalfile);
                }
            }
            str_lowercase(final_path);

            int index = fim_configuration_directory(wdchar, "file");
            int file_index = fim_configuration_directory(final_path, "file");

            Sleep(syscheck.rt_delay);

            if (index == file_index) {
                item->mode = FIM_REALTIME;
                /* Check the change */
                fim_realtime_event(final_path, item);
            }

        } while (pinfo->NextEntryOffset != 0);
        os_free(item);
    }

    realtime_win32read(rtlocald);
    return;
}

void free_win32rtfim_data(win32rtfim *data) {
    if (!data) return;
    if (data->h != NULL && data->h != INVALID_HANDLE_VALUE) CloseHandle(data->h);
    if (data->overlap.Pointer) free(data->overlap.Pointer);
    if (data->dir) free(data->dir);
    free(data);
}

int realtime_start()
{
    w_mutex_init(&adddir_mutex, NULL);

    os_calloc(1, sizeof(rtfim), syscheck.realtime);

    syscheck.realtime->dirtb = OSHash_Create();
    if (syscheck.realtime->dirtb == NULL) {
        merror(MEM_ERROR, errno, strerror(errno));
        return(-1);
    }
    OSHash_SetFreeDataPointer(syscheck.realtime->dirtb, (void (*)(void *))free_win32rtfim_data);

    syscheck.realtime->fd = -1;
    syscheck.realtime->evt = CreateEvent(NULL, TRUE, FALSE, NULL);

    return (0);
}

int realtime_win32read(win32rtfim *rtlocald)
{
    int rc;

    rc = ReadDirectoryChangesW(rtlocald->h,
                               rtlocald->buffer,
                               sizeof(rtlocald->buffer) / sizeof(TCHAR),
                               TRUE,
                               FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE |
                               FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SECURITY,
                               0,
                               &rtlocald->overlap,
                               RTCallBack);
    if (rc == 0) {
        mdebug1(FIM_REALTIME_DIRECTORYCHANGES, rtlocald->dir);
        sleep(2);
    }

    return (0);
}

// In Windows the whodata parameter contains the directory position + 1 to be able to reference it
int realtime_adddir(const char *dir, int whodata)
{
    char wdchar[260 + 1];
    win32rtfim *rtlocald;

    if (whodata) {
#ifdef WIN_WHODATA

    if (!syscheck.wdata.whodata_setup) {
        syscheck.wdata.whodata_setup = 1;
    }
    int type;

    if (!syscheck.wdata.fd && whodata_audit_start()) {
        merror_exit(FIM_CRITICAL_DATA_CREATE, "whodata file descriptors");
    }

    // This parameter is used to indicate if the file is going to be monitored in Whodata mode,
    // regardless of it was checked in the initial configuration (WHODATA_ACTIVE in opts)
    syscheck.wdata.dirs_status[whodata - 1].status |= WD_CHECK_WHODATA;
    syscheck.wdata.dirs_status[whodata - 1].status &= ~WD_CHECK_REALTIME;

    // Check if the file or directory exists
    if (type = check_path_type(dir), type == 2) {
        syscheck.wdata.dirs_status[whodata - 1].object_type = WD_STATUS_DIR_TYPE;
        syscheck.wdata.dirs_status[whodata - 1].status |= WD_STATUS_EXISTS;
    } else if (type == 1) {
        syscheck.wdata.dirs_status[whodata - 1].object_type = WD_STATUS_FILE_TYPE;
        syscheck.wdata.dirs_status[whodata - 1].status |= WD_STATUS_EXISTS;
    } else {
        mdebug1(FIM_WARN_REALTIME_OPENFAIL, dir);
            
        syscheck.wdata.dirs_status[whodata - 1].object_type = WD_STATUS_UNK_TYPE;
        syscheck.wdata.dirs_status[whodata - 1].status &= ~WD_STATUS_EXISTS;
        return 0;
    }

    GetSystemTime(&syscheck.wdata.dirs_status[whodata - 1].last_check);
    if (set_winsacl(dir, whodata - 1)) {
        merror(FIM_ERROR_WHODATA_ADD_DIRECTORY, dir);
        return 0;
    }
    return 1;
#endif
    }

    if (!syscheck.realtime) {
        if (realtime_start() < 0 ) {
            return (-1);
        }
    }

    w_mutex_lock(&adddir_mutex);

    /* Maximum limit for realtime on Windows */
    if (syscheck.realtime->fd > syscheck.max_fd_win_rt) {
        merror(FIM_ERROR_REALTIME_MAXNUM_WATCHES, dir);
        w_mutex_unlock(&adddir_mutex);
        return (0);
    }

    /* Set key for hash */
    wdchar[260] = '\0';
    snprintf(wdchar, 260, "%s", dir);
    if(OSHash_Get_ex(syscheck.realtime->dirtb, wdchar)) {
        mdebug2(FIM_REALTIME_HASH_DUP, wdchar);
        w_mutex_unlock(&adddir_mutex);
    }
    else {
        os_calloc(1, sizeof(win32rtfim), rtlocald);

        rtlocald->h = CreateFile(dir,
                                FILE_LIST_DIRECTORY,
                                FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL,
                                OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                NULL);


        if (rtlocald->h == INVALID_HANDLE_VALUE || rtlocald->h == NULL) {
            free(rtlocald);
            rtlocald = NULL;
            mdebug1(FIM_REALTIME_ADD, dir);
            w_mutex_unlock(&adddir_mutex);
            return (0);
        }
        syscheck.realtime->fd++;
        w_mutex_unlock(&adddir_mutex);

        /* Add final elements to the hash */
        os_strdup(dir, rtlocald->dir);
        os_strdup(dir, rtlocald->overlap.Pointer);
        if (!OSHash_Add_ex(syscheck.realtime->dirtb, wdchar, rtlocald)) {
            merror_exit(FIM_CRITICAL_ERROR_OUT_MEM);
        }
        /* Add directory to be monitored */
        realtime_win32read(rtlocald);
    }

    return (1);
}

#else /* !WIN32 */

int realtime_start()
{
    merror(FIM_ERROR_REALTIME_INITIALIZE);

    return (0);
}

int realtime_adddir(__attribute__((unused)) const char *dir, __attribute__((unused))int whodata)
{
    return (0);
}

void realtime_process()
{
    return;
}

#endif /* WIN32 */
