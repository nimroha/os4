#include "types.h"
#include "user.h"
#include "stat.h"



int main(int argc, char *argv[]){
	char buff[50]={0};
	int n,fd,block_fd;

	fd = open("ls",0);
	if(fd>0){
		n = read(fd,buff,10);
		if(n>0){
			memset(buff,0,50);
			if(n>0){
				block_fd = open("/proc/blockstat",0);

				n = read(block_fd,buff,50);

				if(n<=50){
					printf(1,"%s",buff);
				}
			}
		}
	}


	exit();
	return fd;
}