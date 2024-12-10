# SIMPLE FILE SYSTEM IMPLEMENTATION
## Project overview
This project involves implementing a simple file system on top of a virtual disk. The goal is to gain a deeper understanding of file system implementation by creating a library that offers basic file system operations such as opening, reading, and writing files. The file system operates on a single virtual disk file, stored within the Linux file system.

### make_fs
Purpose: Initializes a new file system on a virtual disk.
Creates and opens the virtual disk, sets up the superblock with information about the file system structure and initializes the directory and the FAT (File Allocation Table) to default values.

### mount_fs
Purpose: Mounts an existing file system, making it ready for operations.
Reads the superblock, FAT, and directory from the disk into memory. Resets all file descriptors to unused state.

### unmount_fs
Purpose: Saves all changes and unmounts the file system.
Writes the updated superblock, FAT, and directory back to the disk and closes the disk.

### fs_open
Purpose: Opens a file and returns a file descriptor.
Increases the reference count for the file in the directory and allocates a file descriptor and sets its initial offset.
Returns: File descriptor or -1 if the file is not found or the limit is reached.

### fs_close
Purpose: Closes a file descriptor.
Decreases the file's reference count in the directory and frees the file descriptor slot.
Returns: 0 on success, -1 on failure.

### fs_create
Purpose: Creates a new empty file.
Adds a new entry in the directory with the file name and initializes its metadata.
Returns: 0 on success, -1 if the name is too long or the file already exists.

### fs_delete
Purpose: Deletes a file.
Frees all data blocks associated with the file. Removes the directory entry if the file is not currently open.
Returns: 0 on success, -1 if the file is open or does not exist.

### fs_write
Purpose: Writes data to a file.
Allocates additional blocks as necessary and writes data from buf into the file starting at the current offset.
Returns: Number of bytes written or -1 on failure.

### fs_read
Purpose: Reads data from a file.
Reads data blocks into a buffer starting from the current offset.
Returns: Number of bytes read or -1 on failure.

### fs_get_filesize
Purpose: Returns the size of a file.
Checks the directory entry for the file size
Returns: Size in bytes or -1 if the descriptor is invalid.

### fs_listfiles
Purpose: Lists all files in the directory.
Allocates memory for an array of file names and populates the array with the names of existing files.
Returns: 0 on success.

### fs_lseek
Purpose: Sets the file pointer to a new position.
Updates the offset in the file descriptor.
Returns: 0 on success, -1 if the offset is invalid.

### fs_truncate
Purpose: Reduces the size of a file.
Frees blocks beyond the specified lengthand updates the file size and offset if necessary.
Returns: 0 on success, -1 on failure.



