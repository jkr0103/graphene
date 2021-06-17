/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright (C) 2021 Intel Corporation
 *                    Paweł Marczewski <pawel@invisiblethingslab.com>
 */

/*
 * This file defines the "pseudo" filesystem: a building block for implementing pseudo-FS's such as
 * `/proc`, `/sys` or `/dev`.
 *
 * A pseudo-filesystem is defined by creating a tree of `pseudo_node` nodes:
 *
 *     struct pseudo_node* root = pseudo_add_root_dir("proc");
 *     pseudo_add_link(root, "self", &proc_self_follow_link);
 *     pseudo_add_str(root, "cpuinfo", &proc_cpuinfo_load);
 *
 * It can then be mounted by providing the root name ("proc" in the example):
 *
 *     ret = mount_fs("pseudo", "proc", "/proc");
 *
 * See the documentation of `pseudo_node` structure for details.
 *
 * TODO: The pseudo filesystem currently does not implement proper invalidation for
 * dynamically-generated directory listings (such as `/proc/<pid>`), so stale dentries might remain
 * for files that shouldn't exist anymore.
 */

#ifndef SHIM_FS_PSEUDO_H_
#define SHIM_FS_PSEUDO_H_

#include "list.h"
#include "perm.h"
#include "shim_fs.h"

/* Node types */
enum pseudo_type {
    /* Directory */
    PSEUDO_DIR = 1,

    /* Symbolic link */
    PSEUDO_LINK = 2,

    /* String-backed file */
    PSEUDO_STR = 3,

    /* Character device */
    PSEUDO_DEV = 4,
};

/* Callbacks for PSEUDO_DEV nodes */
struct shim_dev_ops {
    int (*open)(struct shim_handle* hdl, struct shim_dentry* dent, int flags);
    int (*close)(struct shim_handle* hdl);
    ssize_t (*read)(struct shim_handle* hdl, void* buf, size_t count);
    ssize_t (*write)(struct shim_handle* hdl, const void* buf, size_t count);
    int (*flush)(struct shim_handle* hdl);
    int64_t (*seek)(struct shim_handle* hdl, int64_t offset, int whence);
    int (*truncate)(struct shim_handle* hdl, uint64_t len);
};

#define PSEUDO_PERM_DIR     PERM_r_xr_xr_x  /* default for directories */
#define PSEUDO_PERM_LINK    PERM_rwxrwxrwx  /* default for links */
#define PSEUDO_PERM_FILE_R  PERM_r__r__r__  /* default for all other files */
#define PSEUDO_PERM_FILE_RW PERM_rw_rw_rw_

/*
 * A node of the pseudo filesystem. A single node can describe either a single file, or a family of
 * files (see `match_name` and `list_names` below).
 *
 * The constructors for `pseudo_node` (`pseudo_add_*`) take arguments for most commonly used fields.
 * The node can then be further customized by directly modifying other fields.
 */
DEFINE_LIST(pseudo_node);
DEFINE_LISTP(pseudo_node);
struct pseudo_node {
    enum pseudo_type type;

    LIST_TYPE(pseudo_node) siblings;

    struct pseudo_node* parent;

    /* File name. Can be NULL if the node provides the below callbacks. */
    const char* name;

    /* Verify if a file with a given name exists. Should return 0 if file exists, negative value if
     * not. */
    int (*match_name)(struct shim_dentry* parent, const char* name);

    /* Retrieves all file names for this node. Works the same as `readdir`. */
    int (*list_names)(struct shim_dentry* parent, readdir_callback_t callback, void* arg);

    /* File permissions. See `PSEUDO_PERM_*` above for defaults. */
    mode_t perm;

    /* Type-specific fields: */
    union {
        /* PSEUDO_DIR */
        struct {
            LISTP_TYPE(pseudo_node) children;
        } dir;

        /* PSEUDO_LINK */
        struct {
            /*
             * Read link target. Should allocate a new (null-terminated) string: it will be freed
             * using `free`.
             *
             * If the callback is not provided, the `target` field will be used instead.
             */
            int (*follow_link)(struct shim_dentry* dent, char** target);

            const char* target;
        } link;

        /* PSEUDO_STR */
        struct {
            /*
             * Provide data for newly-opened file. Should allocate a new buffer: on file close, the
             * buffer will be freed using `free`.
             *
             * If the callback is not provided, the opened file will start out empty.
             */
            int (*load)(struct shim_dentry* dent, char** data, size_t* size);

            /* Invoked when saving a modified file (on `close` or `flush`). Optional. */
            int (*save)(struct shim_dentry* dent, const char* data, size_t size);
        } str;

        /* PSEUDO_DEV */
        struct {
            /* Callbacks to use with this device */
            struct shim_dev_ops dev_ops;

            /* Device major and minor numbers, used with `stat` */
            unsigned int major;
            unsigned int minor;
        } dev;
    };
};

struct pseudo_node* pseudo_add_root_dir(const char* name);

struct pseudo_node* pseudo_add_dir(struct pseudo_node* parent_ent, const char* name);

struct pseudo_node* pseudo_add_link(struct pseudo_node* parent_ent, const char* name,
                                    int (*follow_link)(struct shim_dentry*, char**));

struct pseudo_node* pseudo_add_str(struct pseudo_node* parent_ent, const char* name,
                                   int (*load)(struct shim_dentry*, char**, size_t*));

struct pseudo_node* pseudo_add_dev(struct pseudo_node* parent_ent, const char* name);

extern struct shim_fs pseudo_builtin_fs;

/* procfs */

int init_procfs(void);
int proc_meminfo_load(struct shim_dentry* dent, char** data, size_t* size);
int proc_cpuinfo_load(struct shim_dentry* dent, char** data, size_t* size);
int proc_self_follow_link(struct shim_dentry* dent, char** target);
int proc_thread_pid_match_name(struct shim_dentry* parent, const char* name);
int proc_thread_pid_list_names(struct shim_dentry* parent, readdir_callback_t callback, void* arg);
int proc_thread_tid_match_name(struct shim_dentry* parent, const char* name);
int proc_thread_tid_list_names(struct shim_dentry* parent, readdir_callback_t callback, void* arg);
int proc_thread_follow_link(struct shim_dentry* dent, char** target);
int proc_thread_maps_load(struct shim_dentry* dent, char** data, size_t* size);
int proc_thread_cmdline_load(struct shim_dentry* dent, char** data, size_t* size);
int proc_thread_fd_match_name(struct shim_dentry* parent, const char* name);
int proc_thread_fd_list_names(struct shim_dentry* parent, readdir_callback_t callback, void* arg);
int proc_thread_fd_follow_link(struct shim_dentry* dent, char** target);
int proc_ipc_thread_pid_match_name(struct shim_dentry* parent, const char* name);
int proc_ipc_thread_pid_list_names(struct shim_dentry* parent, readdir_callback_t callback,
                                   void* arg);
int proc_ipc_thread_follow_link(struct shim_dentry* dent, char** target);

/* devfs */

int init_devfs(void);
int init_attestation(struct pseudo_node* dev);

/* sysfs */

int init_sysfs(void);
int sys_load(const char* str, char** data, size_t* size);
int sys_resource_find(struct shim_dentry* parent, const char* name, unsigned int* num);
int sys_resource_match_name(struct shim_dentry* parent, const char* name);
int sys_resource_list_names(struct shim_dentry* parent, readdir_callback_t callback, void* arg);
int sys_node_general_load(struct shim_dentry* dent, char** data, size_t* size);
int sys_node_load(struct shim_dentry* dent, char** data, size_t* size);
int sys_cpu_general_load(struct shim_dentry* dent, char** data, size_t* size);
int sys_cpu_load(struct shim_dentry* dent, char** data, size_t* size);
int sys_cache_load(struct shim_dentry* dent, char** data, size_t* size);
int sys_cpu_online_match_name(struct shim_dentry* parent, const char* name);
int sys_cpu_online_list_names(struct shim_dentry* parent, readdir_callback_t callback, void* arg);

#endif /* SHIM_FS_PSEUDO_H_ */
