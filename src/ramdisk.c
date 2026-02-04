#include "ramdisk.h"
#include "util.h"

typedef struct {
    uint32_t nfiles;
} initrd_header_t;

typedef struct {
    uint8_t name[64];
    uint32_t offset;
    uint32_t length;
} initrd_file_header_t;

static uint8_t* initrd_base;
static initrd_header_t* initrd_header;
static initrd_file_header_t* file_headers;
static fs_node_t root_node;
static fs_node_t file_nodes[64];

static uint32_t ramdisk_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    initrd_file_header_t* header = (initrd_file_header_t*)node->impl;
    if (offset >= header->length) {
        return 0;
    }
    if (offset + size > header->length) {
        size = header->length - offset;
    }
    memcpy(buffer, initrd_base + header->offset + offset, size);
    return size;
}

fs_node_t* ramdisk_init(uint32_t module_start, uint32_t module_size) {
    (void)module_size;
    initrd_base = (uint8_t*)module_start;
    initrd_header = (initrd_header_t*)initrd_base;
    file_headers = (initrd_file_header_t*)(initrd_base + sizeof(initrd_header_t));

    for (uint32_t i = 0; i < initrd_header->nfiles && i < 64; ++i) {
        fs_node_t* node = &file_nodes[i];
        memcpy(node->name, file_headers[i].name, 64);
        node->length = file_headers[i].length;
        node->flags = 0;
        node->refcount = 0;
        node->read = ramdisk_read;
        node->write = 0;
        node->open = 0;
        node->close = 0;
        node->impl = &file_headers[i];
    }

    memset(&root_node, 0, sizeof(fs_node_t));
    root_node.name[0] = '/';
    root_node.name[1] = 0;
    root_node.length = initrd_header->nfiles;
    root_node.flags = 0;
    root_node.refcount = 0;
    root_node.read = 0;
    root_node.write = 0;
    root_node.open = 0;
    root_node.close = 0;
    root_node.impl = file_nodes;

    return &root_node;
}
