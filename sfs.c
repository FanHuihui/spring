#include <stdio.h>
#include <stdlib.h>
#include "disk_emu.h"
#include "sfs_api.h"
#include <string.h>

#define BLOCK_SIZE 10
#define BLOCK_NO 100
#define ROOT_DIRECOTRY_NO 2
#define SUPER_BLOCK_NO 2
#define FAT_BLOCK_NO 3
#define FREE_BLOCK_NO 1

typedef struct
{
	int ID;
	int size;
}Attr;

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

int *freeBlockTable;
int file_no;
int last_id_index;

int last_free_block_index;
int free_block_size;

void init(){
	file_no = 0;	//0 file at first
	last_id_index = 0;	//id index
	free_block_root_index = 0;

	free_block_size = BLOCK_NO - ROOT_DIRECOTRY_NO - SUPER_BLOCK_NO - FAT_BLOCK_NO;
	freeBlockTable = (int *)malloc(free_block_size * sizeof(int)); 
	
	directoryTable = NULL;
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
	
	if(!sfs_fcreate(name)){	//not create successfully
		return -1;
	};

	return returnIdex;
}

int sfs_fcreate(char *name){
	int free_block_index;
	if( (free_block_index = getLastFreeBlockIndex()) < 0){	//no space to create new file
		return 0;
	}
	//first, write into buffer
	//1. change file directory table
		//1a find free block
		//1b add new entry
	DirectoryEntry new_fentry;

	strncpy(new_fentry.name,name,sizeof(name));
	new_fentry.FAT_index = free_block_index;
	new_fentry.attr.id = last_id_index;
	new_fentry.attr.size = 0;

	int no_directory_entry = sizeof(directoryTable)/sizeof(DirectoryEntry);
	directoryTable = realloc(directoryTable,sizeof(DirectoryEntry));
	directoryTable[no_directory_entry] = new_fentry;

	last_id_index++;

	//2. change free table
	freeBlockTable[free_block_index] = 1;
	last_free_block_index = free_block_index;
	
	//3. change descriptor table
	DescriptorEntry new_dentry;
	new_dentry.root_FAT = new_fentry.FAT_index;
	new_dentry.wirte_ptr = 0;
	new_dentry.read_ptr = 0;
	
	int no_descriptor_entry = sizeof(descriptorTable)/sizeof(DescriptorEntry);
	descriptorTable = realloc(descriptorTable,sizeof(DescriptorEntry));
	descriptorTable[no_descriptor_entry] = new_dentry;

	//then push free block and directory table to harddisk
	write_blocks(BLOCK_NO-1, FREE_BLOCK_NO, freeBlockTable);
	write_blocks(SUPER_BLOCK_NO, ROOT_DIRECOTRY_NO, directoryTable);

	return 1;
}

int getLastFreeBlockIndex(){
	int i;
	for(i=0;i<free_block_size;i++){
		int index = (last_free_block_index + i) % free_block_size;
		if( freeBlockTable[index] == 0 ){
			return index;
		}
	}

	return -1;
}



