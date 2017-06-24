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

#define MAX_STAT_BUF_SIZE   2048
#define MAX_FDINFO_BUF_SIZE 1024
#define MAX_PROC_STATUS_BUF_SIZE 600

struct proc* lookup_proc_py_pid(int pid);
int proc_dir_lookup_empty_cell(void);
void inode_stats_to_buf(struct p_inode_stats*);
void block_stats_to_buf(struct p_block_stats*);
void fd_info_to_buf(int fd,struct file *f);
void status_to_buf(struct proc *);


static struct p_dirent proc_dir_dirents[DIRENTS_SIZE];
static struct p_block_stats block_stats = {0};
static struct p_inode_stats inode_stats = {0};
static char   inode_stats_buff[MAX_STAT_BUF_SIZE];
static char   block_stats_buff[MAX_STAT_BUF_SIZE];
static char   fd_info_buff[MAX_FDINFO_BUF_SIZE];
static char   status_buff[MAX_PROC_STATUS_BUF_SIZE];
static int    inode_buf_length;
static int    block_buf_length;
static int    fd_info_buf_length;
static int    status_buf_length;

int
cat_fdinfo_dir(char *dst, int off, int n, struct proc *p){
  int i,m=0,k=sizeof(p->fdinfo_dirents[0]);

  //cprintf("I:   m=%d n=%d k=%d\n",m,n,k); //DEBUG
  if(off + n > sizeof(p->fdinfo_dirents)){
    n = sizeof(p->fdinfo_dirents) - off;
    if(n <= 0){
      return 0;
    }
  }
  //cprintf("II:  m=%d n=%d k=%d\n",m,n,k); //DEBUG

  for(i=0 ; i<FDINFO_DIRENTS_SIZE ; i++){
    if(p->fdinfo_dirents[i].inum != 0){
      if(m + k > n){
        k = n-m;
      }
      //cprintf("III: m=%d n=%d k=%d\n",m,n,k); //DEBUG
      memmove(dst + m, (char*)(p->fdinfo_dirents) + off + m, k);
      m += k;
    }
  }
  return m;
}

int
cat_proc_status(char *dst, int off, int n, struct proc *p){

  status_to_buf(p);

  if(off+n > status_buf_length){
    n = status_buf_length - off;
    if(n > 0){
      memmove(dst + off,status_buff + off,n);
    }else{
      n = 0;
    }
  }else{
    memmove(dst + off,status_buff + off,n);  
  }

  return n;
}

int
cat_fdinfo_file(char *dst, int off, int n, int fd, struct file *f){

  //cprintf("cat_fdinfo_file: fetching data\n");//DEBUG
  fd_info_to_buf(fd,f);

  if(off+n > fd_info_buf_length){
    n = fd_info_buf_length - off;
    if(n > 0){
      memmove(dst + off,fd_info_buff + off,n);
    }else{
      n = 0;
    }
  }else{
    memmove(dst + off,fd_info_buff + off,n);  
  }

  return n;
}

int
cat_inode_stats(char *dst, int off, int n){

  if(off == 0){
    //cprintf("cat_inode_stats: fetching data\n");//DEBUG
    get_inode_stats(&inode_stats);
    inode_stats_to_buf(&inode_stats);
  }

  if(off+n > inode_buf_length){
    n = inode_buf_length - off;
    if(n > 0){
      memmove(dst + off,inode_stats_buff + off,n);
    }else{
      n = 0;
    }
  }else{
    memmove(dst + off,inode_stats_buff + off,n);  
  }

  return n;
}

int
cat_block_stats(char *dst, int off, int n){

  if(off == 0){
    // cprintf("cat_block_stats: fetching data\n");//DEBUG
    get_block_stats(&block_stats);
    block_stats_to_buf(&block_stats);
  }

  if(off+n > block_buf_length){
    n = block_buf_length - off;
    if(n > 0){
      memmove(dst + off,block_stats_buff + off,n);
    }else{
      n = 0;
    }
  }else{
    memmove(dst + off,block_stats_buff + off,n);  
  }

  return n;
}

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
  char size[]="Mem size: ";
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
  // cprintf("iread name: %s\n", name);//DEBUG
  if(strcmp(name,"cwd") == 0){
    ip->sub_type = CWD;
  }else if(strcmp(name,"fdinfo") == 0){
    // cprintf("iread name: in %s\n", name);//DEBUG
    ip->sub_type = FD_INFO;
  }else if(strcmp(name,"status") == 0){
    ip->sub_type = STATUS;
  }else if(strcmp(name,"..") == 0){
    ip->proc_pid = 0;
    ip->sub_type = PROC_DIR;
    ip->size = (DIRENTS_SIZE)*sizeof(struct dirent);
  }
}

//--------------------------------------------------------------------------
//------------------------fdinfo functions----------------------------------


void
fdinfo_set_inode_by_name(struct inode* ip, char* name, int pid)
{

  int fd;
  // cprintf("iread-fdinfo name:  %s\n", name);//DEBUG
  if(strcmp(name,"..") == 0){
    ip->proc_pid = pid;
    ip->sub_type = PROC;
    ip->size = 5*sizeof(struct dirent);
  }else{
    fd = atoi(name);
    ip->proc_fd = fd;
    ip->sub_type = FD;
    ip->size = MAX_FDINFO_BUF_SIZE;
  }
}




int 
fdinfo_lookup_empty_cell(int pid)
{
  int i;
  struct proc* p;

  p = lookup_proc_py_pid(pid);

  for(i = 0; i < FDINFO_DIRENTS_SIZE; i++){
      if(p->fdinfo_dirents[i].inum == 0)
        break;
    }
    if(i == FDINFO_DIRENTS_SIZE) return -1;
    return i;
}

int 
fdinfo_lookup_cell_by_inum(int pid, uint inum)
{
  int i;
  struct proc* p;

  p = lookup_proc_py_pid(pid);

  for(i = 0; i < FDINFO_DIRENTS_SIZE; i++){
    if(p->fdinfo_dirents[i].inum == inum)
      break;
  }
  if(i == FDINFO_DIRENTS_SIZE) return -1;
  return i;
}


//--------------------------------------------------------------------------

int 
procfsisdir(struct inode *ip) {
  return (ip->sub_type == PROC_DIR || ip->sub_type == PROC || ip->sub_type == FD_INFO);
}

void 
procfsiread(struct inode* dp, struct inode *ip) {
  int i;
  struct proc *p;
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
      proc_set_inode_by_name(dp->proc_pid, ip, p->proc_dirents[i].name);//sets ip's proc_pid ans sub_type
      break;

    case FD_INFO:
      // cprintf("--procfsiread FD_INFO--\n");//DEBUG
      p =  lookup_proc_py_pid(dp->proc_pid);
      if((i = fdinfo_lookup_cell_by_inum(dp->proc_pid,ip->inum)) < 0) panic("iread: dirent not in fdinfo");
      fdinfo_set_inode_by_name(ip, p->fdinfo_dirents[i].name, dp->proc_pid);
      ip->proc_pid = dp->proc_pid;
      break;

    default:
      panic("procfsiread: unknown sub_type");
      break;
  }

}

int
procfsread(struct inode *ip, char *dst, int off, int n) {
  struct proc* p;
  int fd;
  struct file *f;

  switch(ip->sub_type){

    case PROC_DIR:
      //cprintf("procfsread: PROC_DIR - off=%d size=%d\n",off,n);//DEBUG
      if(off + n > sizeof(proc_dir_dirents)){
        n = sizeof(proc_dir_dirents) - off;
        if(n <= 0){
          return 0;
        }
      } 
      memmove(dst, (char*)proc_dir_dirents + off, n);
      return n;
      break;

    case PROC:
      //cprintf("procfsread: PROC - off=%d size=%d\n",off,n);//DEBUG
      p = lookup_proc_py_pid(ip->proc_pid);
      if(off + n > sizeof(p->proc_dirents)){
        n = sizeof(p->proc_dirents) - off;
        if(n <= 0){
          return 0;
        }
      } 
      memmove(dst, (char*)(p->proc_dirents) + off, n);//TODO deal with this like in fdinfo_dirents?
      return n;
      break;

    case STATUS:
      if( (off + n) > MAX_PROC_STATUS_BUF_SIZE){
        n = MAX_PROC_STATUS_BUF_SIZE - off;
        if(n <= 0){
          return 0;
        }
      }
      p = lookup_proc_py_pid(ip->proc_pid);
      return cat_proc_status(dst,off,n,p);
      break;

    case FD_INFO:
      //cprintf("procfsread: FD_INFO - off=%d size=%d max_size=%d\n",off,n,sizeof(p->fdinfo_dirents));//DEBUG
      p = lookup_proc_py_pid(ip->proc_pid);
      return cat_fdinfo_dir(dst,off,n,p);
      break;

    case FD:
      //cprintf("procfsread: FD - off=%d size=%d\n",off,n);//DEBUG
      p = lookup_proc_py_pid(ip->proc_pid);
      fd = ip->proc_fd;
      f = p->ofile[fd];
      if( (off + n) > MAX_FDINFO_BUF_SIZE){
        n = MAX_FDINFO_BUF_SIZE - off;
        if(n <= 0){
          return 0;
        }
      }
      return cat_fdinfo_file(dst,off,n,fd,f);
      break;

    case BLOCK_STAT:
      //cprintf("procfsread: BLOCK_STAT - off=%d size=%d\n",off,n);//DEBUG
      if( (off + n) > MAX_STAT_BUF_SIZE){
        n = MAX_STAT_BUF_SIZE - off;
        if(n <= 0){
          return 0;
        }
      }
      return cat_block_stats(dst,off,n);
      break;

    case INODE_STAT:
      //cprintf("procfsread: INODE_STAT - off=%d size=%d\n",off,n);//DEBUG
      if( (off + n) > MAX_STAT_BUF_SIZE){
        n = MAX_STAT_BUF_SIZE - off;
        if(n <= 0){
          return 0;
        }
      }
      return cat_inode_stats(dst,off,n);
      break;

    default:
      panic("procfsread: unknown inode sub_type");
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

void status_to_buf(struct proc *p){
  char status[25] = {0};
  char size[25] = {0};

  memset(status_buff,0,MAX_PROC_STATUS_BUF_SIZE);

  status_to_str(status, p->state);
  size_to_str(size, p->sz);
  strcpy(status_buff,status);
  strcpy(status_buff + strlen(status),size);

  status_buf_length = strlen(status_buff);
}


void inode_stats_to_buf(struct p_inode_stats* stats){
  int off=0;
  char *free_s="free inodes: ";
  char *valid_s="valid inodes: ";
  char *ratio_s="refs per inode: ";
  char *nline="\n";
  char *div=" / ";
  char free_n[100] = {0};
  char valid_n[100] = {0};
  char refs_n[100] = {0};
  char used_n[100] = {0};


  memset(inode_stats_buff,0,MAX_STAT_BUF_SIZE);

  //free
  strcpy(inode_stats_buff + off,free_s);
  off += strlen(free_s);
  uitoa(free_n,stats->free_inodes);
  strcpy(inode_stats_buff + off,free_n);
  off += strlen(free_n);
  strcpy(inode_stats_buff + off,nline);
  off += strlen(nline);

  //valid
  strcpy(inode_stats_buff + off,valid_s);
  off += strlen(valid_s);
  uitoa(valid_n,stats->free_inodes);
  strcpy(inode_stats_buff + off,valid_n);
  off += strlen(valid_n);
  strcpy(inode_stats_buff + off,nline);
  off += strlen(nline);

  //ratio
  strcpy(inode_stats_buff + off,ratio_s);
  off += strlen(ratio_s);
  uitoa(refs_n,stats->total_refs);
  strcpy(inode_stats_buff + off,refs_n);
  off += strlen(refs_n);
  strcpy(inode_stats_buff + off,div);
  off += strlen(div);
  uitoa(used_n,stats->total_used);
  strcpy(inode_stats_buff + off,used_n);
  off += strlen(used_n);
  strcpy(inode_stats_buff + off,nline);
  off += strlen(nline);

  inode_stats_buff[off] = 0;

  inode_buf_length = off;

}

void block_stats_to_buf(struct p_block_stats* stats){
  int off=0;
  char *free_s="free blocks: ";
  char *used_s="used blocks: ";
  char *ratio_s="hits ratio: ";
  char *nline="\n";
  char *div=" / ";
  char free_n[100] = {0};
  char used_n[100] = {0};
  char hits_n[100] = {0};
  char access_n[100] = {0};


  memset(block_stats_buff,0,MAX_STAT_BUF_SIZE);

  //free
  strcpy(block_stats_buff + off,free_s);
  off += strlen(free_s);
  uitoa(free_n,stats->free_blocks_cache);
  strcpy(block_stats_buff + off,free_n);
  off += strlen(free_n);
  strcpy(block_stats_buff + off,nline);
  off += strlen(nline);

  //used
  strcpy(block_stats_buff + off,used_s);
  off += strlen(used_s);
  uitoa(used_n,stats->used_blocks_cache);
  strcpy(block_stats_buff + off,used_n);
  off += strlen(used_n);
  strcpy(block_stats_buff + off,nline);
  off += strlen(nline);

  //ratio
  //cprintf("blockstat: hits=%d access=%d\n",stats->hits_in_cache,stats->num_block_access); //DEBUG
  strcpy(block_stats_buff + off,ratio_s);
  off += strlen(ratio_s);
  uitoa(hits_n,stats->hits_in_cache);
  //cprintf("hits %s length=%d\n",hits_n, strlen(hits_n)); //DEBUG
  strcpy(block_stats_buff + off,hits_n);
  off += strlen(hits_n);
  strcpy(block_stats_buff + off,div);
  off += strlen(div);
  uitoa(access_n,stats->num_block_access);
  //cprintf("access %s\n",access_n); //DEBUG
  strcpy(block_stats_buff + off,access_n);
  off += strlen(access_n);
  strcpy(block_stats_buff + off,nline);
  off += strlen(nline);

  block_stats_buff[off] = 0;

  block_buf_length = off;

}

void fd_info_to_buf(int fd,struct file *f){
  int off = 0;
  char *name_s="name: ";
  char *type_s="type: ";
  char *pos_s="position: ";
  char *flags_s="flags: ";
  char *inum_s="inode number: ";
  char *ref_s="refs: ";
  char *read="READ ";
  char *write="WRITE";
  char *nline="\n";
  char *flag;
  char *types[3] = { "FD_NONE", "FD_PIPE", "FD_INODE" };
  char name_n[100] = {0};
  char pos_n[100] = {0};
  char inum_n[100] = {0};
  char ref_n[100] = {0};


  memset(fd_info_buff,0,MAX_FDINFO_BUF_SIZE);

  //name
  strcpy(fd_info_buff + off,name_s);
  off += strlen(name_s);
  uitoa(name_n,fd);
  strcpy(fd_info_buff + off,name_n);
  off += strlen(name_n);
  strcpy(fd_info_buff + off,nline);
  off += strlen(nline);

  //type
  strcpy(fd_info_buff + off,type_s);
  off += strlen(type_s);
  strcpy(fd_info_buff + off,types[f->type]);
  off += strlen(types[f->type]);
  strcpy(fd_info_buff + off,nline);
  off += strlen(nline);

  //pos
  strcpy(fd_info_buff + off,pos_s);
  off += strlen(pos_s);
  uitoa(pos_n,f->off);
  strcpy(fd_info_buff + off,pos_n);
  off += strlen(pos_n);
  strcpy(fd_info_buff + off,nline);
  off += strlen(nline);

  //flags
  strcpy(fd_info_buff + off,flags_s);
  off += strlen(flags_s);
  flag = f->readable? read : "";
  strcpy(fd_info_buff + off,flag);
  off += strlen(flag);
  flag = f->writable? write : "";
  strcpy(fd_info_buff + off,flag);
  off += strlen(flag);
  strcpy(fd_info_buff + off,nline);
  off += strlen(nline);

  //inum
  strcpy(fd_info_buff + off,inum_s);
  off += strlen(inum_s);
  uitoa(inum_n,f->ip->inum);
  strcpy(fd_info_buff + off,inum_n);
  off += strlen(inum_n);
  strcpy(fd_info_buff + off,nline);
  off += strlen(nline);

  //refs
  strcpy(fd_info_buff + off,ref_s);
  off += strlen(ref_s);
  uitoa(ref_n,f->ref);
  strcpy(fd_info_buff + off,ref_n);
  off += strlen(ref_n);
  strcpy(fd_info_buff + off,nline);
  off += strlen(nline);


  fd_info_buff[off] = 0;

  fd_info_buf_length = off;

}