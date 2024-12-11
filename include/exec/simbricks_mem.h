#include "qemu/osdep.h"
#include "qemu/timer.h"
#include "qemu/option.h"
#include "qemu/config-file.h"
#include "include/hw/boards.h"

#include <simbricks/mem/if.h>

#define SIMBRICKS_MEM_CLOCK QEMU_CLOCK_VIRTUAL


typedef struct SimbricksMemState {
    struct SimbricksMemIf memif;

    int64_t ts_base;
    QEMUTimer *timer_dummy;
    QEMUTimer *timer_sync;
    QEMUTimer *timer_poll;
} SimbricksMemState;

int init_new_simbricks_mem_if(FarOffSocket * far_off_socket);
int uninit_simbricks_mem_if(void);

int simbricks_mem_read(uint64_t addr, uint64_t *value, unsigned size);
int simbricks_mem_write(uint64_t addr, uint64_t *value, unsigned size);