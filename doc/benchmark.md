
压力测试
-----------
```C++
// ping服务器
struct cping
{
    dummy:int;
}
```
clt --->>> gateway --->>> world --->>> gateway --->>> clt

clt_num  times    sec  tps
1        100000   10   10000

20       100000   42   50000

200      100000   340  58000
```shell
lscpu
Architecture:          x86_64
CPU op-mode(s):        32-bit, 64-bit
Byte Order:            Little Endian
CPU(s):                1
On-line CPU(s) list:   0
Thread(s) per core:    1
Core(s) per socket:    1
Socket(s):             1
NUMA node(s):          1
Vendor ID:             GenuineIntel
CPU family:            6
Model:                 60
Stepping:              3
CPU MHz:               3192.616
BogoMIPS:              6385.23
Hypervisor vendor:     KVM
Virtualization type:   full
L1d cache:             32K
L1i cache:             32K
L2 cache:              256K
L3 cache:              6144K
NUMA node0 CPU(s):     0
```

```shell
Architecture:          x86_64
CPU op-mode(s):        32-bit, 64-bit
Byte Order:            Little Endian
CPU(s):                1
On-line CPU(s) list:   0
Thread(s) per core:    1
Core(s) per socket:    1
Socket(s):             1
NUMA node(s):          1
Vendor ID:             AuthenticAMD
CPU family:            21
Model:                 16
Stepping:              1
CPU MHz:               1896.550
BogoMIPS:              3793.10
Hypervisor vendor:     KVM
Virtualization type:   full
L1d cache:             16K
L1i cache:             64K
L2 cache:              2048K
NUMA node0 CPU(s):     0

200      100000   1091  20000
```