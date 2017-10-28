#ifndef _SCULL_H
#define _SCULL_H

#define SCULL_MAJOR 0
#define SCULL_BUFFER_SIZE PAGE_SIZE

#define SCULLC_QUANTUM  4000 /* use a quantum size like scull */
#define SCULLC_QSET     500

struct scull_qset {
	void **data;
	struct scull_qset *next; 
}; 

struct scull_dev { 
	 struct scull_qset *data;  /* 指向第一个量子集的指针 */ 
	 int quantum;  		       /* 当前量子的大小 */ 
	 int qset; 		  		   /* 当前数组的大小 */ 
	 unsigned long size;       /* 保存在其中的数组总量 */ 
	 unsigned int access_key;  /* 由sculluid 和 scullpriv使用 */ 
	 struct mutex mutex;     /* Mutual exclusion */     
	 struct cdev cdev;         /* 字符设备结构 */
};

#endif