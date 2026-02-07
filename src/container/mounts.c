#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <limits.h>
#include <sys/syscall.h>

#ifndef __NR_pivot_root
#define __NR_pivot_root 155
#endif

#include "nk_container.h"

/* Default mounts for container */
static const struct {
    const char *source;
    const char *target;
    const char *type;
    unsigned long flags;
    const char *options;
} default_mounts[] = {
    { "proc",     "/proc",     "proc",     0,                                NULL },
    { "sysfs",    "/sys",      "sysfs",    0,                                NULL },
    { "tmpfs",    "/dev",      "tmpfs",    MS_NOSUID | MS_STRICTATIME,       "mode=755" },
    { "devpts",   "/dev/pts",  "devpts",   MS_NOSUID | MS_NOEXEC,            NULL },
    { "tmpfs",    "/dev/shm",  "tmpfs",    MS_NOSUID | MS_NODEV,             NULL },
    { "tmpfs",    "/dev/mqueue", "tmpfs",  MS_NOSUID | MS_NODEV,             NULL },
};

#define DEFAULT_MOUNTS_COUNT (sizeof(default_mounts) / sizeof(default_mounts[0]))

/**
 * nk_mount_make_private - Make mount private
 */
static int nk_mount_make_private(const char *path) {
    if (mount(NULL, path, NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
        fprintf(stderr, "Error: Failed to make %s private: %s\n",
                path, strerror(errno));
        return -1;
    }
    return 0;
}

/**
 * nk_mount_make_bind - Create bind mount
 */
__attribute__((unused))
static int nk_mount_bind(const char *source, const char *target, unsigned long flags) {
    /* First do a basic bind mount */
    if (mount(source, target, NULL, MS_BIND | MS_REC, NULL) == -1) {
        fprintf(stderr, "Error: Failed to bind mount %s to %s: %s\n",
                source, target, strerror(errno));
        return -1;
    }

    /* Then remount with flags if specified */
    if (flags != 0) {
        if (mount(source, target, NULL, MS_BIND | MS_REMOUNT | flags, NULL) == -1) {
            fprintf(stderr, "Error: Failed to remount %s: %s\n",
                    target, strerror(errno));
            return -1;
        }
    }

    return 0;
}

/**
 * nk_mount_create_device - Create a device node
 */
static int nk_mount_create_device(const char *path, unsigned int mode, int major, int minor) {
    dev_t dev = makedev(major, minor);
    (void)dev;  /* Will be used when device creation is completed */

    if (mknod(path, S_IFCHR | mode, 0600) == -1) {
        fprintf(stderr, "Error: Failed to create device %s: %s\n",
                path, strerror(errno));
        return -1;
    }

    return 0;
}

/**
 * nk_mount_setup_slaves - Create /dev/slurm and /dev/null if needed
 */
static int nk_mount_setup_dev(const char *rootfs) {
    char path[PATH_MAX];

    /* Create /dev/null */
    snprintf(path, sizeof(path), "%s/dev/null", rootfs);
    nk_mount_create_device(path, 0666, 1, 3);

    /* Create /dev/zero */
    snprintf(path, sizeof(path), "%s/dev/zero", rootfs);
    nk_mount_create_device(path, 0666, 1, 5);

    /* Create /dev/full */
    snprintf(path, sizeof(path), "%s/dev/full", rootfs);
    nk_mount_create_device(path, 0666, 1, 7);

    /* Create /dev/random */
    snprintf(path, sizeof(path), "%s/dev/random", rootfs);
    nk_mount_create_device(path, 0666, 1, 8);

    /* Create /dev/urandom */
    snprintf(path, sizeof(path), "%s/dev/urandom", rootfs);
    nk_mount_create_device(path, 0666, 1, 9);

    /* Create /dev/tty */
    snprintf(path, sizeof(path), "%s/dev/tty", rootfs);
    nk_mount_create_device(path, 0666, 5, 0);

    /* Create symlinks */
    snprintf(path, sizeof(path), "%s/dev/fd", rootfs);
    if (symlink("/proc/self/fd", path) == -1 && errno != EEXIST) {
        fprintf(stderr, "Warning: Failed to create symlink %s: %s\n", path, strerror(errno));
    }

    snprintf(path, sizeof(path), "%s/dev/stdin", rootfs);
    if (symlink("/proc/self/fd/0", path) == -1 && errno != EEXIST) {
        fprintf(stderr, "Warning: Failed to create symlink %s: %s\n", path, strerror(errno));
    }

    snprintf(path, sizeof(path), "%s/dev/stdout", rootfs);
    if (symlink("/proc/self/fd/1", path) == -1 && errno != EEXIST) {
        fprintf(stderr, "Warning: Failed to create symlink %s: %s\n", path, strerror(errno));
    }

    snprintf(path, sizeof(path), "%s/dev/stderr", rootfs);
    if (symlink("/proc/self/fd/2", path) == -1 && errno != EEXIST) {
        fprintf(stderr, "Warning: Failed to create symlink %s: %s\n", path, strerror(errno));
    }

    return 0;
}

/**
 * nk_mount_pivot_root - Pivot to new root filesystem
 */
static int nk_mount_pivot_root(const char *new_root) {
    /* Create directory for old root */
    char put_old[PATH_MAX];
    snprintf(put_old, sizeof(put_old), "%s/.pivot_old", new_root);

    if (mkdir(put_old, 0700) == -1 && errno != EEXIST) {
        fprintf(stderr, "Error: Failed to create %s: %s\n",
                put_old, strerror(errno));
        return -1;
    }

    /* Bind mount new root to itself to ensure it's a mount point */
    if (mount(new_root, new_root, NULL, MS_BIND | MS_REC, NULL) == -1) {
        fprintf(stderr, "Error: Failed to bind mount %s: %s\n",
                new_root, strerror(errno));
        return -1;
    }

    /* Pivot root */
    if (syscall(__NR_pivot_root, new_root, put_old) == -1) {
        fprintf(stderr, "Error: Failed to pivot_root: %s\n", strerror(errno));
        return -1;
    }

    /* Change to new root */
    if (chdir("/") == -1) {
        fprintf(stderr, "Error: Failed to chdir to new root: %s\n",
                strerror(errno));
        return -1;
    }

    /* Unmount old root */
    if (umount2("/.pivot_old", MNT_DETACH) == -1) {
        fprintf(stderr, "Warning: Failed to unmount old root: %s\n",
                strerror(errno));
    }

    /* Remove old root directory */
    rmdir("/.pivot_old");

    return 0;
}

/**
 * nk_container_setup_rootfs - Setup container root filesystem
 */
int nk_container_setup_rootfs(const nk_container_ctx_t *ctx) {
    if (!ctx || !ctx->rootfs) {
        fprintf(stderr, "Error: No rootfs specified\n");
        return -1;
    }

    printf("  Setting up rootfs: %s\n", ctx->rootfs);

    /* Make rootfs mount private */
    if (nk_mount_make_private(ctx->rootfs) == -1) {
        return -1;
    }

    /* Mount default filesystems */
    for (size_t i = 0; i < DEFAULT_MOUNTS_COUNT; i++) {
        char target[PATH_MAX];
        snprintf(target, sizeof(target), "%s%s", ctx->rootfs, default_mounts[i].target);

        /* Create target directory if it doesn't exist */
        struct stat st;
        if (stat(target, &st) == -1) {
            if (mkdir(target, 0755) == -1) {
                fprintf(stderr, "Error: Failed to create %s: %s\n",
                        target, strerror(errno));
                continue;
            }
        }

        /* Perform mount */
        if (mount(default_mounts[i].source, target,
                  default_mounts[i].type,
                  default_mounts[i].flags,
                  default_mounts[i].options) == -1) {
            fprintf(stderr, "Warning: Failed to mount %s to %s: %s\n",
                    default_mounts[i].type, target, strerror(errno));
        }
    }

    /* Setup device nodes */
    nk_mount_setup_dev(ctx->rootfs);

    /* Pivot root */
    if (nk_mount_pivot_root(ctx->rootfs) == -1) {
        fprintf(stderr, "Error: Failed to pivot root\n");
        return -1;
    }

    printf("  Root filesystem ready\n");
    return 0;
}

/**
 * nk_container_mount_custom - Mount custom mounts from OCI spec
 */
int nk_container_mount_custom(const nk_oci_mount_t *mounts, size_t len,
                               const char *rootfs) {
    if (!mounts || len == 0) {
        return 0;
    }

    for (size_t i = 0; i < len; i++) {
        char target[PATH_MAX];
        snprintf(target, sizeof(target), "%s%s", rootfs, mounts[i].destination);

        /* Create target directory */
        struct stat st;
        if (stat(target, &st) == -1) {
            if (mkdir(target, 0755) == -1) {
                fprintf(stderr, "Error: Failed to create %s: %s\n",
                        target, strerror(errno));
                continue;
            }
        }

        /* Parse mount flags */
        unsigned long flags = 0;
        if (mounts[i].options) {
            for (size_t j = 0; j < mounts[i].options_len; j++) {
                if (strcmp(mounts[i].options[j], "ro") == 0) {
                    flags |= MS_RDONLY;
                } else if (strcmp(mounts[i].options[j], "nosuid") == 0) {
                    flags |= MS_NOSUID;
                } else if (strcmp(mounts[i].options[j], "noexec") == 0) {
                    flags |= MS_NOEXEC;
                } else if (strcmp(mounts[i].options[j], "nodev") == 0) {
                    flags |= MS_NODEV;
                }
            }
        }

        /* Perform mount */
        if (mount(mounts[i].source, target, mounts[i].type, flags, NULL) == -1) {
            fprintf(stderr, "Warning: Failed to mount %s to %s: %s\n",
                    mounts[i].source, target, strerror(errno));
        } else {
            printf("  Mounted: %s -> %s\n", mounts[i].source, mounts[i].destination);
        }
    }

    return 0;
}
