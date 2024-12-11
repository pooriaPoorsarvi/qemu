#ifndef CUSTOM_MEMORY_DEVICE_H
#define CUSTOM_MEMORY_DEVICE_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "include/hw/boards.h"
#include "exec/simbricks_mem.h"



typedef struct CustomMemoryDevice {
    SysBusDevice        parent;
    MemoryRegion        mr;

    uint64_t            base;
    uint64_t            size;

    bool uses_socket;
    uint8_t *data;         // Pointer to the data buffer if there is no socket
    // RAMBlock *data;

} CustomMemoryDevice;

#define TYPE_CUSTOM_MEMORY_DEVICE "custom.memory"

DECLARE_INSTANCE_CHECKER(CustomMemoryDevice,
                         CUSTOM_MEMORY_DEVICE,
                         TYPE_CUSTOM_MEMORY_DEVICE)

CustomMemoryDevice *get_new_custom_memory_device(FarOffMemory * far_off_memory);
CustomMemoryDevice *get_custom_memory_device_singleton(void);


#endif