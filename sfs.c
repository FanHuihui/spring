#include <stdio.h>
#include <stdlib.h>
#include "disk_emu.h"
#include "sfs_api.h"
#include <string.h>

#define BLOCK_SIZE 10
#define BLOCK_NO 10

typedef struct
{
	int ID;
	int size;
}Attr;

typedef struct 
{
	int content;
	int next;
}FreeBlockEntry;

typedef struct
{
    char name[20];
    Attr attr;
    int FAT_index;
} DirectoryEntry;

typedef struct
{
	int root_FAT;
	int wirte_ptr;
	int read_ptr;
} DescriptorEntry; 

DirectoryEntry *directoryTable;
DescriptorEntry *descriptorTable;
FreeBlockEntry *freeBlockTable;
int file_no;
int last_id_index;

void init(){
	file_no = 0;	//0 file at first
	last_id_index = 0;	//id index

	directoryTable = NULL;
	freeBlockTable = NULL;
	descriptorTable = NULL;
}

int mksfs(int fresh){

	if(fresh){
		init();
		init_fresh_disk("v_disk.txt", BLOCK_SIZE, BLOCK_NO);
	}else{
		init_disk("v_disk.txt",BLOCK_SIZE ,BLOCK_NO);
	}
    
    return 0;
}


void sfs_ls(){
	int i;
	for(i=0;i<file_no;i++){
		DirectoryEntry entry = directoryTable[i];
		printf("- File name is %20s -", entry.name);
		printf("- Size is %10d -", entry.attr.size);
		printf("- FAT index is %10d", entry.FAT_index);
		printf("\n");
	}
}

int sfs_fopen(char *name){
	int i,j;

	for(i = 0;i<file_no;i++){
		DirectoryEntry entry = directoryTable[i];
		if(strcmp(entry.name,name) == 0){
			int FAT_index = entry.FAT_index;
			for(j = 0;j < file_no; j++){
				DescriptorEntry descriptor = descriptorTable[j];
				if(descriptor.root_FAT == FAT_index ){
					return j;
				}
			}
		}
	}

	//if no such file
	int returnIdex = last_id_index;
	//sfs_fwrite(last_id_index, NULL,0);
	sfs_fcreate(name);

	return returnIdex;
}

int sfs_fcreate(char *name){
	//first, write into buffer
	//1. change file directory table
	//2. change free table
	//3. change descriptor table

	//then push to harddisk

	return 0;
}



