/**
 * @file bplustree.h
 * @brief 
 * 
 * @author gyz
 * @email mni_gyz@163.com
 */
#include <sys/types.h>
#include <stdlib.h>
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <queue>

#pragma pack(1)

#define MIN_CACHE_NUM 5
#define S_OK 1
#define S_FALSE -1
#define INVALID_OFFSET -1

static int blockSize_;
static int maxEntries_;
static int maxOrder_;

// TODO: 初始化中填充
static int LEAFSIZE_;
static int NOLEAFSIZE_;

enum
{
    BPLUS_TREE_LEAF = 0,
    BPLUS_TREE_NO_LEAF = 1,
};

typedef struct bPlusTreeConfig
{
    char fileName[64]; // 文件名称
    int blockSize;     // 块对齐
} config, *pConfig;

typedef struct bPlusNode
{
    ssize_t self;   // 自己在tree中的偏移量
    ssize_t parent; // 父母的节点的偏移
    ssize_t prev;   // 叶子节点的时候 指向上一个叶子节点
    ssize_t next;   // 叶子节点的时候 指向下一个叶子节点
    int type;       // 表明是叶子还是索引

    // 叶子节点的话就是当前元素数目
    // 索引节点的话就是孩子数量
    int count;
} node, *pNode;

struct listHead
{
    struct listHead *prev, *next;
};

struct freeBlock
{
    struct listHead link;
    ssize_t offset;
};

typedef struct bPlusTree
{
    char *caches;               // 缓冲区位置指针
    bool used[MIN_CACHE_NUM];   // 缓冲区使用情况
    char fileName[64];          // 文件名称
    int fd;                     // 文件描述符
    int level;                  // 一共有几层
    ssize_t root;               // 根节点偏移
    ssize_t fileSize;           // 文件大小
    struct listHead freeBlocks; // 空闲块链表
} tree, *pTree;

// 基本偏移操作函数
char *offsetPtr(pNode node);
char *sub(pNode node);
ssize_t *subi(pNode node, int i);
char *key(pNode node);
int *keyi(pNode node, int i);
char *data(pNode node);
ssize_t *datai(pNode node, int i);

// 函数声明
int setConfig(pConfig config);
int initTree(pTree tree, char *fileName, int blockSize);
int insert(pTree tree, int key, ssize_t data);
int leafInsert(pTree tree, pNode node, int key, ssize_t data);
int buildParentNode(pTree tree, pNode left, pNode right, int key);
int noLeafInsert(pTree tree, pNode node, pNode left, pNode right, int key);
int noLeafSplitRight(pTree tree, pNode node, pNode right, pNode lch, pNode rch, int key, int insert);
int noLeafSplitRight1(pTree tree, pNode node, pNode right, pNode lch, pNode rch, int key, int insert);
int subNodeFlush(pTree tree, pNode parent, ssize_t offset);
int noLeafSplitLeft(pTree tree, pNode node, pNode left, pNode lch, pNode rch, int key, int insert);
int noLeafSimpleInsert(pTree tree, pNode node, pNode left, pNode right, int key, int insert);
int subNodeUpdate(pTree tree, pNode parent, int insert, pNode child);
int leafSimpleInsert(pTree tree, pNode leaf, int key, ssize_t data, int insert);
int leafSplitLeft(pTree tree, pNode leaf, pNode left, int key, ssize_t data, int insert);
int leafSplitRight(pTree tree, pNode leaf, pNode right, int key, ssize_t data, int insert);
int leftNodeAdd(pTree tree, pNode node, pNode left);
int rightNodeAdd(pTree tree, pNode node, pNode right);
int isLeaf(pNode node);
int nodeFlush(pTree tree, pNode node);
int freeCache(pTree tree, pNode node);
ssize_t append2Tree(pTree tree, pNode node);
int listEmpty(struct listHead *head);
pNode getNode(pTree tree, ssize_t offset);
pNode newLeaf(pTree tree);
pNode newNoLeaf(pTree tree);
pNode newNode(pTree tree);
pNode getCache(pTree tree);
ssize_t search(pTree tree, int key);
int keyBinarySearch(pNode node, int target);

// 删除
int deleteNode(pTree tree, int key);
int nodeDelete(pTree tree, pNode node, pNode lch, pNode rch);
int leafSimpleRemove(pTree tree, pNode node, int pos);
int leafRemove(pTree tree, pNode node, int key);
noLeafBorrowRight(pTree tree, pNode left, pNode right);
noLeafBorrowLeft(pTree tree, pNode left, pNode right);

int noLeafRemove(pTree tree, pNode node, int key);
int noLeafSimpleRemove(pTree tree, pNode node, int pos);
int noLeafReplace(pTree tree, ssize_t node, int preK, int newK);

// 展示
void show(pTree tree);
void showNode(pNode node);