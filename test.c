#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "disk.h"
#include "fs.h"


typedef struct{
int init;
int fat_idx; // First block of the FAT
int fat_len; // Length of FAT in blocks
int dir_idx; // First block of directory
int dir_len; // Length of directory in blocks
int data_idx; // First block of file-data
}super_block;

typedef struct{
int used; // Is this file-”slot” in use
char name [MAX_F_NAME + 1]; // DOH!
int size; // file size
int head; // first data block of file
int ref_cnt;
// how many open file descriptors are there?
// ref_cnt > 0 -> cannot delete file
}dir_entry;

typedef struct{
int used; // fd in use
int file; // the first block of the file
// (f) to which fd refers too
int offset; // position of fd within f
}file_descriptor;

super_block fs;
file_descriptor fildes[MAX_FILDES]; // 32
int FAT[DATA_SIZE]; // Will be populated with the FAT data
dir_entry DIR[MAX_FILES]; // Will be populated with
//the directory data

int make_fs(char *disk_name)
{
    if(make_disk(disk_name)==-1)
    {
        fprintf(stderr, "error making file system\n");  
        return -1; 
    }
    if(open_disk(disk_name)==-1)
    {
        fprintf(stderr, "error opening file system\n");  
        return -1; 
    }
    super_block *sblock = calloc(1,sizeof(super_block));
    sblock->fat_len=4;
    sblock->fat_idx=65;
    sblock->dir_len=64;
    sblock->dir_idx=1;
    sblock->init=1;
    sblock->data_idx=4096;
    char* sblockBuffer = malloc(sizeof(super_block));
    memcpy(sblockBuffer, sblock, sizeof(super_block));
    block_write(0, sblockBuffer);
    free(sblock);
    free(sblockBuffer);
    int i;
    char* dirBuffer = malloc(sizeof(dir_entry));
    dir_entry *dir = calloc(1,sizeof(dir_entry));
    for(i=1;i<65;i++) //writing directory into disk
    {
        dir->head=-1;
        memcpy(dirBuffer, dir, sizeof(dir_entry));
        block_write(i,dirBuffer);
    }
    free(dirBuffer);
    free(dir);

    char* fatBuffer = malloc(BLOCK_SIZE);
    int* fat = calloc(1024,sizeof(int));
    for(i=65;i<69;i++)
    {
        int j;
        for(j=0;j<1024;j++)
        {
            fat[j]=FREE;
        }

        memcpy(fatBuffer, fat, BLOCK_SIZE);
        block_write(i,fatBuffer);

    }
    free(fatBuffer);
    free(fat);
    close_disk();
    return 0;
}

int mount_fs(char *disk_name)
{
    //restore superblock
    if(open_disk(disk_name)==-1) 
    {
        return -1;
    }

    char* sblockMount = (char *)calloc(1,BLOCK_SIZE);

    block_read(0,sblockMount);
    memcpy(&fs,sblockMount,sizeof(super_block));
    free(sblockMount);
    int i;
    for(i=1;i<65;i++) //writing directory into disk
    {
        char* dirBufferMount = malloc(BLOCK_SIZE);
        block_read(i, dirBufferMount);
        memcpy(&DIR[i-1],dirBufferMount,sizeof(dir_entry));
        free(dirBufferMount);
    }
    for(i = 65; i<69;i++)
    {
        char* FatBufferMount = malloc(BLOCK_SIZE);
        block_read(i, FatBufferMount);
        memcpy((FAT+(i-65)*1024),FatBufferMount,BLOCK_SIZE);  
        free(FatBufferMount);
    }

    for(i =0;i<32;i++)
    {
        fildes[i].used=0;
    }
    return 0;
}

int umount_fs(char *disk_name)
{
    super_block *sblock = calloc(1,sizeof(super_block));
    char* sblockMount = (char *)calloc(1,BLOCK_SIZE);
    block_read(0,sblockMount);
    memcpy(sblock,sblockMount,sizeof(super_block));
    if(sblock->init!=1)
    {
        return -1;
    }
    if(fs.init!=1)
    {
        return -1;
    }
    char* sblockBuffer = malloc(sizeof(super_block));
    memcpy(sblockBuffer, &fs, sizeof(super_block));
    block_write(0, sblockBuffer);
    free(sblockBuffer);
    int i;
    char* dirBuffer = malloc(sizeof(dir_entry));
    for(i=1;i<65;i++) //writing directory into disk
    {
        memcpy(dirBuffer, &DIR[i], sizeof(dir_entry));
        block_write(i,dirBuffer);
    }
    free(dirBuffer);

    char* fatBuffer = malloc(BLOCK_SIZE);
    for(i=65;i<69;i++)
    {
        memcpy(fatBuffer, &FAT[i], BLOCK_SIZE);
        block_write(i,fatBuffer);
    }
    free(fatBuffer);
    close_disk();
    return 0;
}


int findSpecificDir(char* name)
{
    int i;
    for(i =0; i<MAX_FILES;i++)
    {
        if((DIR[i].used==1) && (strcmp(DIR[i].name,name)==0))
        {
            return i;
        }
    }
    return -1;
}

int findEmptyDirPath()
{
    int i;
    for(i =0; i<MAX_FILES;i++)
    {
        if(DIR[i].used==0)
        {
            return i;
        }
    }
    return -1;
}

int findEmptyFDPath()
{
    int i;
    for(i =0; i<MAX_FILDES;i++)
    {
        if(fildes[i].used==0)
        {
            return i;
        }
    }
    return -1;
}

int findEmptyBlock()
{
    int i;
    for(i=0;i<DATA_SIZE;i++)
    {
        if(FAT[i]==-2)
        {
            return i;
        }
    }
    return -1;
}


int fs_open(char *name)
{
    int dirIndex = findSpecificDir(name);
    if (dirIndex==-1)
    {
        return -1;
    }
    int FD = findEmptyFDPath();
    if (FD == -1)
    {
        return -1;
    }
    
    DIR[dirIndex].ref_cnt++;
    fildes[FD].used=1;
    fildes[FD].offset=0;
    fildes[FD].file=dirIndex;
    return FD;
}

int fs_close(int filde)
{
    if (fildes[filde].used==0)
    {
        return -1;
    }
    fildes[filde].used=0;
    fildes[filde].offset=0;
    int dirIndex= fildes[filde].file;
    DIR[dirIndex].ref_cnt--;
    return 0;
}

int fs_delete(char *name)
{
    int filde = findSpecificDir(name);
    if(filde == -1)
    {
        return -1;
    }else
    {
        if(DIR[filde].ref_cnt>0)
        {
            DIR[filde].ref_cnt--;
        }
        else
        {
            DIR[filde].used =0;
        }
    }
    return 0;
}

int fs_create(char *name)
{
    if(sizeof(name)/sizeof(char)>15)
    {
        return -1;
    }
    int dirIndex = findEmptyDirPath();
    if(dirIndex == -1)
    {
        return -1;
    }
    int i;
    for(i=0;i<strlen(name);i++)
    {
        DIR[dirIndex].name[i] = name[i];
    }
    DIR[dirIndex].used = 1;
    return 0;
}



int fs_write(int filde, void *buf, size_t nbyte)
{
    if (fildes[filde].used==0)
    {
        return -1;
    }
    int bytesize = nbyte;
    int dirIndex = fildes[filde].file;
    int byteStored = 0;
    int block = findEmptyBlock();
    DIR[dirIndex].head=block;

    while(bytesize!=0 && block!=-1)
    {
        if (bytesize>4096)
        {
            char* writebuf = malloc(BLOCK_SIZE);
            memcpy(writebuf, buf, BLOCK_SIZE);
            block_write(block+4096,writebuf); //offset for data
            free(writebuf);
            bytesize-=4096;
            buf += 4096;
            byteStored+=4096;
            FAT[block] = -1;
            int nextblock = findEmptyBlock();
            FAT[block] = nextblock; //if no more emptyBLock, it will still assign -1 meaning EOF
            block = nextblock;

        }
        else
        {
            char* writebuf = malloc(BLOCK_SIZE);        
            memcpy(writebuf, buf, bytesize);
            block_write(block+4096,writebuf); //offset for data
            free(writebuf);
            buf += bytesize;
            bytesize -= bytesize;
            byteStored+=bytesize;
            FAT[block] = -1;
        }
    }
    DIR[dirIndex].size=byteStored;
    return byteStored;
}

int fs_read(int filde, void *buf, size_t nbyte)
{
    if (fildes[filde].used==0)
    {
        return -1;
    }
    void *headbuf = buf;
    int bytesize = nbyte;
    int dirIndex = fildes[filde].file;
    int block = DIR[dirIndex].head;  
    while((bytesize != 0 )&&(block!=-1))
    {
        if(bytesize>4096)
        {
            char* readbuf = malloc(BLOCK_SIZE);  
            block_read(4096+block,readbuf);      
            memcpy(buf,readbuf,4096);
            buf+=4096;
            bytesize-=4096;
            free(readbuf);
        }
        else
        {
            char* readbuf = malloc(BLOCK_SIZE);  
            block_read(4096+block,readbuf);      
            memcpy(buf,readbuf,bytesize);
            buf+=bytesize;
            bytesize-=bytesize;
            free(readbuf);
        }
    }

    if(bytesize!=0)
    {
        if(bytesize>4096)
        {
            char* readbuf = malloc(BLOCK_SIZE);  
            block_read(4096+block,readbuf);      
            memcpy(buf,readbuf,4096);
            buf+=4096;
            bytesize-=4096;
            free(readbuf);
        }
        else
        {
            char* readbuf = malloc(BLOCK_SIZE);  
            block_read(4096+block,readbuf);      
            memcpy(buf,readbuf,bytesize);
            buf+=bytesize;
            bytesize-=bytesize;
            free(readbuf);
        }
    }   
    buf = headbuf;
    return 0;
}

int fs_get_filesize(int filde)
{
    if(fildes[filde].used==0)
    {
        return -1;
    }else
    {
        return DIR[fildes[filde].file].size;
    }
}

int fs_listfiles(char **files)
{
    char **head = files;
    int i;
    for(i =0;i<MAX_FILES;i++)
    {
        if(DIR[i].used)
        {
            *files = DIR[i].name;
            files++;
        }
    }
}

int fs_lseek(int filde, off_t offset)
{
    if(offset>fs_get_filesize(filde)||offset<0)
    {
        return -1;
    }else
    {
        fildes[filde].offset=offset;
    }
    return 0;
}

int main()
{
    mount_fs("idk");
    fs_create("file1");
    char* test = "hi";
    int fd = fs_open("file1");
    fs_write(fd,test,2);
    char* buf = (char*)malloc(5);
    fs_read(fd,buf,5);
    printf("%s",buf);
    
}