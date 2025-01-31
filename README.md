# Kama-memoryPool
【代码随想录知识星球】项目分享：Kama-memoryPool

>⭐️ 本项目为[【代码随想录知识星球】](https://programmercarl.com/other/kstar.html) 教学项目  
>⭐️ 在 内存池文档 里详细讲解：项目前置知识 + 项目细节 + 代码解读 + 项目难点 + 面试题与回答 + 简历写法 + 项目拓展。 全面帮助你用这个项目求职面试！
## 项目介绍
本项目是基于 C++ 实现的自定义内存池框架，旨在提高内存分配和释放的效率，特别是在多线程环境下。
该项目中实现的内存池主要分为两个版本，分别是目录 v1 和 v2 ( v3 在 v2 基础上进行了一定优化)，这两个版本的内存池设计思路大不相同。
### v1 介绍
基于哈希映射的多种定长内存分配器，可用于替换 new 和 delete 等内存申请释放的系统调用。包含以下主要功能：
- 内存分配：提供 allocate 方法，从内存池中分配内存块。
- 内存释放：提供 deallocate 方法，将内存块归还到内存池。
- 内存块管理：通过 allocateNewBlock 方法管理内存块的分配和释放。
- 自由链表：使用无锁的自由链表管理空闲内存块，提高并发性能。

### v2、v3 介绍
该项目包括以下主要功能：
- 线程本地缓存（ThreadCache）：每个线程维护自己的内存块链表，减少线程间的锁竞争，提高内存分配效率。
- 中心缓存（CentralCache）：用于管理多个线程共享的内存块，支持批量分配和回收，优化内存利用率。
- 页面缓存（PageCache）：负责从操作系统申请和释放大块内存，支持内存块的合并和分割，减少内存碎片。
- 自旋锁和原子操作：在多线程环境下使用自旋锁和原子操作，确保线程安全的同时减少锁的开销。

## 编译  
先进入 v1 或 v2 或 v3 项目目录
```
cd v1
```
在项目目录下创建build目录，并进入该目录
```
mkdir build
cd build
```
执行 cmake 命令
```
cmake ..
```
执行 make 命令
```
make
```  
删除编译生成的可执行文件：  
```
make clean
```  
## 运行
```
./可执行文件名
```  
