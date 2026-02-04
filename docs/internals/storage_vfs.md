# Storage & VFS Subsystem Internals

## 1. Overview
The Virtual File System (VFS) provides a unified interface for all storage operations, abstracting physical file systems (ext2, FAT32) and pseudo-filesystems (procfs, devfs).

## 2. VFS Architecture

### Node Hierarchy
```ascii
[ / (Root) ]
    |
    +-- [ bin/ ]
    |     |
    |     +-- [ init ]
    |
    +-- [ dev/ ]
    |     |
    |     +-- [ sda ] (Block Dev)
    |     +-- [ tty0 ] (Char Dev)
    |
    +-- [ mnt/ ]
          |
          +-- [ data/ ] -> (Mounted ext2)
```

### Data Structures
```c
struct fs_node {
    char name[64];
    uint32_t flags;      // Read-only, hidden, system
    uint32_t refcount;   // Active handles
    read_type_t read;    // Function pointer
    write_type_t write;  // Function pointer
    void* impl;          // Driver-specific data
};
```

## 3. Filesystem Registration
Drivers register capabilities via `fs_register`, enabling the kernel to mount partitions by type name.

### Mounting Flow
1.  **Probe**: `vfs_mount` calls the filesystem driver's `mount` function.
2.  **Superblock Read**: Driver reads the superblock from the block device.
3.  **Root Node**: Driver populates the `fs_node` for the root of the mounted partition.
4.  **Attach**: VFS attaches this root node to the mount point directory.

### API Reference
- `fs_register(type)`: Registers a new FS driver.
- `fs_mount(path, device, type)`: Mounts a device to a path.

## 4. Inode Caching (`inode_cache`)
To reduce disk I/O, frequently accessed inodes are cached in memory.

### Configuration (`vfs_config.h`)
```c
#define INODE_CACHE_SIZE 1024
#define MAX_OPEN_FILES 256
#define SYNC_INTERVAL_MS 5000
```

## 5. Journaling (`journal`)
Provides crash consistency for metadata updates using a Write-Ahead Log (WAL).

### Transaction Lifecycle
1.  **Begin**: `journal_begin()` starts an atomic op.
2.  **Log**: Updates written to journal area.
3.  **Commit**: Commit block written.
4.  **Checkpoint**: Data written to final location.

## 6. Implementation Guide
To add a new filesystem:
1.  Define `fs_type_t`.
2.  Implement `read`, `write`, `readdir` callbacks.
3.  Call `fs_register` in `kernel_init`.
