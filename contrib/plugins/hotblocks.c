/*
 * Copyright (C) 2019, Alex Bennée <alex.bennee@linaro.org>
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */
#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <glib.h>

#include <qemu-plugin.h>
// Import time library to capture the length of the program in microseconds
#include <time.h>

// Start and end time of the program
struct timespec start, end;

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static bool do_inline;

/* Plugins need to take care of their own locking */
static GMutex lock;
static GHashTable *hotblocks;
static guint64 limit = 20;

/*
 * Counting Structure
 *
 * The internals of the TCG are not exposed to plugins so we can only
 * get the starting PC for each block. We cheat this slightly by
 * xor'ing the number of instructions to the hash to help
 * differentiate.
 */
typedef struct {
    uint64_t start_addr;
    uint64_t exec_count;
    int      trans_count;
    unsigned long insns;
} ExecCount;


long get_time_in_ms(struct timespec *ts) {
    return ts->tv_sec * 1000LL + ts->tv_nsec / 1000000; // Convert sec + nanosec to milliseconds
}

static gint cmp_exec_count(gconstpointer a, gconstpointer b)
{
    ExecCount *ea = (ExecCount *) a;
    ExecCount *eb = (ExecCount *) b;
    return ea->exec_count > eb->exec_count ? -1 : 1;
}

static void plugin_exit(qemu_plugin_id_t id, void *p)
{
    // Get the end time of the program in milliseconds using time.h
    clock_gettime(CLOCK_MONOTONIC, &end);
    // Calculate the time taken by the program in milliseconds
    long elapsed_time_mili = get_time_in_ms(&end) - get_time_in_ms(&start);
    g_autoptr(GString) report = g_string_new("collected ");
    GList *counts, *it;
    int i;

    g_mutex_lock(&lock);
    g_string_append_printf(report, "%d entries in the hash table\n",
                           g_hash_table_size(hotblocks));
    counts = g_hash_table_get_values(hotblocks);
    it = g_list_sort(counts, cmp_exec_count);

    // Total number of instructions
    long long int total_insns = 0;

    if (it) {
        g_string_append_printf(report, "pc, tcount, icount, ecount\n");

        for (i = 0; i < limit && it->next; i++, it = it->next) {
            ExecCount *rec = (ExecCount *) it->data;
            g_string_append_printf(report, "%#016"PRIx64", %d, %ld, %"PRId64"\n",
                                   rec->start_addr, rec->trans_count,
                                   rec->insns, rec->exec_count);
            total_insns += (rec->insns * rec->exec_count);
        }

        // Instructions per second
        double insns_per_sec = (double) total_insns / (elapsed_time_mili/1000);

        g_string_append_printf(report, "================= Total instructions ================= : %lld\n", total_insns);
        g_string_append_printf(report, "================= Total instructions per second ================= : %f\n", insns_per_sec);
        // BIG TODO, this needs to be removed, its only here because simbricks has a bug for allowing the plugins to be printed
        for (int i=0 ; i < 100 ; i ++){
            
            // check if output_ips_<i>.txt exists
            char filename[100];
            sprintf(filename, "output_ips_%d.txt", i);
            // check if file exists, then ignore it, if not create it
            if(access(filename, F_OK) == 0){
                continue;
            }else{
                // Open a file to write the output
                FILE *fptr;
                fptr = fopen(filename, "w");
                if (fptr == NULL) {
                    printf("Error!");
                    exit(1);
                }
                fprintf(fptr, "================= Total instructions ================= : %lld\n", total_insns);
                fprintf(fptr, "================= Total instructions per second ================= : %f\n", insns_per_sec);
                // close file
                fclose(fptr);
                break;
            }
        }
        

        g_list_free(it);
        g_mutex_unlock(&lock);
    }else{
        printf("================= No data found ================= \n");
    }



    qemu_plugin_outs(report->str);
}

static void plugin_init(void)
{
    hotblocks = g_hash_table_new(NULL, g_direct_equal);
    // Get the start time of the program in milliseconds using time.h
    clock_gettime(CLOCK_MONOTONIC, &start);
    printf("===================== initialized hotblocks plugin =====================\n");
}

static void vcpu_tb_exec(unsigned int cpu_index, void *udata)
{
    ExecCount *cnt;
    uint64_t hash = (uint64_t) udata;

    g_mutex_lock(&lock);
    cnt = (ExecCount *) g_hash_table_lookup(hotblocks, (gconstpointer) hash);
    /* should always succeed */
    g_assert(cnt);
    cnt->exec_count++;
    g_mutex_unlock(&lock);
}

/*
 * When do_inline we ask the plugin to increment the counter for us.
 * Otherwise a helper is inserted which calls the vcpu_tb_exec
 * callback.
 */
static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    ExecCount *cnt;
    uint64_t pc = qemu_plugin_tb_vaddr(tb);
    size_t insns = qemu_plugin_tb_n_insns(tb);
    uint64_t hash = pc ^ insns;

    g_mutex_lock(&lock);
    cnt = (ExecCount *) g_hash_table_lookup(hotblocks, (gconstpointer) hash);
    if (cnt) {
        cnt->trans_count++;
    } else {
        cnt = g_new0(ExecCount, 1);
        cnt->start_addr = pc;
        cnt->trans_count = 1;
        cnt->insns = insns;
        g_hash_table_insert(hotblocks, (gpointer) hash, (gpointer) cnt);
    }

    g_mutex_unlock(&lock);

    if (do_inline) {
        qemu_plugin_register_vcpu_tb_exec_inline(tb, QEMU_PLUGIN_INLINE_ADD_U64,
                                                 &cnt->exec_count, 1);
    } else {
        qemu_plugin_register_vcpu_tb_exec_cb(tb, vcpu_tb_exec,
                                             QEMU_PLUGIN_CB_NO_REGS,
                                             (void *)hash);
    }
}

QEMU_PLUGIN_EXPORT
int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                        int argc, char **argv)
{
    if (argc && strcmp(argv[0], "inline") == 0) {
        do_inline = true;
    }

    plugin_init();

    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);
    return 0;
}
