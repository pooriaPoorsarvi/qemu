#include "exec/custom-memory-device.h"
#include "hw/qdev-properties.h"
#include "include/hw/boards.h"
#include "qapi/error.h"
#include "exec/address-spaces.h"
#include "include/qemu/qemu-print.h"



CustomMemoryDevice * singleton;

CustomMemoryDevice *get_new_custom_memory_device(FarOffMemory * far_off_memory) {
    u_int64_t base = far_off_memory->base;
    u_int64_t size = far_off_memory->size;
    assert(singleton == NULL);
    DeviceState        *dev = qdev_new(TYPE_CUSTOM_MEMORY_DEVICE);
    CustomMemoryDevice *memory = singleton = CUSTOM_MEMORY_DEVICE(dev);


    qdev_prop_set_uint64(DEVICE(dev), "base", base);
    qdev_prop_set_uint64(DEVICE(dev), "size", size);

    qemu_printf("Initializing custom memory");
    
    if (far_off_memory->socket != NULL) {
        qemu_printf("Initializing SimBricks memory interface");
        qdev_prop_set_bit(DEVICE(dev), "uses_socket", true);
        init_new_simbricks_mem_if(far_off_memory->socket);
    }else{
        qdev_prop_set_bit(DEVICE(dev), "uses_socket", false);
    }
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    return memory;
}

CustomMemoryDevice *get_custom_memory_device_singleton(void){
    return singleton;
}



static Property props[] = {
    // Pooria TODO : check if the base is correct
    DEFINE_PROP_UINT64("base", CustomMemoryDevice, base, 0x200000000ULL),
    DEFINE_PROP_UINT64("size", CustomMemoryDevice, size, 0x100000000ULL),
    DEFINE_PROP_BOOL("uses_socket", CustomMemoryDevice, uses_socket, true),

    DEFINE_PROP_END_OF_LIST()
};


static MemTxResult mem_wr(void *opaque, hwaddr addr, uint64_t value, unsigned size, MemTxAttrs attrs) {
    CustomMemoryDevice *memory = (CustomMemoryDevice *)opaque;

    // Check for out-of-bounds access
    if (addr + size > memory->size) {
        qemu_printf("DMA write out of bounds: addr=%lx, size=%u\n", addr, size);
        return MEMTX_ERROR;
    }

    // Copy data from the value to the memory buffer
    if (!memory->uses_socket){
        memcpy(memory->data + addr, &value, size);
    }
    else if (simbricks_mem_write(addr, &value, size)) {
        return MEMTX_ERROR; // Sid TODO: more descriptive error here
    }

    return MEMTX_OK;
}

static MemTxResult mem_rd(void *opaque, hwaddr addr, uint64_t *value, unsigned size, MemTxAttrs attrs) {

    CustomMemoryDevice *memory = (CustomMemoryDevice *)opaque;

    // Check for out-of-bounds access
    if (addr + size > memory->size) {
        return MEMTX_ERROR;
    }

    // Initialize the value to zero
    *value = 0;

    // Copy data from the memory buffer to the value
    if (!memory->uses_socket){
        memcpy(value, memory->data + addr, size);
    }
    else if (simbricks_mem_read(addr, value, size)) {
        return MEMTX_ERROR; // Sid TODO: more descriptive error here
    }

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

    if (!memory->uses_socket) {
        memory->data = g_malloc0(memory->size);
    }else{
        memory->data = NULL;
    }

    qemu_printf("custom memory realized with size: 0x%llx\n starting at 0x%llx\n", memory->size, memory->base);

    memory_region_init_io(&memory->mr, OBJECT(dev), &mem_ops, memory, TYPE_CUSTOM_MEMORY_DEVICE, memory->size);

    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &memory->mr);
    memory_region_add_subregion_overlap(sys_mem, memory->base, &memory->mr, 10);
}


static void reset(DeviceState *dev) {
    CustomMemoryDevice *memory = CUSTOM_MEMORY_DEVICE(dev);

    // Zero out the data buffer
    if (!memory->uses_socket){
        memset(memory->data, 0, memory->size);
    }
}
static void class_init(ObjectClass *klass, void *data) {
    DeviceClass *dev = DEVICE_CLASS(klass);

    device_class_set_props(dev, props);

    dev->realize = realize_custom_memory;
    dev->reset   = reset;
    dev->unrealize = NULL;
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
