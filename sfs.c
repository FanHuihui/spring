#include <stdio.h>
#include <stdlib.h>
#include "disk_emu.h"
#include "sfs_api.h"
#include <string.h>
#include <math.h>

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
	int write_ptr;
	int read_ptr;
	int fileID;
} DescriptorEntry; 

typedef struct 
{
	int DB_index;
	int next;
}FATEntry;


DirectoryEntry *directoryTable;
DescriptorEntry *descriptorTable;
FATEntry *FATTable;

int *freeBlockTable;
int directory_no;
int open_directory_no;
int last_id_index;

int last_free_block_index;
int free_block_size;


int getLastFreeBlockIndex();
int sfs_fcreate(char *name);
int getWritePointer(int fileID);
int getRootFatIndex(int fileID);
int writeDataToDisk(int fileID,char *buf, int length);

void init(){
	directory_no = 0;	//0 file at first
	last_id_index = 0;	//id index
	open_directory_no = 0;	

	free_block_size = BLOCK_NO - ROOT_DIRECOTRY_NO - SUPER_BLOCK_NO - FAT_BLOCK_NO;
	freeBlockTable = (int *)malloc(free_block_size * sizeof(int)); 
	//first several block should be non-empty
	//????
	//????

	directoryTable = NULL;
	descriptorTable = NULL;
	FATTable = NULL;
}

int mksfs(int fresh){

	if(fresh){
		init();
		init_fresh_disk("v_disk.txt", BLOCK_SIZE, BLOCK_NO);
	}else{
		init_disk("v_disk.txt",BLOCK_SIZE ,BLOCK_NO);
	}
    
    return 1;
}


void sfs_ls(){
	int i;
	for(i=0;i<directory_no;i++){
		DirectoryEntry entry = directoryTable[i];
		printf("\n- File name is %s -\n", entry.name);
		printf("- Size is %d -\n", entry.attr.size);
		printf("- FAT index is %d\n", entry.FAT_index);
		printf("\n");
	}
}

int sfs_fopen(char *name){
	int i,j;

	for(i = 0;i<directory_no;i++){
		DirectoryEntry entry = directoryTable[i];
		printf("entry name: %s\n", entry.name);

		if(strcmp(entry.name,name) == 0){
			printf("Same name found\n");
			int FAT_index = entry.FAT_index;
			for(j = 0;j < open_directory_no; j++){
				DescriptorEntry descriptor = descriptorTable[j];
				if(descriptor.root_FAT == FAT_index ){
					return j;
				}
			}
		}
	}

	//if no such file
	printf("%s not exist\n",name);
	int returnIdex = last_id_index;
	
	if(!sfs_fcreate(name)){	//not create successfully
		return 0;
	};

	return returnIdex;
}

int sfs_fwrite(int fileID, char *buf, int length){
	int i,j;
	
	//find the write pointer
	int w_pointer;
	if(w_pointer = getWritePointer(fileID)){
		if(w_pointer%BLOCK_SIZE != 0){//need to append bytes to last block
			int nth_block = w_pointer/BLOCK_SIZE;
			int nextIndex = getRootFatIndex(fileID);

			for(i=0;i<nth_block;i++){
				nextIndex = directoryTable[nextIndex].next;
			}
			
			int DB_index = directoryTable[nextIndex].DB_index;
			char *buffer = (char *)malloc(BLOCK_SIZE);
			read_blocks(DB_index,1, buffer);

			int left_bytes = BLOCK_SIZE - w_pointer%BLOCK_SIZE;
			if(left_bytes >= length){ //no need to write to a new block
				for(j = 0;j < length; j++){
					buffer[w_pointer%BLOCK_SIZE] = buf[j];
					w_pointer++;
				}

				//write to hard-disk
				write_blocks(DB_index,1, buffer);
			}else{
				for(j=0;j<left_bytes;j++){//write all bytes into current block
					buffer[w_pointer%BLOCK_SIZE] = buf[j];
					w_pointer++;
				}
				write_blocks(DB_index,1, buffer);//write block to disk

				char *newBuf = (char *)malloc(length - left_bytes);
				// for(j=0;j<length - left_bytes;j++){
				// 	newBuf[j] = buf[j+left_bytes];
				// }
				memcpy(newBuf, &buf[left_bytes], (length - left_bytes) * sizeof(char)); 

				writeDataToDisk(fileID,newBuf,length - left_bytes);
			}
		}else{
			writeDataToDisk(fileID,buf,length);
		}
	}else{
		return 0;
	};
	
	return 1;
}

int writeDataToDisk(int fileID,char *buf, int length){
	int no_block = ceil((double)length/BLOCK_SIZE);

	int *array = (int *)malloc(sizeof(int) * no_block);

	int i,j;

	//get space first 
	for(i=0;i<no_block;i++){
		array[i] = getLastFreeBlockIndex();
		if(array[i] == -1){
			for(j=0;j<i;j++){
				freeBlockTable[array[j]] = 0;
			}

			printf("fail to allocate db block, no space\n");
			return 0;
		}

		freeBlockTable[array[i]] = 1;
	}
	
	//find root fat index first
	int rootFatIndex = getRootFatIndex(fileID);
	if(FATTable[rootFatIndex].DB_index == -1){
		
	}

	return 1;
}

int getRootFatIndex(int fileID){
	int i;
	for(i=0;i<directory_no;i++){
		DirectoryEntry entry = directoryTable[i];
		if(entry.attr.ID == fileID){
			return entry.FAT_index;
		}
	}

	return -1;
}

int getWritePointer(int fileID){
	int i;

	for(i=0;i<open_directory_no;i++){
		DescriptorEntry entry = descriptorTable[i];
		if(entry.fileID == fileID){
			return entry.write_ptr;
		}
	}

	printf("cannot find write pointer\n");
	return 0;
}

int sfs_fread(int fileID, char *buf, int length){
	int i,j;

	

	return 1;
}

int sfs_fcreate(char *name){
	//printf("going to create!");

	int free_block_index;
	if( (free_block_index = getLastFreeBlockIndex()) < 0){	//no space to create new file
		return 0;
	}
	//first, write into buffer

	//0. add fat node in fat table
		//0a. create a FAT NODE
	FATEntry fat_entry;
	fat_entry.DB_index = -1;		//no block is allocated at first
	fat_entry.next = -1;
	
	int top_fat_index = sizeof(FATTable)/sizeof(FATEntry);
	FATTable = realloc(FATTable,sizeof(FATEntry));
	FATTable[top_fat_index] = fat_entry;

	//1. change file directory table
		//1a find free block
		//1b add new entry
	DirectoryEntry new_fentry;

	strncpy(new_fentry.name,name,strlen(name)+1); //plus 1 to include \0
	new_fentry.FAT_index = fat_entry.DB_index;
	new_fentry.attr.ID = last_id_index;
	new_fentry.attr.size = 0;

	int top_directory_index = directory_no;
	directoryTable = realloc(directoryTable,sizeof(DirectoryEntry));
	directoryTable[top_directory_index] = new_fentry;

	last_id_index++;

	// //2. change free table
	// freeBlockTable[free_block_index] = 1;
	// last_free_block_index = free_block_index;
	
	//3. change descriptor table
	DescriptorEntry new_dentry;
	new_dentry.root_FAT = new_fentry.FAT_index;
	new_dentry.write_ptr = 0;
	new_dentry.read_ptr = 0;
	new_dentry.fileID = new_fentry.attr.ID;

	int top_descriptor_index = open_directory_no;
	descriptorTable = realloc(descriptorTable,sizeof(DescriptorEntry));
	descriptorTable[top_descriptor_index] = new_dentry;

	// then push free block and directory table to harddisk & fat table
	// write_blocks(BLOCK_NO-1, FREE_BLOCK_NO, freeBlockTable);
	// write_blocks(SUPER_BLOCK_NO, ROOT_DIRECOTRY_NO, directoryTable);
	// write_blocks(SUPER_BLOCK_NO+ROOT_DIRECOTRY_NO, FAT_BLOCK_NO, FATTable);

	//increase file no
	directory_no++;
	open_directory_no++;

	//write info to hard disk
	write_blocks(SUPER_BLOCK_NO, ROOT_DIRECOTRY_NO, directoryTable);
	write_blocks(SUPER_BLOCK_NO+ROOT_DIRECOTRY_NO, FAT_BLOCK_NO, FATTable);

	return 1;
}

int sfs_fclose(int fileID){
	//flush memory to harddisk
	write_blocks(BLOCK_NO-1, FREE_BLOCK_NO, freeBlockTable);
	write_blocks(SUPER_BLOCK_NO, ROOT_DIRECOTRY_NO, directoryTable);
	write_blocks(SUPER_BLOCK_NO+ROOT_DIRECOTRY_NO, FAT_BLOCK_NO, FATTable);

	open_directory_no--;
	
	return 1;
}

int sfs_remove(char *file){

	//remove the file from the directory entry 
	int i,j;
	char *buffer = (char *)malloc(BLOCK_SIZE);
	for (i = 0; i < BLOCK_SIZE; i++)
	{
		buffer[i]=48;
	}

	
	for(i = 0;i<directory_no;i++){
		DirectoryEntry entry = directoryTable[i];
		printf("entry name: %s\n", entry.name);

		if(strcmp(entry.name,file) == 0){
			printf("Same name found\n");
			// To release the file allocation table entries, but dont know how to link to free slot
			FATEntry *current= &FATTable[entry.FAT_index];
			FATEntry *previous= NULL;
			for(j=0;current->next!= -1;j++){
				freeBlockTable[current->DB_index]=0; // Mark the data block as free
				write_blocks(current->DB_index,1, buffer);
				previous=current;
				current=&FATTable[current->next];
				previous->next=-1;
			}		

			//release the current directory entry
			for (j = i; j < directory_no-i-1; j++){
				directoryTable[j]=directoryTable[j+1];	
			}
			directory_no--;
			DirectoryEntry *dirTempraryTable= (DirectoryEntry *)malloc(directory_no * sizeof(DirectoryEntry));
			memcpy(dirTempraryTable, &directoryTable[0], directory_no * sizeof(DirectoryEntry)); 
			directoryTable=dirTempraryTable;

			//release the descritory entry
			for (j = i; j <  open_directory_no-i-1; j++){
				descriptorTable[j]=descriptorTable[j+1];
			}
			open_directory_no--;
			DescriptorEntry *desTempraryTable=(DescriptorEntry *)malloc(open_directory_no * sizeof(DescriptorEntry));
			memcpy(desTempraryTable, &descriptorTable[0], open_directory_no * sizeof(DescriptorEntry)); 
			descriptorTable=desTempraryTable;
		}

		break;
	}	


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



