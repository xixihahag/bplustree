/**
 * @file bplustree.h
 * @brief 
 * 
 * @author gyz
 * @email mni_gyz@163.com
 */
#include <sys/types.h>

#define MIN_CACHE_NUM 5
#define S_OK 1
#define S_FALSE -1
#define INVALID_OFFSET -1
#define offsetPtr(node) ((char *)(node) + sizeof(*node))
#define key(node) ((int *)offsetPtr(node))
#define data(node) ((ssize_t *)(offsetPtr(node) + maxEntries_ * sizeof(int)))
// 这里减1的原因是索引节点偏移指针比存的key多一个
#define sub(node) ((ssize_t *)(offsetPtr(node) + (maxOrder_ - 1) * sizeof(ssize_t)))

static int blockSize_;
static int maxEntries_;
static int maxOrder_;

enum
{
    BPLUS_TREE_LEAF,
    BPLUS_TREE_NO_LEAF = 1,
};

typedef struct bPlusTreeConfig
{
    char fileName[1024]; // 文件名称
    int blockSize;       // 块对齐
} config, *pConfig;

typedef struct bPlusNode
{
    ssize_t self;   // 自己在tree中的偏移量
    ssize_t parent; //父母的偏移量
    ssize_t prev;   //叶子节点的时候 指向上一个叶子节点
    ssize_t next;   // 叶子节点的时候 指向下一个叶子节点
    int type;       //表明是叶子还是索引

    //叶子节点的话就是当前元素数目
    //索引节点的话就是孩子数量
    int count;
} node, *pNode;

typedef struct listHead
{
    struct listHead *prev, *next;
} listHead;

typedef struct freeBlock
{
    struct listHead link;
    ssize_t offset;
} freeBlock;

typedef struct bPlusTree
{
    char *caches;               // 缓冲区位置指针
    int used[MIN_CACHE_NUM];    // 缓冲区使用情况
    char fileName[1024];        // 文件名称
    int fd;                     // 文件描述符
    int level;                  // 一共有几层
    ssize_t root;               // 根节点便宜
    ssize_t fileSize;           // 文件大小
    struct listHead freeBlocks; // 空闲块链表

} tree, *pTree;