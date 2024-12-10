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
int blocksNeeded;
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
    if (fildes[filde].used==0 || filde<0)
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
    int dirIndex = findSpecificDir(name);
    if(dirIndex == -1)
    {
        return -1;
    }

    if(DIR[dirIndex].ref_cnt>0)
    {
        return -1;
    }
    DIR[dirIndex].used=0;
    int block = DIR[dirIndex].head;
    if(block==-1)
    {
        return 0;
    }

    while (FAT[block]!=EOF)
    {
        int temp = FAT[block];
        FAT[block]=FREE;
        block = temp;
    }
    FAT[block] = FREE;
    DIR[dirIndex].size=0;
    memset(DIR[dirIndex].name,0,16);
    DIR[dirIndex].head=-1;
    DIR[dirIndex].blocksNeeded=0;
    return 0;
}

int fs_create(char *name)
{
    if(sizeof(name)/sizeof(char)>15)
    {
        return -1;
    }

    if (findSpecificDir(name)!=-1)
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
    DIR[dirIndex].head = -1;
    DIR[dirIndex].blocksNeeded = 0;
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
    int offset = fildes[filde].offset;
    int byteStored = 0;
    int block = DIR[dirIndex].head;
    int currBlocks = DIR[dirIndex].blocksNeeded;
    int numBlocksNeeded = ((bytesize-1 + offset)/4096) + 1;

    if(offset+bytesize-1>DIR[dirIndex].size)
    {
        numBlocksNeeded = ((bytesize-1 + offset)/4096) + 1;
    }else
    {
        numBlocksNeeded = DIR[dirIndex].size/4096 + 1;
    }

    int blocks[numBlocksNeeded];

    int numBlockUsed = 0;
    int i;
    if(currBlocks>=numBlocksNeeded)
    {
        while(FAT[block]!=-1)
        {
            blocks[i]=block;
            block = FAT[block];
            numBlockUsed++; 
        } 
        blocks[i]=block;
        block = FAT[block];
        numBlockUsed++; 
    }
    else
    {
    for(i =0;(i<numBlocksNeeded);i++)
    {
        if(block==-1)
        {
            if(findEmptyBlock()!=-1)
            {
                DIR[dirIndex].head = findEmptyBlock();
                block = DIR[dirIndex].head;
                FAT[block]= -1;
                blocks[i]=block;
                numBlockUsed++;
            }
        }else if(FAT[block]==-1)
        {
            if(findEmptyBlock()!=-1)
            {
                FAT[block]= findEmptyBlock();
                blocks[i]=findEmptyBlock();
                block = FAT[block];
                FAT[block]=-1;
                numBlockUsed++;
            }
            else
            {
                numBlockUsed++; 
                blocks[i]=block;
                break;         
            }
        }else
        {
            blocks[i]=block;
            block = FAT[block];
            numBlockUsed++;
        }
    }
    }
    int blocksInUse[numBlockUsed];
    for(i =0;i<numBlockUsed;i++)
    {
        blocksInUse[i] = blocks[i];
    }
    char *bufTotal = malloc(numBlockUsed*BLOCK_SIZE);
    char* readbuf = malloc(BLOCK_SIZE);
    for(i=0;i<numBlockUsed;i++)
    {
        block_read(blocksInUse[i]+4096,readbuf);
        memcpy((void *)bufTotal,(void *)readbuf,BLOCK_SIZE);
        bufTotal += BLOCK_SIZE;
    }
    bufTotal -= BLOCK_SIZE*numBlockUsed;    
    bufTotal = bufTotal+offset;
    if(nbyte+offset>BLOCK_SIZE*BLOCK_SIZE)
    {
        memmove((void *)bufTotal,buf,(nbyte+offset)-(BLOCK_SIZE*BLOCK_SIZE));
    }
    else
    {
        memcpy((void *)bufTotal,buf,nbyte);
    }
    bufTotal -=offset;    
    char* writebuf = malloc(BLOCK_SIZE);
    for(i=0;i<numBlockUsed;i++)
    {
        memcpy((void *)writebuf,(void *)bufTotal,BLOCK_SIZE);
        block_write(blocksInUse[i]+4096,writebuf);
        bufTotal += BLOCK_SIZE;

    }
    free(readbuf);
    free(writebuf);

    // int index;
    // int count;
    // char* readbuf = malloc(BLOCK_SIZE);
    // int blockn = -1;
    // for(count = 0, index = offset; index<(offset+bytesize)&&index<(numBlockUsed*4096); index++, count++)
    // {
    //     int tempblock = blocksInUse[index/4096];
    //     if (tempblock!=blockn)
    //     {
    //         blockn = tempblock;
    //         block_read(4096+blockn,readbuf);   
    //     }
    //     *(readbuf+(index%4096)) = *((char *)buf + count);
    //     block_write(4096+blockn,readbuf);
    // }
    // free(readbuf);
    DIR[dirIndex].blocksNeeded=numBlockUsed;

    if(offset+nbyte>BLOCK_SIZE*BLOCK_SIZE&&offset<BLOCK_SIZE*BLOCK_SIZE)
    {
        DIR[dirIndex].size=BLOCK_SIZE*BLOCK_SIZE;
        fs_lseek(filde,BLOCK_SIZE*BLOCK_SIZE);
        return (nbyte+offset)-(BLOCK_SIZE*BLOCK_SIZE);
    }else if(offset+nbyte>BLOCK_SIZE*BLOCK_SIZE)
    {
        return 0;
    }
    if(offset+nbyte>DIR[dirIndex].size)
    {
        DIR[dirIndex].size=offset+nbyte;
    }
    fs_lseek(filde,offset+nbyte);
    return nbyte;
}

int fs_read(int filde, void *buf, size_t nbyte)
{
    if (fildes[filde].used==0)
    {
        return -1;
    }
    int bytesize = nbyte;
    int dirIndex = fildes[filde].file;
    int block = DIR[dirIndex].head; 
    int size = DIR[dirIndex].size;
    int offset = fildes[filde].offset;

    int numBlocksNeeded = ((size-1)/4096) + 1;
    int blocksInUse[numBlocksNeeded];
    int i;
    for(i =0;i<numBlocksNeeded;i++)
    {
        blocksInUse[i] = block;
        block = FAT[block];
    }
    char *bufTotal = malloc(numBlocksNeeded*BLOCK_SIZE);
    char* readbuf = malloc(BLOCK_SIZE);
    char * head = bufTotal;
    for(i=0;i<numBlocksNeeded;i++)
    {
        block_read(blocksInUse[i]+4096,readbuf);
        memcpy((void *)bufTotal,(void *)readbuf,BLOCK_SIZE);
        bufTotal += BLOCK_SIZE;
    }
    free(readbuf);
    head = head + offset;
    memcpy(buf,(void *)head,nbyte);
    if(bytesize + offset>size)
    {
        fs_lseek(filde,size);
        return 0;
    }
    fs_lseek(filde,bytesize + offset);
    return nbyte; 


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

int fs_listfiles(char ***files)
{
    int i;
    int count=0;
    for(i =0;i<MAX_FILES;i++)
    {
        if(DIR[i].used)
        {
            count++;
        }
    }
    *files = malloc((count + 1) * sizeof(char *));

    count=0;
    for(i =0;i<MAX_FILES;i++)
    {
        if(DIR[i].used)
        {
            (*files)[count] = DIR[i].name;
            count++;
        }
    }

    (*files)[count]=NULL;
    return 0;
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

int fs_truncate(int filde, off_t length)
{
    int dirIndex = fildes[filde].file;
    int sizeOfFile = DIR[dirIndex].size;
    if (length>sizeOfFile||filde<0||fildes[filde].used==0)
    {
        return -1;
    }
    char* buf = malloc(sizeof(DIR[dirIndex].size));
    int fp = fildes[filde].offset;
    fs_lseek(filde,0);
    fs_read(filde,buf,DIR[dirIndex].size);
    int refcount=DIR[dirIndex].ref_cnt;
    DIR[dirIndex].ref_cnt=0;
    fs_delete(DIR[dirIndex].name);
    fs_create(DIR[dirIndex].name);
    fs_write(filde,buf,length);
    if(fp<length)
    {
        fildes[filde].offset = fp;
    }
    DIR[dirIndex].ref_cnt=refcount;
    DIR[dirIndex].size=length;
    return 0;  
}

// int main()
// {
//     make_fs("idk"); 
//     mount_fs("idk");
//     fs_create("file1");
//     fs_create("file2");
//     fs_create("file3");
//     fs_create("file4");
//     char ***list = malloc(100000);
//     fs_listfiles(list);

// }

// int main()
// {
// // // // // //      make_fs("idk"); 
// // // // // //     mount_fs("idk");
// // // // // //     fs_create("file1");
// // // // // //     int* test = calloc(1,1000000);
// // // // // //     int fd = fs_open("file1");
// // // // // //     fs_write(fd,test,1000000);
// // // // // //     fs_write(fd,test,1000000);

// // // // // //     fs_lseek(fd,7999);
// // // // // //     char * buf = calloc(1,10000000);
// // // // // //     fs_read(fd,buf,10000000);

//     make_fs("idk"); 
//     mount_fs("idk");
//     fs_create("file1");
//      char* test = calloc(1,BLOCK_SIZE*(BLOCK_SIZE-1));
//     memset(test,'0',BLOCK_SIZE*(BLOCK_SIZE-1));
//     int fd = fs_open("file1");
//     fs_write(fd,test,BLOCK_SIZE*(BLOCK_SIZE-1));
//     memset(test,'1',BLOCK_SIZE*2);
//     fs_write(fd,test,BLOCK_SIZE*2);   
//     char * buf = calloc(1,BLOCK_SIZE*BLOCK_SIZE);
//     fs_lseek(fd,0);
//     int x = fs_read(fd,buf,fs_get_filesize(fd));
//     printf("%s", buf);

// // // // // //     fs_lseek(fd,5);
// // // // // //     fs_read(fd,buf,6);
    
    
// }