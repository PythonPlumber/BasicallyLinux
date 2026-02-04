#ifndef VFS_H
#define VFS_H

#include "types.h"

typedef struct fs_node fs_node_t;

typedef uint32_t (*read_type_t)(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
typedef uint32_t (*write_type_t)(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
typedef void (*open_type_t)(fs_node_t* node);
typedef void (*close_type_t)(fs_node_t* node);
typedef uint32_t (*fsync_type_t)(fs_node_t* node);
typedef uint32_t (*readdir_type_t)(fs_node_t* node, uint32_t index, fs_node_t* out);
typedef fs_node_t* (*finddir_type_t)(fs_node_t* node, const char* name);

struct fs_node {
    uint8_t name[64];
    uint32_t length;
    uint32_t flags;
    uint32_t refcount;
    uint32_t inode_id;
    uint32_t parent_id;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t nlink;
    read_type_t read;
    write_type_t write;
    open_type_t open;
    close_type_t close;
    fsync_type_t fsync;
    readdir_type_t readdir;
    finddir_type_t finddir;
    void* impl;
};

void vfs_init(void);
void vfs_set_root(fs_node_t* root);
fs_node_t* vfs_root(void);
fs_node_t* vfs_find(const char* name);
uint32_t vfs_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
uint32_t vfs_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
void vfs_open_node(fs_node_t* node);
void vfs_close_node(fs_node_t* node);
uint32_t vfs_readdir(fs_node_t* node, uint32_t index, fs_node_t* out);
fs_node_t* vfs_finddir(fs_node_t* node, const char* name);
uint32_t vfs_fsync(fs_node_t* node);

#endif
