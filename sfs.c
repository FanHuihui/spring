#include <stdio.h>
#include <stdlib.h>
#include "disk_emu.h"
#include "sfs_api.h"
#include <string.h>
#include <math.h>

#define BLOCK_SIZE 1000
#define BLOCK_NO 1000
#define ROOT_DIRECOTRY_NO 2
#define SUPER_BLOCK_NO 1
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

typedef struct 
{
	int no_root_block;
	int no_fat_block;
	int no_free_block;
	int no_supper_block;
	int block_no;
}SuperBlockData;


DirectoryEntry *directoryTable;
DescriptorEntry *descriptorTable;
FATEntry *FATTable;

int *freeBlockTable;
int directory_no;
int open_directory_no;
int fat_node_no;
int last_id_index;

int last_free_block_index;
int free_block_size;


int getLastFreeBlockIndex();
int sfs_fcreate(char *name);
int getWritePointer(int fileID);
int getRootFatIndex(int fileID);
int writeDataToDisk(int fileID,char *buf, int length);
int changeDirectoryRootFatIndex(int fileID, int rootIndex);
int changeOpenFileRootFatIndex(int fileID, int rootIndex);
int getReadPointer(int fileID);
void removeEntryFromDescriptorTable(int fileID);
void increaseWritePointer(int fileID,int length);
void increaseReadPointer(int fileID,int length);
int getFileSize(int fileID);
int getNthBlockDBIndex(int fileID, int nth_fat_block);
int createNewFatEntry(int DB_index,int next);
FATEntry* getLastFatEntry(int fileID);

void init(){
	directory_no = 0;	//0 file at first
	last_id_index = 0;	//id index
	open_directory_no = 0;	

	free_block_size = BLOCK_NO;
	freeBlockTable = (int *)malloc(free_block_size * sizeof(int)); 
	///first several block should be non-empty
	int i;

	for(i=0;i<BLOCK_NO;i++){
		freeBlockTable[i] = 0;
	}
	for(i=0;i<SUPER_BLOCK_NO+FAT_BLOCK_NO+ROOT_DIRECOTRY_NO;i++){
		freeBlockTable[i] = 1;
	}
	freeBlockTable[BLOCK_NO-1] = 1;

	directoryTable = NULL;
	descriptorTable = NULL;
	FATTable = NULL;
	fat_node_no = 0;

	SuperBlockData data;
	data.no_fat_block = FAT_BLOCK_NO;
	data.no_supper_block = SUPER_BLOCK_NO;
	data.no_free_block = FREE_BLOCK_NO;
	data.no_root_block = ROOT_DIRECOTRY_NO;
	data.block_no = BLOCK_NO;

	char *buf = (char *)malloc(BLOCK_SIZE);
	//memcpy(buf,&data,sizeof(data));

	write_blocks(0,1,buf);
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

	printf("open name is %s \n", name);
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

			//same name not found in open file, may be closed already
			//append the new entry into last row

			DescriptorEntry *tmpTable = (DescriptorEntry *)malloc(sizeof(open_directory_no+1));
			memcpy(tmpTable,descriptorTable,open_directory_no * sizeof(DescriptorEntry));

			DescriptorEntry e;
			e.root_FAT = entry.FAT_index;
			int last_ptr = getFileSize(open_directory_no);
			e.write_ptr = last_ptr;
			e.read_ptr = last_ptr;

			tmpTable[open_directory_no] = e;
			descriptorTable = tmpTable; 
			
			open_directory_no++;
			

			return open_directory_no-1;
		}
	}

	//if no such file
	
	if(!sfs_fcreate(name)){	//not create successfully
		return 0;
	};

	return open_directory_no-1;
}



int sfs_fwrite(int fileID, char *buf, int length){
	int i,j;
	
	int w_pointer = getWritePointer(fileID);
	int no_block_to_write = ceil((double)length/BLOCK_SIZE);
	int offset = w_pointer%BLOCK_SIZE;
	int block_base = 0;

	printf("31");
	fflush(stdout);
	char *w_buffer = (char *)malloc(no_block_to_write * BLOCK_SIZE); 

	if(offset != 0 || w_pointer < getFileSize(fileID)){
		int db_id = getNthBlockDBIndex(fileID,w_pointer/BLOCK_SIZE);
		char *r_buf = (char *)malloc(BLOCK_SIZE);
		read_blocks(db_id,1,r_buf);

		memcpy(w_buffer,r_buf,offset);
	}

	printf("32");
	fflush(stdout);
	memcpy(w_buffer+offset,buf,length);

	int *db_id_array = (int *)malloc(sizeof(int) * no_block_to_write);
	for(i=0;i<no_block_to_write;i++){
		db_id_array[i] = -1;
	}

	printf("33");
	fflush(stdout);
	int off=0;
	if(offset != 0 || w_pointer < getFileSize(fileID)){//need to record first block
		printf("34");
		fflush(stdout);		
		int db_id = getNthBlockDBIndex(fileID,w_pointer/BLOCK_SIZE);
		db_id_array[0] = db_id;
		off = 1;

		if(w_pointer < getFileSize(fileID)){//trace back then find the following blocks 
			printf("found trace back\n");
			int nth = w_pointer/BLOCK_SIZE+1;
			
			while(1){
				int r = getNthBlockDBIndex(fileID,nth);
				if(r != -1 && off < no_block_to_write){
					db_id_array[off] = r;
					off++;
				}else{
					break;
				}
			}
		}

		if(off < no_block_to_write){//still need to find free block
			int i;
			for(i = 0; i < no_block_to_write-off; i++){
				int f_id = getLastFreeBlockIndex();
				if(f_id < 0){
					printf("cannot find free block already\n");
					break;
				}
				freeBlockTable[f_id] = 1;
				db_id_array[off] = f_id;
				off++;
			}
		}
	}else{
			printf("35");
			fflush(stdout);		
		int i;
			for(i = 0; i < no_block_to_write-off; i++){
				int f_id = getLastFreeBlockIndex();
				if(f_id < 0){
					printf("cannot find free block already\n");
					break;
				}
				freeBlockTable[f_id] = 1;
				db_id_array[off] = f_id;
				off++;
			}
	}
	
	printf("36");
	fflush(stdout);
	//write to hard disk
	int fail=0;
	int count=0;
	for(i = 0; i < no_block_to_write; i++){
		if(db_id_array[i] != -1){
			count++;		
		}else{
			fail = 1;		
		}
	}
	for(i = 0; i < count; i++){
		
		int size = (i == count - 1)?sizeof(buf)%BLOCK_SIZE:BLOCK_SIZE;
		char *tmp_buf = (char *)malloc(BLOCK_SIZE);
		memcpy(tmp_buf, buf+i*BLOCK_SIZE,BLOCK_SIZE);

		write_blocks(db_id_array[i],1,tmp_buf);
	}
	
	//link fat node
	//with head

	printf("37");
	fflush(stdout);
	int root_id = getRootFatIndex(fileID);
	FATEntry *f_entry = &FATTable[root_id];
	if(f_entry -> DB_index != -1){
		printf("38");
		fflush(stdout);		
		int isOverride=0;
		while(f_entry -> next != -1){
			if(f_entry->DB_index == db_id_array[0]){
				isOverride = 1;
				break;
			}
		}

		if(isOverride){
			for(i = 1; i < no_block_to_write; i++){
				if( f_entry -> next != -1){
					f_entry = &FATTable[f_entry->next];
				}else{//new fat node
					if(db_id_array[i] == -1) break;

					int fat_index = createNewFatEntry(db_id_array[i],-1);
					f_entry -> next = fat_index;
					f_entry = &FATTable[fat_index];
				}
			}
		}else{//append new fat nodes
			FATEntry *entry = getLastFatEntry(fileID);
			for(i=0;i<no_block_to_write;i++){
				if(db_id_array[i] == -1) break;

				int fat_index = createNewFatEntry(db_id_array[i],-1);
				entry -> next = fat_index;
				entry = &FATTable[fat_index];
			}
		}
	}else{
		printf("39");
		fflush(stdout);		
	
		//update the root for directory table and descriptor table
		FATEntry *entry = getLastFatEntry(fileID);
		entry -> DB_index = db_id_array[0];

		for(i=1;i<no_block_to_write;i++){
			if(db_id_array[i] == -1) break;
			printf("39.1");
			fflush(stdout);
			int fat_index = createNewFatEntry(db_id_array[i],-1);
			printf("39.2");
			fflush(stdout);
			entry -> next = fat_index;
			entry = &FATTable[fat_index];
		}
	}

	if(fail){
		int no_block = 0;
		for(i=0;i<no_block_to_write;i++){
			if(db_id_array[i] == -1){
				break;				
			}		
			no_block++;
		}
		increaseWritePointer(fileID,no_block * BLOCK_SIZE);
		return -1;
	}

	printf("40");
	fflush(stdout);
	increaseWritePointer(fileID,length);
	return length;
}

int sfs_fread(int fileID, char *buf, int length){
	int i,j;

	int read_ptr = getReadPointer(fileID);
	int nth_fat_block = read_ptr/BLOCK_SIZE;

	int offset = read_ptr % BLOCK_SIZE;

	int no_block_to_read = ceil((double)length/BLOCK_SIZE);

	char *r_buf = (char *)malloc(no_block_to_read * BLOCK_SIZE);
	for(i = 0;i < no_block_to_read; i++){
		int db_index = getNthBlockDBIndex(fileID, nth_fat_block);
		nth_fat_block++;
		char *tmp_buf = (char *)malloc(BLOCK_SIZE);
		read_blocks(db_index,1,tmp_buf);
		memcpy(r_buf + i*BLOCK_SIZE,tmp_buf,BLOCK_SIZE);
	}

	memcpy(buf,r_buf+offset,length);

	increaseReadPointer(fileID,length);

	return 1;
}

void increaseWritePointer(int fileID,int length){

	printf("increase fileID %d w ptr wi length %d\n",fileID,length);
	int i,wp;
	
	wp = descriptorTable[fileID].write_ptr;
	wp+=length;
	printf("write pointer is %d \n" ,wp);
	for(i = 0; i < directory_no;i++){
		if(directoryTable[i].FAT_index == descriptorTable[fileID].root_FAT && wp > directoryTable[i].attr.size){
			directoryTable[i].attr.size = wp;
		}
	}
}

void increaseReadPointer(int fileID,int length){
	int i;
	descriptorTable[fileID].read_ptr += length;
}


int sfs_fseek(int fileID, int offset){
	int i;
	
	descriptorTable[fileID].read_ptr += offset;
	descriptorTable[fileID].write_ptr += offset;
	
	return 1;
}

int sfs_fclose(int fileID){
	int i,j;
	int index;
	//remove entry from open file directory
	removeEntryFromDescriptorTable(fileID);

	//flush memory to harddisk
	write_blocks(BLOCK_NO-1, FREE_BLOCK_NO, freeBlockTable);
	write_blocks(SUPER_BLOCK_NO, ROOT_DIRECOTRY_NO, directoryTable);
	write_blocks(SUPER_BLOCK_NO+ROOT_DIRECOTRY_NO, FAT_BLOCK_NO, FATTable);
	
	return 1;
}

int sfs_remove(char *file){

	//remove the file from the directory entry 
	int i,j;
	char *buffer = (char *)malloc(BLOCK_SIZE);
	for (i = 0; i < BLOCK_SIZE; i++)
	{
		buffer[i] = 0;
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


	return 0;

}


int sfs_fcreate(char *name){
	printf("going to create!");
	fflush(stdout);
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
	
	printf("11\n");
	fflush(stdout);
	int top_fat_index = fat_node_no;

	printf("fattable size is %d\n",sizeof(FATTable));

	FATEntry *ftTable = (FATEntry *)malloc(sizeof(FATEntry) * (top_fat_index+1));
	memcpy(ftTable,FATTable,sizeof(FATEntry) * (top_fat_index));
	ftTable[top_fat_index] = fat_entry;
	free(FATTable);
	FATTable = ftTable;
	fat_node_no++;

	printf("12\n");
	fflush(stdout);
	//1. change file directory table
		//1a find free block
		//1b add new entry
	DirectoryEntry new_fentry;

	strncpy(new_fentry.name,name,strlen(name)+1); //plus 1 to include \0
	new_fentry.FAT_index = top_fat_index;			//index of fat table
	new_fentry.attr.ID = last_id_index;
	new_fentry.attr.size = 0;

	int top_directory_index = directory_no;
	
	DirectoryEntry *direTable = (DirectoryEntry *)malloc(sizeof(DirectoryEntry) * (directory_no+1));
	memcpy(direTable,directoryTable,sizeof(DirectoryEntry) * (directory_no));
	direTable[top_directory_index] = new_fentry;
	free(directoryTable);
	directoryTable = direTable;

	last_id_index++;
	
	printf("13\n");
	fflush(stdout);
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
	
	DescriptorEntry *tmpTable = (DescriptorEntry *)malloc(sizeof(DescriptorEntry) * (open_directory_no+1));
	memcpy(tmpTable, descriptorTable, sizeof(DescriptorEntry) * (open_directory_no));
	tmpTable[top_descriptor_index] = new_dentry;
	free(descriptorTable);
	descriptorTable = tmpTable;

	printf("14\n");
	fflush(stdout);
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
	
	printf("15\n");
	fflush(stdout);
	return 1;
}


int getLastFreeBlockIndex(){
	int i;
	for(i=0;i<BLOCK_NO;i++){
		if( freeBlockTable[i] == 0 ){
			//sleep(1);
			return i;
		}
	}

	printf("\ncannot find a free block\n");

	return -1;
}

int getFileSize(int fileID){
	int i;
	for(i=0;i<directory_no;i++){
		DirectoryEntry entry = directoryTable[i];
		if(entry.FAT_index == descriptorTable[fileID].root_FAT){
			return entry.attr.size; 
		}
	}
	return -1;
}

int getNthBlockFatIndex(int fileID, int nth_block){
	int rootIndex = getRootFatIndex(fileID);
	int i;

	FATEntry entry = FATTable[rootIndex];
	for( i = 0; i < nth_block; i++){
		if(entry.next < 0){
			return -1;
		}
		rootIndex = entry.next;
		entry = FATTable[entry.next];
	}

	return rootIndex;

}

int getNthBlockDBIndex(int fileID, int nth_fat_block){
	int i;

	int rootIndex = getRootFatIndex(fileID);

	FATEntry entry = FATTable[rootIndex];
	for( i = 0; i < nth_fat_block; i++){
		if(entry.next < 0){
			return -1;
		}
		entry = FATTable[entry.next];
	}

	return entry.DB_index;
}

int getWritePointer(int fileID){
	int i;

	return descriptorTable[fileID].write_ptr;

	printf("cannot find write pointer\n");
	return -1;
}

int getReadPointer(int fileID){
	int i;

	return descriptorTable[fileID].read_ptr;

	printf("cannot find read pointer\n");
	return -1;
}

int changeDirectoryRootFatIndex(int fileID, int rootIndex){
	int i;
	for(i=0;i<directory_no;i++){
		if(directoryTable[i].FAT_index == descriptorTable[fileID].root_FAT){
			directoryTable[i].FAT_index = rootIndex;
		}
	}

	return 1;
}

int changeOpenFileRootFatIndex(int fileID, int rootIndex){
	int i;
	for(i=0;i<open_directory_no;i++){
		if(descriptorTable[i].fileID == fileID){
			descriptorTable[i].root_FAT = rootIndex;
		}
	}

	return 1;
}

int getRootFatIndex(int fileID){
	int i;
	
	return descriptorTable[fileID].root_FAT;

	return -1;
}

int fatIndexWithDBIndex(int db_id){
	int ite = fat_node_no;
	int i;	
	for(i = 0; i < ite; i++){
		if(FATTable[i].DB_index == db_id){
			return i;
		}
	}

	return -1;
}

FATEntry* getLastFatEntry(int fileID){
	int root = getRootFatIndex(fileID);

	FATEntry *entry = &FATTable[root];

	while(entry->next != -1){
		entry = &FATTable[entry->next];
	}

	return entry;
}

int createNewFatEntry(int DB_index,int next){
	int no_fat = fat_node_no;
	int i;
	for(i=0;i<no_fat;i++){
		FATEntry *entry = &FATTable[i];
		if(freeBlockTable[entry->DB_index] == 0 ){
			entry -> DB_index = DB_index;
			entry -> next = next;
			return i;
		} 
	}

	FATEntry fat_entry;
	fat_entry.DB_index = DB_index;		//no block is allocated at first
	fat_entry.next = next;
	
	int top_fat_index = fat_node_no;

	FATEntry *tmpTable = (FATEntry *)malloc(sizeof(FATEntry) * (top_fat_index + 1));
	memcpy(tmpTable,FATTable,sizeof(FATEntry)*fat_node_no);	
	tmpTable[top_fat_index] = fat_entry;
	FATTable = tmpTable;	

	fat_node_no++;

	return top_fat_index;
}

void removeEntryFromDescriptorTable(int fileID){
	int index = -1;
	int i,j;

	if(index != -1){
		DescriptorEntry *tmpTable = (DescriptorEntry *)malloc(sizeof(DescriptorEntry) * open_directory_no-1);
		memcpy(tmpTable,descriptorTable,fileID * sizeof(DescriptorEntry));
		memcpy(tmpTable + fileID * sizeof(DescriptorEntry), descriptorTable + (fileID+1) * sizeof(DescriptorEntry), (open_directory_no- fileID -1)*sizeof(DescriptorEntry));
		descriptorTable = tmpTable;
		open_directory_no--;
	}
}

