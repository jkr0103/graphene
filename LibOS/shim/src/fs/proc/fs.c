/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright (C) 2014 Stony Brook University */

/*!
 * \file
 *
 * This file contains the implementation of `/proc` pseudo-filesystem.
 */

#include "api.h"
#include "shim_fs.h"
#include "shim_fs_pseudo.h"
#include "shim_process.h"

int proc_self_follow_link(struct shim_dentry* dent, char** target) {
    __UNUSED(dent);
    IDTYPE pid = g_process.pid;
    char name[11];
    snprintf(name, sizeof(name), "%u", pid);
    if (!(*target = strdup(name)))
        return -ENOMEM;
    return 0;
}

static void init_thread_dir(struct pseudo_node* ent) {
    pseudo_add_link(ent, "root", &proc_thread_follow_link);
    pseudo_add_link(ent, "cwd", &proc_thread_follow_link);
    pseudo_add_link(ent, "exe", &proc_thread_follow_link);
    pseudo_add_str(ent, "maps", &proc_thread_maps_load);
    pseudo_add_str(ent, "cmdline", &proc_thread_cmdline_load);

    struct pseudo_node* fd = pseudo_add_dir(ent, "fd");
    struct pseudo_node* fd_link = pseudo_add_link(fd, /*name=*/NULL, &proc_thread_fd_follow_link);
    fd_link->match_name = &proc_thread_fd_match_name;
    fd_link->list_names = &proc_thread_fd_list_names;
}

static void init_ipc_thread_dir(struct pseudo_node* ent) {
    pseudo_add_link(ent, "root", &proc_ipc_thread_follow_link);
    pseudo_add_link(ent, "cwd", &proc_ipc_thread_follow_link);
    pseudo_add_link(ent, "exe", &proc_ipc_thread_follow_link);
}

int init_procfs(void) {
    struct pseudo_node* root = pseudo_add_root_dir("proc");

    pseudo_add_str(root, "meminfo", &proc_meminfo_load);
    pseudo_add_str(root, "cpuinfo", &proc_cpuinfo_load);

    pseudo_add_link(root, "self", &proc_self_follow_link);

    struct pseudo_node* thread_pid = pseudo_add_dir(root, /*name=*/NULL);
    thread_pid->match_name = &proc_thread_pid_match_name;
    thread_pid->list_names = &proc_thread_pid_list_names;
    init_thread_dir(thread_pid);

    struct pseudo_node* thread_task = pseudo_add_dir(thread_pid, "task");
    struct pseudo_node* thread_tid = pseudo_add_dir(thread_task, /*name=*/NULL);
    thread_tid->match_name = &proc_thread_tid_match_name;
    thread_tid->list_names = &proc_thread_tid_list_names;
    init_thread_dir(thread_tid);

    struct pseudo_node* ipc_thread_pid = pseudo_add_dir(root, /*name=*/NULL);
    ipc_thread_pid->match_name = &proc_ipc_thread_pid_match_name;
    ipc_thread_pid->list_names = &proc_ipc_thread_pid_list_names;
    init_ipc_thread_dir(ipc_thread_pid);

    return 0;
}
