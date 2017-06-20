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
#define BUFF_SIZE 512

struct proc* lookup_proc_py_pid(int pid);
int proc_dir_lookup_empty_cell(void);

static struct p_dirent proc_dir_dirents[DIRENTS_SIZE];

struct p_dirent*
get_proc_dir_dirents(void)
{
  return proc_dir_dirents;
}

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

char*
strcpy(char *s, char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

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
}

//--------------------------------------------------------------------------
//------------------------proc_dir functions----------------------------
void
proc_dir_add_files()
{
  proc_dir_dirents[2].inum = 10000 + 2;
  strcpy((char*)( proc_dir_dirents[2].name),"blockstat");

  proc_dir_dirents[3].inum = 10000 + 3;
  strcpy((char*)( proc_dir_dirents[3].name),"inodestat");
}

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

  if(strcmp(name,"blockstat") == 0){
    ip->proc_pid = 0;
    ip->sub_type = BLOCK_STAT;
    ip->size = sizeof(struct dirent);
  }else if(strcmp(name,"inodestat") == 0){
    ip->proc_pid = 0;
    ip->sub_type = INODE_STAT;
    ip->size = sizeof(struct dirent);
  }else {
    pid = atoi(name);
    ip->proc_pid = pid;
    ip->sub_type = PROC;
    ip->size = 5*sizeof(struct dirent);
  }
}
//--------------------------------------------------------------------------
//------------------------proc functions--------------------------------
void
status_to_str(char* dest, enum procstate state)
{
  char *states[] = {
  [UNUSED]    "Status: UNUSED,   ",
  [EMBRYO]    "Status: EMBRYO,   ",
  [SLEEPING]  "Status: SLEEPING,   ",
  [RUNNABLE]  "Status: RUNNABLE,   ",
  [RUNNING]   "Status: RUNNING,   ",
  [ZOMBIE]    "Status: ZOMBIE,   "
  };
  strcpy(dest, states[state] );
}

void
size_to_str(char* dest, uint sz)
{ 
  int off;
  char size[]="Size: ";
  off = strlen(size);
  strcpy(dest, size);
  uitoa((char*)(dest + off),  sz);
  off = strlen(dest);
  dest[off-1] = 10;
  dest[off] = 0;
}

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
  // struct proc* p;
  // p = lookup_proc_py_pid(pid);
  ip->proc_pid = pid;
  ip->size = sizeof(struct dirent);
  // cprintf("proc-> name: %s\n", name);//DEBUG
  if(strcmp(name,"cwd") == 0){
    ip->sub_type = CWD;
  }else if(strcmp(name,"fdinfo") == 0){
    ip->sub_type = FD_INFO;
  }else if(strcmp(name,"status") == 0){
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
  struct proc* p;
  // cprintf("--procfsiread--\n");//DEBUG

  ip->major = 2;
  ip->type = T_DEV;
  ip->flags = I_VALID;

  switch(dp->sub_type){
    case PROC_DIR:
      // cprintf("procfsiread PROC_DIR\n");//DEBUG
      if((i = proc_dir_lookup_cell_by_inum(ip->inum)) < 0) panic("iread: dirent not in proc_dir");
      proc_dir_set_inode_by_name(ip, proc_dir_dirents[i].name);//sets ip's proc_pid ans sub_type
      break;

    case PROC:
      // cprintf("procfsiread PROC\n");//DEBUG
      p =  lookup_proc_py_pid(dp->proc_pid);
      if((i = proc_lookup_cell_by_inum(dp->proc_pid, ip->inum)) < 0) panic("iread: dirent not in proc_dir");
      proc_set_inode_by_name(dp->proc_pid, ip,p->proc_dirents[i].name);//sets ip's proc_pid ans sub_type
      break;

    default:
      break;
  }

}

int
procfsread(struct inode *ip, char *dst, int off, int n) {
  struct proc* p;
  char status[25];
  char size[25];
  int len , offset;

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

    case STATUS:
      if( (off + n) > BUFF_SIZE) return 0; 
      p =  lookup_proc_py_pid(ip->proc_pid);

      status_to_str(status, p->state);
      len = strlen(status);
      memmove(dst,status,len);

      offset = len;

      size_to_str(size, p->sz);
      len = strlen(size);
      memmove(dst + offset ,size,len);
      return offset + len;
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
  proc_dir_add_files();
  devsw[PROCFS].isdir = procfsisdir;
  devsw[PROCFS].iread = procfsiread;
  devsw[PROCFS].write = procfswrite;
  devsw[PROCFS].read = procfsread;
}
