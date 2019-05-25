
* 参考boost实现object_pool，把astar和aoi中的缓存改由object_pool实现，其他系统也看下有没有替换的
* mysql增加lua table直接存mysql接口，并且测试一下和格式化字符串的效率
* 实现场景的static区域属性(不可修改)和dynamic区域属性(可根据逻辑创建和修改)
* fast_class的设计，主要是把基类的函数拷贝到当前子类。由于现在是所有文件热更,不存在之前单个文件热更导致的子类热更不到的情况
* 技能的配置比较复杂，看看能不能设计出多action，多result，多条件控制又比较容易明白的excel配置
* 把所有的static、global变量放到一个static_global类去，以控制他们的创建、销毁顺序,把ssl mysql mongodb的初始化也放里面,这个现在在global.cpp里定义
* 主循环是否使用分时器
* async_worker（属性计算、技能延后，技能延后看看是放这里还是主循环）
* profile看效率问题
* lint优化代码
* 服务器socket预分配内存太大，当服务器之间连接太多(比如中心服，500条连接)，就会分不了内存
* 要不要把运行目录放到bin目录，这样生成的core文件，error文件以及log、setting这些与版本无关的全在一个目录里了
* mongodb的result感觉要用个object_pool
* lclass弄个继承，基础的只是push到lua，不会创建。然后把C++中push到lua的类多余的构造函数去掉
* pbc中错误提示不明确(本来是int，发个float就重现了)

#优化定时器（加到wiki）
* app定时器，只保留5秒，并且修正为整分钟触发(用real time修正)
* 定时器可配置是否处理time jump
* app一次性delay定时器，
* 玩家一次性delay定时器，
* 副本一次性delay定时器，副本延迟、销毁退出用
* 绝对定时器(absolute timer,指定年月日时分秒运行)
* 相对定时器(relative timer,指定周几，几日，时分秒，按月，按周循环)
* 用小堆实现系统长时定时器，把上面的一些定时器管理起来

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

#AOI设计
* 算法：十字链表 九宫格 灯塔
* 作用：移动、攻击广播、技能寻敌
* 事件：移动、加入、离开事件，要针对类型处理，如玩家和npc是不一样的
* 异步：事件触发可以弄成异步的，但是目前只考虑同步

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


```lua
-- http://lua-users.org/wiki/ObjectOrientationClosureApproach
function test_table_object( this,cb )
	return { this = this,cb = cb }
end

function test_closure_object( this,cb )
	return function()
		return cb( this )
	end
end

local function cb()
	print("do nothing ...")
end

local max = 1000000

local table_mgr = {}
local closure_mgr = {}

function test_table()
	
	local beg_m = collectgarbage("count")
	local beg_tm = os.clock()

	for idx = 1,max do
		table_mgr[idx] = test_table_object( {},cb )
	end

	local end_m = collectgarbage("count")
	local end_tm = os.clock()

	print("table test",end_m - beg_m,end_tm - beg_tm)
end

function test_closure()
	
	local beg_m = collectgarbage("count")
	local beg_tm = os.clock()

	for idx = 1,max do
		table_mgr[idx] = test_closure_object( {},cb )
	end

	local end_m = collectgarbage("count")
	local end_tm = os.clock()

	print("closure test",end_m - beg_m,end_tm - beg_tm)
end

collectgarbage("stop")
collectgarbage("collect")

--[[
Program 'lua53.exe' started in 'E:\7yao\lua_test' (pid: 1532).
closure test	117946.5	0.489
table test	125000.0	0.446
]]

test_closure()
test_table()

```