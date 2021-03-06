#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "stat.h"

static uint proc_dir_inum;


struct p_dirent* get_proc_dir_dirents(void);
void init_proc_dirents(struct proc* p);
void init_fdinfo_dirents(struct proc* p);
int icreate(struct proc* p);
void iremove(struct proc* p);

void
set_proc_dir_inum(int i)
{
  proc_dir_inum = i;
}

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);
// extern struct proc* lookup_proc_py_pid(int pid);
static void wakeup1(void *chan);


int
p_atoi(const char *s)
{
  int n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}

char*
p_strcpy(char *s, char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

int
p_strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

void
p_uitoa(char* dest, uint num)
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

#if 0
void copy_fdinfo_dirents(struct p_dirent *src,struct p_dirent *dst){
  int i;

  for(i=2 ; i<FDINFO_DIRENTS_SIZE ; i++){
    memmove(&src[i],&dst[i],sizeof(struct p_dirent));
  }
}
#endif

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  
  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");
  // p->proc_inode = icreate("proc/1" ,T_DEV, 2, 1, PROC, 1);

  p->state = RUNNABLE;
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  char path[] = "proc/0";

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));
 
  pid = np->pid;
  init_proc_dirents(np);
  init_fdinfo_dirents(np);
  path[5] += pid;
  np->proc_inum = icreate(np);
  //copy_fdinfo_dirents(proc->fdinfo_dirents,np->fdinfo_dirents);
  // lock to force the compiler to emit the np->state write last.
  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);
  
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;
  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  iremove(proc);

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        init_proc_dirents(p);
        init_fdinfo_dirents(p);
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;

  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&cpu->scheduler, proc->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot 
    // be run from main().
    first = 0;
    initlog();
  }
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}


struct proc*
lookup_proc_py_pid(int pid)
{
   struct proc *p;

   for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid)
      return p;
  }
  return 0;
}

int
lookup_empty_cell(struct p_dirent* dirents,int size){
  int i;
  for(i = 0; i < size; i++){
      if(dirents[i].inum == 0)
        break;
    }
    if(i == size) return -1;
    return i;
}




void
iremove(struct proc* p)
{
  int i, pid;
  struct p_dirent* proc_dir_dirents;
  char name[P_DIRSIZ];

  pid = (int)(p->pid);
  proc_dir_dirents = get_proc_dir_dirents();
  p_uitoa(name, pid);
  for(i = 0 ; i<DIRENTS_SIZE ;i++){
    if(p_strcmp((proc_dir_dirents[i].name), name) == 0)
      break;
    
  }
  memset(&(proc_dir_dirents[i]),0 ,  sizeof(struct p_dirent));
}

void
init_proc_dirents(struct proc* p)
{
  memset(&(p->proc_dirents),0 ,sizeof(p->proc_dirents)); 
}

void
init_fdinfo_dirents(struct proc* p)
{
  memset(&(p->fdinfo_dirents),0 ,sizeof(p->fdinfo_dirents)); 
}

int
icreate(struct proc* p)
{
  int i, pid, p_inum;
  char name[P_DIRSIZ];
  struct p_dirent* proc_dir_dirents;

  if( p == 0){
    p = lookup_proc_py_pid(1);
      init_proc_dirents(p);
      init_fdinfo_dirents(p);
  }

  pid = (int)(p->pid);
  proc_dir_dirents = get_proc_dir_dirents();
  p_uitoa(name, pid);

  if((i = lookup_empty_cell(proc_dir_dirents, DIRENTS_SIZE))<0)
    panic("no space in proc_dir_dirents");

  p_inum = 10000 + pid*100;

  proc_dir_dirents[i].inum = p_inum;
  p_strcpy((char*)(proc_dir_dirents[i].name),name);

  //linking . and ..
  p->proc_dirents[0].inum = p_inum;
  p_strcpy((char*)( p->proc_dirents[0].name),".");

  p->proc_dirents[1].inum = proc_dir_inum;
  p_strcpy((char*)( p->proc_dirents[1].name),"..");

  //add files
  p->proc_dirents[2].inum = (uint)(((struct p_inode*)p->cwd)->inum);
  p_strcpy((char*)( p->proc_dirents[2].name),"cwd");

  p->proc_dirents[3].inum = p_inum + 2;
  p_strcpy((char*)( p->proc_dirents[3].name),"fdinfo");

  p->proc_dirents[4].inum = p_inum + 3;
  p_strcpy((char*)( p->proc_dirents[4].name),"status");

  //fd_info
  p->fdinfo_dirents[0].inum = p_inum + 2;
  p_strcpy((char*)( p->fdinfo_dirents[0].name),".");

  p->fdinfo_dirents[1].inum = p_inum;
  p_strcpy((char*)( p->fdinfo_dirents[1].name),"..");

  for(i=0 ; i<NOFILE ; i++){

      memset(name,0,P_DIRSIZ);
      p_uitoa(name,i);
      p->fdinfo_dirents[i+2].inum = 20000 + pid*100 + (i+2);
      p_strcpy((char*)( p->fdinfo_dirents[i+2].name),name);
    
  }

  return p_inum;

}

void
add_file_to_fileinfo(struct proc* p,uint inum, int fd)
{
  // int i;
  char name[P_DIRSIZ];

  p_uitoa(name, fd);

  // if( (i = lookup_empty_cell(p->fdinfo_dirents,FDINFO_DIRENTS_SIZE))<0) panic("no emty cell in fd_info");

  p->fdinfo_dirents[fd +2].inum = inum;
  p_strcpy((char*)( p->fdinfo_dirents[fd +2].name),name);
}

void
remove_file_to_fileinfo(struct proc* p, int fd)
{
  // int i;
  // char name[P_DIRSIZ];
  // p_uitoa(name, fd);

  // for(i = 0; i < FDINFO_DIRENTS_SIZE; i++){
  //     if(p_strcmp( p->fdinfo_dirents[i].name, name) == 0)
  //       break;
  //   }

  memset(&(p->fdinfo_dirents[fd +2]),0 ,sizeof(struct p_dirent));
}

void
set_cwd(struct proc* p, uint inum)
{
   p->proc_dirents[2].inum = inum;
}





