#include "exec/custom-memory-device.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"



CustomMemoryDevice * singleton;

CustomMemoryDevice *get_new_custom_memory_device(unsigned long long base, unsigned long long size){
    assert(singleton == NULL);
    DeviceState        *dev = qdev_new(TYPE_CUSTOM_MEMORY_DEVICE);
    CustomMemoryDevice *memory = singleton = CUSTOM_MEMORY_DEVICE(dev);


    qdev_prop_set_uint64(DEVICE(dev), "base", base);
    qdev_prop_set_uint64(DEVICE(dev), "size", size);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    return memory;
}

CustomMemoryDevice *get_custom_memory_device_singleton(void){
    return singleton;
}



static Property props[] = {
    // Pooria TODO : check if the base is correct
    DEFINE_PROP_UINT64("base", CustomMemoryDevice, base, 0x400000000ULL),
    DEFINE_PROP_UINT64("size", CustomMemoryDevice, size, 0x100000000ULL),
    DEFINE_PROP_END_OF_LIST()
};


static MemTxResult mem_wr(void *opaque, hwaddr addr, uint64_t value, unsigned size, MemTxAttrs attrs) {
    // qemu_printf("mem_wr by poori at %lx\n", addr);
    CustomMemoryDevice *memory = (CustomMemoryDevice *)opaque;

    // Check for out-of-bounds access
    if (addr + size > memory->size) {
        qemu_printf("DMA write out of bounds: addr=%lx, size=%u\n", addr, size);
        return MEMTX_ERROR;
    }

    // Copy data from the value to the memory buffer
    memcpy(memory->data + addr, &value, size);

    return MEMTX_OK;
}

static MemTxResult mem_rd(void *opaque, hwaddr addr, uint64_t *value, unsigned size, MemTxAttrs attrs) {
    qemu_printf("mem_rd by poori at %lx\n", addr);

    CustomMemoryDevice *memory = (CustomMemoryDevice *)opaque;

    // Check for out-of-bounds access
    if (addr + size > memory->size) {
        qemu_printf("DMA read out of bounds: addr=%lx, size=%u\n", addr, size);
        return MEMTX_ERROR;
    }

    // Initialize the value to zero
    *value = 0;

    // Copy data from the memory buffer to the value
    memcpy(value, memory->data + addr, size);

    return MEMTX_OK;
}
static const MemoryRegionOps mem_ops = {
    .read_with_attrs  = mem_rd,
    .write_with_attrs = mem_wr,
    .endianness       = DEVICE_LITTLE_ENDIAN,
    .valid            = {
        .min_access_size = 1,
        .max_access_size = 8,
        .unaligned       = false
    },
    .impl             = {
        .min_access_size = 1,
        .max_access_size = 8,
        .unaligned       = false
    }
};

static void realize_custom_memory(DeviceState *dev, Error **errp) {
    MemoryRegion * sys_mem = get_system_memory();
    CustomMemoryDevice *memory = CUSTOM_MEMORY_DEVICE(dev);


    memory->data = g_malloc0(memory->size);
    // assert(llc->idx_mask);

    qemu_printf("realize_custom_memory by poori at size %lx\n and base %lx\n", memory->size, memory->base);

    memory_region_init_io(&memory->mr, OBJECT(dev), &mem_ops, memory, TYPE_CUSTOM_MEMORY_DEVICE, memory->size);

    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &memory->mr);
    memory_region_add_subregion_overlap(sys_mem, memory->base, &memory->mr, 10);
}


static void reset(DeviceState *dev) {
    CustomMemoryDevice *memory = CUSTOM_MEMORY_DEVICE(dev);

    // Zero out the data buffer
    memset(memory->data, 0, memory->size);
}
static void class_init(ObjectClass *klass, void *data) {
    DeviceClass *dev = DEVICE_CLASS(klass);

    device_class_set_props(dev, props);

    dev->realize = realize_custom_memory;
    dev->reset   = reset;
}

static const TypeInfo type_info = {
    .name          = TYPE_CUSTOM_MEMORY_DEVICE,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(CustomMemoryDevice),
    .class_init    = class_init
};

static void type_init_fn(void) {
    type_register(&type_info);
}

type_init(type_init_fn)
