#ifndef FS_H
#define FS_H

#define MAX_F_NAME 15
#define MAX_FILDES 32
#define MAX_FILES 64
#define DATA_SIZE 4096
#define FREE -2
/*This function creates a fresh (and empty) file system on the virtual disk with name disk_name.
As part of this function, you should first invoke make_disk(disk_name) to create a new disk.
Then, open this disk and write/initialize the necessary meta-information for your file system so
that it can be later used (mounted). The function returns 0 on success, and -1 if the disk
disk_name could not be created, opened, or properly initialized.*/
int make_fs(char *disk_name);

int mount_fs(char *disk_name);

int umount_fs(char *disk_name);

int fs_open(char *name);

int fs_close(int filde);

int fs_delete(char *name);

int fs_create(char *name);

int fs_write(int filde, void *buf, size_t nbyte);

int fs_read(int filde, void *buf, size_t nbyte);

int fs_get_filesize(int filde);

int fs_listfiles(char ***files);

int fs_lseek(int filde, off_t offset);

int fs_truncate(int fildes, off_t length);

#endif