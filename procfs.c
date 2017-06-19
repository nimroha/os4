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

struct dirent proc_dir_dirents[DIRENTS_SIZE];

struct proc* lookup_proc_py_pid(int pid);

//--------------------------str functions---------------------------------
int
atoi(const char *s)
{
  int n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}

int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

//--------------------------------------------------------------------------
//------------------------proc_dir functions----------------------------
int
proc_dir_lookup_empty_cell(){
  int i;
  for(i = 0; i < DIRENTS_SIZE; i++){
      if(proc_dir_dirents[i].inum == 0)
        break;
    }
    if(i == DIRENTS_SIZE) return -1;
    return i;
}

int
proc_dir_lookup_cell_by_inum(uint inum){
  int i;
  for(i = 0; i < DIRENTS_SIZE; i++){
      if(proc_dir_dirents[i].inum == inum)
        break;
    }
    if(i == DIRENTS_SIZE) return -1;
    return i;
}

void
proc_dir_set_inode_by_name(struct inode* ip, char* name)
{
  int pid;
  if(strcmp(name,"blockstat")){
    ip->proc_pid = 0;
    ip->sub_type = BLOCK_STAT;
  }else if(strcmp(name,"inodestat")){
    ip->proc_pid = 0;
    ip->sub_type = INODE_STAT;
  }else {
    pid = atoi(name);
    ip->proc_pid = pid;
    ip->sub_type = PROC;
  }
}
//--------------------------------------------------------------------------
//------------------------proc functions--------------------------------

int 
proc_lookup_empty_cell(int pid)
{
  int i;
  struct proc* p;

  p = lookup_proc_py_pid(pid);

  for(i = 0; i < PROC_DIRENTS_SIZE; i++){
      if(p->proc_dirents[i].inum == 0)
        break;
    }
    if(i == PROC_DIRENTS_SIZE) return -1;
    return i;
}

int 
proc_lookup_cell_by_inum(int pid, uint inum)
{
  int i;
  struct proc* p;

  p = lookup_proc_py_pid(pid);
  for(i = 0; i < PROC_DIRENTS_SIZE; i++){
    if(p->proc_dirents[i].inum == inum)
      break;
  }
  if(i == PROC_DIRENTS_SIZE) return -1;
  return i;
}

void 
proc_set_inode_by_name(int pid, struct inode* ip, char* name)
{
  ip->proc_pid = pid;

  if(strcmp(name,"cwd")){
    ip->sub_type = CWD;
  }else if(strcmp(name,"fdinfo")){
    ip->sub_type = FD_INFO;
  }else if(strcmp(name,"status")){
    ip->sub_type = STATUS;
  }
}
//--------------------------------------------------------------------------

int 
procfsisdir(struct inode *ip) {
  return (ip->sub_type == PROC_DIR || ip->sub_type == PROC || ip->minor == 1);
}

void 
procfsiread(struct inode* dp, struct inode *ip) {
  int i;
  char *name;
  cprintf("--procfsiread--\n");

  switch(dp->sub_type){
    case PROC_DIR:
      if((i = proc_dir_lookup_cell_by_inum(ip->inum)) < 0) panic("iread: dirent not in proc_dir");
      name = proc_dir_dirents[i].name;
      proc_dir_set_inode_by_name(ip, name);//sets ip's proc_pid ans sub_type
      break;

    case PROC:
      if((i = proc_lookup_cell_by_inum(dp->proc_pid, ip->inum)) < 0) panic("iread: dirent not in proc_dir");
      name = proc_dir_dirents[i].name;
      proc_set_inode_by_name(dp->proc_pid, ip, name);//sets ip's proc_pid ans sub_type
      break;

    default:
      break;
  }
  ip->major = 2;
  ip->type = T_DEV;
  ip->flags &= I_VALID;
}

int
procfsread(struct inode *ip, char *dst, int off, int n) {
  struct proc* p;

  switch(ip->sub_type){

    case PROC_DIR:
      if(off + n > sizeof(proc_dir_dirents)) return 0; 
      memmove(dst, (char*)proc_dir_dirents + off, n);
      return n;
      break;

    case PROC:
      p =  lookup_proc_py_pid(ip->proc_pid);
      if(off + n > sizeof(p->proc_dirents)) return 0; 
      memmove(dst, (char*)(p->proc_dirents) + off, n);
      return n;
      break;

    default:
      return 0;
  }  
}

int
procfswrite(struct inode *ip, char *buf, int n){
  // cprintf("--procfswrite--\n");
  int i;
  struct proc* p;
  
  switch(ip->sub_type){

    case PROC_DIR:
      if((i=proc_dir_lookup_empty_cell())< 0) return 0;
      memmove(&proc_dir_dirents[i], buf, n);
      return n;

    case PROC:
      p =  lookup_proc_py_pid(ip->proc_pid);
      if((i=proc_lookup_empty_cell(ip->proc_pid))< 0) return 0;
      memmove(&(p->proc_dirents[i]), buf, n);
      return n;
    default:
      return 0;
  }
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
