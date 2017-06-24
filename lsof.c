#include "types.h"
#include "user.h"
#include "stat.h"
#include "param.h"
#include "fs.h"

#define FILENAME_SIZE 	100
#define PROC_PRFX 		5
#define FDINFO_LEN		7
#define DIRENTS_SIZE 	(NOFILE+2)
#define P_DIRSIZ 		14

struct p_dirent {
  ushort inum;
  char name[P_DIRSIZ];
};

#define DIRENTS_BUF_SIZE	(DIRENTS_SIZE * sizeof(struct p_dirent))
#define FILEINFO_BUF_SIZE 	1024
#define OUTPUT_BUF_SIZE 	1024

#//define inc_offset(a,b) a = a + pid>9? (b):(b+1);

static struct p_dirent *dirents;

void
uitoa(char* dest, uint num)
{
  int i,n,m,exp;
  n = 0;
  exp = 1;
  if (num == 0) {
    dest[0]='0';
    dest[1] = 0;
    return;
  }

  while ((num/exp) > 0){
    n++;
    exp *= 10;
  }

  m = num;
  
  for (i = 1; i < n + 1; i++){
    dest[n - i] = m % 10 + '0';
    m = m / 10;
  }

dest[n] = 0;
}

int
main(int argc, char *argv[]){
	int  /*proc_fd,*/info_fd,file_fd;
	int  pid,name_off,n,i;
	char file_name[FILENAME_SIZE] = {0};
	char pid_ascii[4] = {0};
	char fd_ascii[4] = {0};
	char dirents_buffer[DIRENTS_BUF_SIZE] = {0};
	char fileinfo_buffer[FILEINFO_BUF_SIZE] = {0};
	char *fdinfo_name = "/fdinfo";
	char *refs, *inum, *type, *pointer;


	printf(1,"start test!\n" );

	// if((proc_fd = open("proc",0)) < 0 ){//unused
	// 	printf(1,"ERROR: no file proc\n");
	// 	exit();
	// }
	strcpy(file_name,"proc/");

	printf(1," PID  FD   Refs  Inum   Type\n");
	printf(1,"---------------------------------\n");

	for(pid=0 ; pid<NPROC ; pid++){
		memset(pid_ascii,0,sizeof(pid_ascii));
		memset(file_name + PROC_PRFX,0,FILENAME_SIZE - PROC_PRFX);

		uitoa(pid_ascii,pid);
		strcpy(file_name + PROC_PRFX,pid_ascii);
		name_off = pid>9? 2:1;
		name_off += PROC_PRFX;
		strcpy(file_name + name_off,fdinfo_name);


		//printf(1,"Trying to open %s\n",file_name); //DEBUG
		if((info_fd = open(file_name,0)) > 2 ){
			//printf(1,"pid=%d file=%s exists\n",pid,file_name);//DEBUG

			n = read(info_fd,dirents_buffer,DIRENTS_BUF_SIZE);
			if(n>0){
				dirents = (struct p_dirent *)dirents_buffer;
				name_off += FDINFO_LEN;
				file_name[name_off] = '/';

				for(i=2 ; i<DIRENTS_SIZE ; i++){
					//printf(1,"Trying to open fd=%d in %s\n",i,file_name); //DEBUG
					if(dirents[i].inum != 0){
						//printf(1,"%s\n",dirents[i].name); //DEBUG

						memset(fd_ascii,0,sizeof(fd_ascii));
						uitoa(fd_ascii,i-2);

						memset(file_name + name_off + 1,0,FILENAME_SIZE - name_off - 1);
						strcpy(file_name + name_off + 1,dirents[i].name);

						if((file_fd = open(file_name,0)) < 0){
							printf(1,"ERROR: file %s doesn't exist\n",file_name);
						}else{
							memset(fileinfo_buffer,0,FILEINFO_BUF_SIZE);
							//printf(1,"before read %s\n",file_name); //DEBUG
							n = read(file_fd,fileinfo_buffer,FILEINFO_BUF_SIZE);
							//printf(1,"after read %s\n",file_name); //DEBUG
							if(n>0 && *fileinfo_buffer == 'n'){
								//printf(1,"local fd=%d\n",file_fd);//DEBUG
								//printf(1,"read %d bytes\n",n); //DEBUG
								//printf(1,"%s",fileinfo_buffer);//DEBUG
								pointer = strchr(fileinfo_buffer,'\n'); //skip name
								pointer = strchr(pointer,':');//goto type value
								type = pointer + 2;//get type value
								pointer = strchr(type,'\n');//goto end of line
								*pointer = 0;//delemit
								pointer++;
								pointer = strchr(pointer,'\n'); //skip position
								pointer++;
								pointer = strchr(pointer,'\n'); //skip flags
								pointer = strchr(pointer,':');//goto inum value
								inum = pointer + 2;//get inum value
								pointer = strchr(inum,'\n');//goto end of line
								*pointer = 0;//delemit
								pointer++;
								pointer = strchr(pointer,':');//goto refs value
								refs = pointer + 2;//get refs value
								pointer = strchr(refs,'\n');//goto end of line
								*pointer = 0;

								printf(1," %s    %s    %s     %s     %s\n",pid_ascii,fd_ascii,refs,inum,type);
							}
						}
						close(file_fd);

					}
				}
			}
		}
		close(info_fd);

	}

    	

	printf(1,"finish test!\n" );

	//bulshit against warnings

	exit();
	return 0;
}