typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;
enum inode_sub_type { NONE, PROC_DIR, PROC, BLOCK_STAT, INODE_STAT, CWD, FD_INFO, STATUS, FD}; 

struct p_block_stats{
  int   free_blocks_cache;
  int   used_blocks_cache;
  int   hits_in_cache;
  int   num_block_access;
};

struct p_inode_stats{
  int   free_inodes;
  int   valid_inodes;
  int   total_refs;
  int   total_used;
};