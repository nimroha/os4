struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  struct inode *ip;
  uint off;
};


// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  int flags;          // I_BUSY, I_VALID

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
  uint proc_pid;    // -1 for regular inode, 0 for /proc, PID for /proc/proc->pid/
  uint proc_fd;     // -1 for regular inode, FD for /proc/fdinfo/fd
  enum inode_sub_type sub_type;
};
#define I_BUSY 0x1
#define I_VALID 0x2

// table mapping major device number to
// device functions
struct devsw {
  int (*isdir)(struct inode*);
  void (*iread)(struct inode*, struct inode*);
  int (*read)(struct inode*, char*, int, int);
  int (*write)(struct inode*, char*, int);
};

extern struct devsw devsw[];

#define CONSOLE 1
#define PROCFS  2

#define IS_DEV_DIR(ip) (ip->type == T_DEV && devsw[ip->major].isdir && devsw[ip->major].isdir(ip))

//PAGEBREAK!
// Blank page.
