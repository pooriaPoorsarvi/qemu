#ifndef CUSTOM_MEMORY_DEVICE_H
#define CUSTOM_MEMORY_DEVICE_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"



typedef struct CustomMemoryDevice {
    SysBusDevice        parent;
    MemoryRegion        mr;

    uint64_t            base;
    uint64_t            size;

    uint8_t *data;         // Pointer to the data buffer
    // RAMBlock *data;

} CustomMemoryDevice;

#define TYPE_CUSTOM_MEMORY_DEVICE "custom.memory"
#define CUSTOM_MEMORY_MAX_SIZE 0xffffffff80000000lu

DECLARE_INSTANCE_CHECKER(CustomMemoryDevice,
                         CUSTOM_MEMORY_DEVICE,
                         TYPE_CUSTOM_MEMORY_DEVICE)

CustomMemoryDevice *get_new_custom_memory_device();
CustomMemoryDevice *get_custom_memory_device_singleton(void);


#endif