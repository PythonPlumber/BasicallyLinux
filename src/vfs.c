#include "vfs.h"
#include "types.h"
#include "util.h"

static fs_node_t* vfs_root_node = 0;

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
    if (!vfs_root_node || !name) {
        return 0;
    }

    fs_node_t* current = vfs_root_node;
    
    // Skip leading slashes
    while (*name == '/') {
        name++;
    }
    
    if (*name == '\0') {
        return current;
    }

    char component[256];
    while (*name) {
        uint32_t i = 0;
        while (*name && *name != '/' && i < 255) {
            component[i++] = *name++;
        }
        component[i] = '\0';
        
        if (*name && *name != '/') {
            // Component too long
            return 0;
        }
        
        if (*name == '/') {
            name++;
        }
        
        // Find component in current directory
        if (current->finddir) {
            current = current->finddir(current, component);
            if (!current) {
                return 0;
            }
        } else if (current->impl) {
            // Fallback for simple ramdisk/array based VFS
            fs_node_t* nodes = (fs_node_t*)current->impl;
            uint32_t count = current->length;
            fs_node_t* found = 0;
            for (uint32_t j = 0; j < count; ++j) {
                if (strcmp((const char*)nodes[j].name, component) == 0) {
                    found = &nodes[j];
                    break;
                }
            }
            if (!found) {
                return 0;
            }
            current = found;
        } else {
            return 0;
        }
    }
    
    return current;
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
