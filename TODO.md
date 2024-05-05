## codec放到Lua
1. 读写事件从ev回调到Socket.cpp，在Socket调用对应packet的unpack
2. unpack成功，现在根据Socket类型派发到不同函数。直接激发到lua，由Lua处理
3. 不同的Socket，header的定义也不同，在Lua怎么处理(只有Lua才知道当前这个Socket的header类型，因此需要回调到Lua后，Lua再调用额外的函数从header解析出额外的数据)
	http、websocket等直接是原始数据，不需要解析
	c2s需要解析出来一个协议号，再用protobuf解码
	s2s则要区分数据转发，rpc调用、s2sHeader等，需要从header解析出额外的数据

socket::append函数会唤醒io线程，但有时候会同一逻辑中多次append，这个优化一下
ssl的发送机制有问题，未握手完成时，一直在send队列，假如对方迟迟未发送握手数据，这时候会处理死循环地直尝试写数据

fd、io对象是在主线程创建，io线程读写。主线程关闭socket时，io线程正常读写怎么办？（EV_CLOSE或者加锁？epoll_wait时，socket也不能关闭）
创建socket后，可能由于脚本报错而无法设置io对象，需要处理

* 实现场景的static区域属性(不可修改)和dynamic区域属性(可根据逻辑创建和修改)
* 技能的配置比较复杂，看看能不能设计出多action，多result，多条件控制又比较容易明白的excel配置
* 主循环是否使用分时器

* profile看效率问题
* lint优化代码
* mongodb的result感觉要用个object_pool
* stream_packet.cc里和ws_stream_packet.cc里的sc_command和cs_command可以合并复用
* 考虑用C++编译Lua:https://www.liangzl.com/get-article-detail-140988.html
* lrank中的排行榜把多因子独立出去形成一个类，因为大部分都是单因子排序
* lmongo里有很多接口参数是旧的，需要更新为最新版本
* 由于使用了自动生成协议编码机制，原来的协议相关需要优化下，比如 截取字符串取package名

#优化定时器（加到wiki）
* app定时器，只保留5秒，并且修正为整分钟触发(用real time修正)
* 定时器可配置是否处理time jump
* app一次性delay定时器，
* 玩家一次性delay定时器，
* 副本一次性delay定时器，副本延迟、销毁退出用
* 绝对定时器(absolute timer,指定年月日时分秒运行)
* 相对定时器(relative timer,指定周几，几日，时分秒，按月，按周循环)
* 用小堆实现系统长时定时器，把上面的一些定时器管理起来

## win下epoll接口封装： https://github.com/piscisaureus/wepoll/blob/dist/wepoll.h

#清理test和doc目录中无关的内容，迁移到wiki
#位置同步
http://blog.codingnow.com/2006/04/sync.html
http://blog.codingnow.com/2012/03/dev_note_12.html

#AOI模块及其算法
http://docs2x.smartfoxserver.com/AdvancedTopics/advanced-mmo-api
http://blog.codingnow.com/2012/03/dev_note_13.html
http://www.cnblogs.com/sniperHW/archive/2012/09/29/2707953.html
http://www.codedump.info/?p=388
https://github.com/xiarendeniao/pomelo-aoi

#Pomelo协议设计参考
https://github.com/NetEase/pomelo/wiki/Pomelo-%E5%8D%8F%E8%AE%AE

# 模块框架不是很好（参考ping）
* 总得声明一个singleton？
* gateway、world不同进程运行的逻辑不一样，用if判断？
* rpc的声明、回调只能用local函数？
* 客户端回调也只能是local函数？
* rpc、cmd注册放在文件尾？不是每个模块都有一个xxx_cmd文件的


#状态监控
* C++内存池统计
* C++主要对象统计
* 脚本在C++中创建的对象统计
* 各协议接收、发送次数、频率统计（包括rpc调用）、slow protocol、db存库、查库频率，各查询平均时间
* socket流量统计
* 各协议处理耗时统计
* sethook各协议调用函数统计(debug用)
* sethook限制、统计堆栈调用深度，防止死循环(debug用)
* 统计各副本、各场景在线人数
* 统计routine主循环中各实体的耗时
* 像实体出现这种频率高又包含子message的，如何优化?(这个是发出去的，可以用固定table)

尝试用upb替换pbc:https://github.com/google/upb
lua_stream中解析proto文件，用proto文件来做schema文件

mem statistics
http://man7.org/linux/man-pages/man3/mallinfo.3.html

https://sourceware.org/glibc/wiki/MallocInternals
https://sploitfun.wordpress.com/2015/02/10/understanding-glibc-malloc/

https://fossies.org/linux/glibc/malloc/malloc.c
https://code.woboq.org/userspace/glibc/malloc/malloc.c.html#int_mallinfo

```c++
/*
   ------------------------------ mallinfo ------------------------------
   Accumulate malloc statistics for arena AV into M.
 */
static void
int_mallinfo (mstate av, struct mallinfo *m)
{
  size_t i;
  mbinptr b;
  mchunkptr p;
  INTERNAL_SIZE_T avail;
  INTERNAL_SIZE_T fastavail;
  int nblocks;
  int nfastblocks;
  check_malloc_state (av);
  /* Account for top */
  avail = chunksize (av->top);
  nblocks = 1;  /* top always exists */
  /* traverse fastbins */
  nfastblocks = 0;
  fastavail = 0;
  for (i = 0; i < NFASTBINS; ++i)
    {
      for (p = fastbin (av, i); p != 0; p = p->fd)
        {
          ++nfastblocks;
          fastavail += chunksize (p);
        }
    }
  avail += fastavail;
  /* traverse regular bins */
  for (i = 1; i < NBINS; ++i)
    {
      b = bin_at (av, i);
      for (p = last (b); p != b; p = p->bk)
        {
          ++nblocks;
          avail += chunksize (p);
        }
    }
  m->smblks += nfastblocks;
  m->ordblks += nblocks;
  m->fordblks += avail;
  m->uordblks += av->system_mem - avail;
  m->arena += av->system_mem;
  m->fsmblks += fastavail;
  if (av == &main_arena)
    {
      m->hblks = mp_.n_mmaps;
      m->hblkhd = mp_.mmapped_mem;
      m->usmblks = 0;
      m->keepcost = chunksize (av->top);
    }
}
struct mallinfo
__libc_mallinfo (void)
{
  struct mallinfo m;
  mstate ar_ptr;
  if (__malloc_initialized < 0)
    ptmalloc_init ();
  memset (&m, 0, sizeof (m));
  ar_ptr = &main_arena;
  do
    {
      __libc_lock_lock (ar_ptr->mutex);
      int_mallinfo (ar_ptr, &m);
      __libc_lock_unlock (ar_ptr->mutex);
      ar_ptr = ar_ptr->next;
    }
  while (ar_ptr != &main_arena);
  return m;
}
/*
   ------------------------------ malloc_stats ------------------------------
 */
void
__malloc_stats (void)
{
  int i;
  mstate ar_ptr;
  unsigned int in_use_b = mp_.mmapped_mem, system_b = in_use_b;
  if (__malloc_initialized < 0)
    ptmalloc_init ();
  _IO_flockfile (stderr);
  int old_flags2 = stderr->_flags2;
  stderr->_flags2 |= _IO_FLAGS2_NOTCANCEL;
  for (i = 0, ar_ptr = &main_arena;; i++)
    {
      struct mallinfo mi;
      memset (&mi, 0, sizeof (mi));
      __libc_lock_lock (ar_ptr->mutex);
      int_mallinfo (ar_ptr, &mi);
      fprintf (stderr, "Arena %d:\n", i);
      fprintf (stderr, "system bytes     = %10u\n", (unsigned int) mi.arena);
      fprintf (stderr, "in use bytes     = %10u\n", (unsigned int) mi.uordblks);
#if MALLOC_DEBUG > 1
      if (i > 0)
        dump_heap (heap_for_ptr (top (ar_ptr)));
#endif
      system_b += mi.arena;
      in_use_b += mi.uordblks;
      __libc_lock_unlock (ar_ptr->mutex);
      ar_ptr = ar_ptr->next;
      if (ar_ptr == &main_arena)
        break;
    }
  fprintf (stderr, "Total (incl. mmap):\n");
  fprintf (stderr, "system bytes     = %10u\n", system_b);
  fprintf (stderr, "in use bytes     = %10u\n", in_use_b);
  fprintf (stderr, "max mmap regions = %10u\n", (unsigned int) mp_.max_n_mmaps);
  fprintf (stderr, "max mmap bytes   = %10lu\n",
           (unsigned long) mp_.max_mmapped_mem);
  stderr->_flags2 = old_flags2;
  _IO_funlockfile (stderr);
}


int
__malloc_info (int options, FILE *fp)
{
  /* For now, at least.  */
  if (options != 0)
    return EINVAL;
  int n = 0;
  size_t total_nblocks = 0;
  size_t total_nfastblocks = 0;
  size_t total_avail = 0;
  size_t total_fastavail = 0;
  size_t total_system = 0;
  size_t total_max_system = 0;
  size_t total_aspace = 0;
  size_t total_aspace_mprotect = 0;
  if (__malloc_initialized < 0)
    ptmalloc_init ();
  fputs ("<malloc version=\"1\">\n", fp);
  /* Iterate over all arenas currently in use.  */
  mstate ar_ptr = &main_arena;
  do
    {
      fprintf (fp, "<heap nr=\"%d\">\n<sizes>\n", n++);
      size_t nblocks = 0;
      size_t nfastblocks = 0;
      size_t avail = 0;
      size_t fastavail = 0;
      struct
      {
        size_t from;
        size_t to;
        size_t total;
        size_t count;
      } sizes[NFASTBINS + NBINS - 1];
#define nsizes (sizeof (sizes) / sizeof (sizes[0]))
      __libc_lock_lock (ar_ptr->mutex);
      for (size_t i = 0; i < NFASTBINS; ++i)
        {
          mchunkptr p = fastbin (ar_ptr, i);
          if (p != NULL)
            {
              size_t nthissize = 0;
              size_t thissize = chunksize (p);
              while (p != NULL)
                {
                  ++nthissize;
                  p = p->fd;
                }
              fastavail += nthissize * thissize;
              nfastblocks += nthissize;
              sizes[i].from = thissize - (MALLOC_ALIGNMENT - 1);
              sizes[i].to = thissize;
              sizes[i].count = nthissize;
            }
          else
            sizes[i].from = sizes[i].to = sizes[i].count = 0;
          sizes[i].total = sizes[i].count * sizes[i].to;
        }
      mbinptr bin;
      struct malloc_chunk *r;
      for (size_t i = 1; i < NBINS; ++i)
        {
          bin = bin_at (ar_ptr, i);
          r = bin->fd;
          sizes[NFASTBINS - 1 + i].from = ~((size_t) 0);
          sizes[NFASTBINS - 1 + i].to = sizes[NFASTBINS - 1 + i].total
                                          = sizes[NFASTBINS - 1 + i].count = 0;
          if (r != NULL)
            while (r != bin)
              {
                size_t r_size = chunksize_nomask (r);
                ++sizes[NFASTBINS - 1 + i].count;
                sizes[NFASTBINS - 1 + i].total += r_size;
                sizes[NFASTBINS - 1 + i].from
                  = MIN (sizes[NFASTBINS - 1 + i].from, r_size);
                sizes[NFASTBINS - 1 + i].to = MAX (sizes[NFASTBINS - 1 + i].to,
                                                   r_size);
                r = r->fd;
              }
          if (sizes[NFASTBINS - 1 + i].count == 0)
            sizes[NFASTBINS - 1 + i].from = 0;
          nblocks += sizes[NFASTBINS - 1 + i].count;
          avail += sizes[NFASTBINS - 1 + i].total;
        }
      size_t heap_size = 0;
      size_t heap_mprotect_size = 0;
      size_t heap_count = 0;
      if (ar_ptr != &main_arena)
        {
          /* Iterate over the arena heaps from back to front.  */
          heap_info *heap = heap_for_ptr (top (ar_ptr));
          do
            {
              heap_size += heap->size;
              heap_mprotect_size += heap->mprotect_size;
              heap = heap->prev;
              ++heap_count;
            }
          while (heap != NULL);
        }
      __libc_lock_unlock (ar_ptr->mutex);
      total_nfastblocks += nfastblocks;
      total_fastavail += fastavail;
      total_nblocks += nblocks;
      total_avail += avail;
      for (size_t i = 0; i < nsizes; ++i)
        if (sizes[i].count != 0 && i != NFASTBINS)
          fprintf (fp, "                                                              \
  <size from=\"%zu\" to=\"%zu\" total=\"%zu\" count=\"%zu\"/>\n",
                   sizes[i].from, sizes[i].to, sizes[i].total, sizes[i].count);
      if (sizes[NFASTBINS].count != 0)
        fprintf (fp, "\
  <unsorted from=\"%zu\" to=\"%zu\" total=\"%zu\" count=\"%zu\"/>\n",
                 sizes[NFASTBINS].from, sizes[NFASTBINS].to,
                 sizes[NFASTBINS].total, sizes[NFASTBINS].count);
      total_system += ar_ptr->system_mem;
      total_max_system += ar_ptr->max_system_mem;
      fprintf (fp,
               "</sizes>\n<total type=\"fast\" count=\"%zu\" size=\"%zu\"/>\n"
               "<total type=\"rest\" count=\"%zu\" size=\"%zu\"/>\n"
               "<system type=\"current\" size=\"%zu\"/>\n"
               "<system type=\"max\" size=\"%zu\"/>\n",
               nfastblocks, fastavail, nblocks, avail,
               ar_ptr->system_mem, ar_ptr->max_system_mem);
      if (ar_ptr != &main_arena)
        {
          fprintf (fp,
                   "<aspace type=\"total\" size=\"%zu\"/>\n"
                   "<aspace type=\"mprotect\" size=\"%zu\"/>\n"
                   "<aspace type=\"subheaps\" size=\"%zu\"/>\n",
                   heap_size, heap_mprotect_size, heap_count);
          total_aspace += heap_size;
          total_aspace_mprotect += heap_mprotect_size;
        }
      else
        {
          fprintf (fp,
                   "<aspace type=\"total\" size=\"%zu\"/>\n"
                   "<aspace type=\"mprotect\" size=\"%zu\"/>\n",
                   ar_ptr->system_mem, ar_ptr->system_mem);
          total_aspace += ar_ptr->system_mem;
          total_aspace_mprotect += ar_ptr->system_mem;
        }
      fputs ("</heap>\n", fp);
      ar_ptr = ar_ptr->next;
    }
  while (ar_ptr != &main_arena);
  fprintf (fp,
           "<total type=\"fast\" count=\"%zu\" size=\"%zu\"/>\n"
           "<total type=\"rest\" count=\"%zu\" size=\"%zu\"/>\n"
           "<total type=\"mmap\" count=\"%d\" size=\"%zu\"/>\n"
           "<system type=\"current\" size=\"%zu\"/>\n"
           "<system type=\"max\" size=\"%zu\"/>\n"
           "<aspace type=\"total\" size=\"%zu\"/>\n"
           "<aspace type=\"mprotect\" size=\"%zu\"/>\n"
           "</malloc>\n",
           total_nfastblocks, total_fastavail, total_nblocks, total_avail,
           mp_.n_mmaps, mp_.mmapped_mem,
           total_system, total_max_system,
           total_aspace, total_aspace_mprotect);
  return 0;
}
```
