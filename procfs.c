#include "types.h"
#include "stat.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#define DIRENTS_SIZE NPROC + 4

struct proc* lookup_proc_py_pid(int pid);


struct dirent proc_dir_dirents[DIRENTS_SIZE];
  
int
lookup_empty_cell(){
  int i;
  for(i = 0; i < DIRENTS_SIZE; i++){
      if(proc_dir_dirents[i].inum == 0)
        break;
    }
    if(i == DIRENTS_SIZE) return -1;
    return i;
}

int 
procfsisdir(struct inode *ip) {
  int ans = (ip->sub_type == PROC_DIR || ip->sub_type == PROC || ip->minor == 1);
  // cprintf("procfsisdir ans:%d\n",ans);
  return ans;
}

void 
procfsiread(struct inode* dp, struct inode *ip) {
	cprintf("--procfsiread--\n");
}

int
procfsread(struct inode *ip, char *dst, int off, int n) {
  // cprintf("--procfsread--\n");
  if(ip->sub_type == PROC_DIR){
    if(off + n > sizeof(proc_dir_dirents)) return 0; 
    cprintf("offset: %d\n", off/sizeof(struct dirent));
    memmove(dst, proc_dir_dirents + off, n);
    return n;
  }
  return 0;
}

int
procfswrite(struct inode *ip, char *buf, int n){
  // cprintf("--procfswrite--\n");
  int i;
  if(ip->sub_type == PROC_DIR){
    if((i=lookup_empty_cell())< 0) return 0;
    memmove(&proc_dir_dirents[i], buf, n);
    return n;
  }
  return 0;
}

void
procfsinit(void)
{
  memset(&proc_dir_dirents, 0 ,sizeof(proc_dir_dirents));
  devsw[PROCFS].isdir = procfsisdir;
  devsw[PROCFS].iread = procfsiread;
  devsw[PROCFS].write = procfswrite;
  devsw[PROCFS].read = procfsread;
}
