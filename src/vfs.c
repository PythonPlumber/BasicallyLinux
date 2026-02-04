#include "vfs.h"
#include "types.h"

static fs_node_t* vfs_root_node = 0;

static int vfs_strcmp(const uint8_t* a, const char* b) {
    uint32_t i = 0;
    while (a[i] && b[i]) {
        if (a[i] != (uint8_t)b[i]) {
            return (int)a[i] - (int)(uint8_t)b[i];
        }
        ++i;
    }
    return (int)a[i] - (int)(uint8_t)b[i];
}

void vfs_init(void) {
    vfs_root_node = 0;
}

void vfs_set_root(fs_node_t* root) {
    vfs_root_node = root;
}

fs_node_t* vfs_root(void) {
    return vfs_root_node;
}

fs_node_t* vfs_find(const char* name) {
    if (!vfs_root_node || !vfs_root_node->impl) {
        return 0;
    }
    if (!name) {
        return 0;
    }
    while (*name == '/') {
        name++;
    }
    for (uint32_t i = 0; name[i]; ++i) {
        if (name[i] == '/') {
            return 0;
        }
    }
    fs_node_t* nodes = (fs_node_t*)vfs_root_node->impl;
    uint32_t count = vfs_root_node->length;
    for (uint32_t i = 0; i < count; ++i) {
        if (vfs_strcmp(nodes[i].name, name) == 0) {
            return &nodes[i];
        }
    }
    return 0;
}

uint32_t vfs_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node || !node->read) {
        return 0;
    }
    return node->read(node, offset, size, buffer);
}

uint32_t vfs_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    if (!node || !node->write) {
        return 0;
    }
    return node->write(node, offset, size, buffer);
}

void vfs_open_node(fs_node_t* node) {
    if (!node) {
        return;
    }
    node->refcount++;
    if (node->open) {
        node->open(node);
    }
}

void vfs_close_node(fs_node_t* node) {
    if (!node) {
        return;
    }
    if (node->refcount > 0) {
        node->refcount--;
    }
    if (node->close && node->refcount == 0) {
        node->close(node);
    }
}

uint32_t vfs_readdir(fs_node_t* node, uint32_t index, fs_node_t* out) {
    if (!node || !node->readdir) {
        return 0;
    }
    return node->readdir(node, index, out);
}

fs_node_t* vfs_finddir(fs_node_t* node, const char* name) {
    if (!node || !node->finddir) {
        return 0;
    }
    return node->finddir(node, name);
}

uint32_t vfs_fsync(fs_node_t* node) {
    if (!node || !node->fsync) {
        return 0;
    }
    return node->fsync(node);
}
