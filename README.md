# librctl

## Guide to a Good Setup

### Finding the right CPU

It is nice to find a CPU that does not have pinned work on top of it. Additionally, also make sure that your SMT sibling is not vital. Generally CPU0 is the dirtiest one for a variety of reasons, so I advise not to use it. For some, it is a convention to use the first few CPUs as housekeeping CPUs, therefore I recommend generally skipping the first 8 CPUs if you can. Labrctl sets the first 8 CPUs as housekeeping ones.

To check SMT siblings, replace the CPU number (in this case 18) with the CPU in question and run:

```bash
cat /sys/devices/system/cpu/cpu18/topology/thread_siblings_list

18,42
```

The next thing to look out for is who shares the cache with who. Run:

```bash
lscpu -e=CPU,CORE,SOCKET,NODE,CACHE,ONLINE

CPU CORE SOCKET NODE L1d:L1i:L2:L3 ONLINE
  0    0      0    0 0:0:0:0          yes
  1    1      0    0 1:1:1:0          yes
  2    2      0    0 2:2:2:0          yes
  3    3      0    0 3:3:3:0          yes
  4    4      0    0 4:4:4:0          yes
  5    5      0    0 5:5:5:0          yes
  6    6      0    0 8:8:8:1          yes
  7    7      0    0 9:9:9:1          yes
  8    8      0    0 10:10:10:1       yes
  9    9      0    0 11:11:11:1       yes
 10   10      0    0 12:12:12:1       yes
 11   11      0    0 13:13:13:1       yes
 12   12      0    0 16:16:16:2       yes
 13   13      0    0 17:17:17:2       yes
 14   14      0    0 18:18:18:2       yes
 15   15      0    0 19:19:19:2       yes
 16   16      0    0 20:20:20:2       yes
 17   17      0    0 21:21:21:2       yes
 18   18      0    0 24:24:24:3       yes
 19   19      0    0 25:25:25:3       yes
 20   20      0    0 26:26:26:3       yes
 21   21      0    0 27:27:27:3       yes
 22   22      0    0 28:28:28:3       yes
 23   23      0    0 29:29:29:3       yes
 24    0      0    0 0:0:0:0          yes
 25    1      0    0 1:1:1:0          yes
 26    2      0    0 2:2:2:0          yes
 27    3      0    0 3:3:3:0          yes
 28    4      0    0 4:4:4:0          yes
 29    5      0    0 5:5:5:0          yes
 30    6      0    0 8:8:8:1          yes
 31    7      0    0 9:9:9:1          yes
 32    8      0    0 10:10:10:1       yes
 33    9      0    0 11:11:11:1       yes
 34   10      0    0 12:12:12:1       yes
 35   11      0    0 13:13:13:1       yes
 36   12      0    0 16:16:16:2       yes
 37   13      0    0 17:17:17:2       yes
 38   14      0    0 18:18:18:2       yes
 39   15      0    0 19:19:19:2       yes
 40   16      0    0 20:20:20:2       yes
 41   17      0    0 21:21:21:2       yes
 42   18      0    0 24:24:24:3       yes
 43   19      0    0 25:25:25:3       yes
 44   20      0    0 26:26:26:3       yes
 45   21      0    0 27:27:27:3       yes
 46   22      0    0 28:28:28:3       yes
 47   23      0    0 29:29:29:3       yes
```

As you can see, CPU0 to CPU5 share the same L3 cluster, and since they are usually housekeeping, it is ill-advised to use anything that shares a cache with them. From the above output, I would likely choose CPU 17 or around that as they are far away from the housekeeping cores and share L3 with "rarely used" ones.

The next useful thing to find out is what work they have. Run:

```bash
ps -eLo pid,tid,psr,comm | awk '$3 == 17'

    118     118  17 cpuhp/17
    119     119  17 idle_inject/17
    120     120  17 migration/17
    121     121  17 ksoftirqd/17
    123     123  17 kworker/17:0H-events_highpri
    351     351  17 irq/38-AMD-Vi3-GA
    373     373  17 kworker/17:1-events
    498     498  17 kworker/17:1H-kblockd
   1311    1712  17 snapd
   1311   20344  17 snapd
   1572    1584  17 tailscaled
   7763    7820  17 llvmpipe-21
   7896    7896  17 at-spi2-registr
   7952    7952  17 gsd-power
   8299    8299  17 gvfsd-trash
  60950   60950  17 kworker/17:0
```

The above script lists the tasks for CPU17. You can set the CPU number in the `awk` script. Some tasks are pinned to CPU (marked by the `/cpunum` after their `comm`), the kernel usually calls them bound tasks. An unbounded task just happens to reside on that CPU and can be moved off. Such is `tailscale` from the above example. Make sure your chosen CPU does not have many bound tasks.

You should set IRQ affinity either way, but to double check what CPUs saw some action you can check `/proc/interrupts`.

It is more of an art to choose your experiment CPU. The above help, but there are definitely factors that I'm skipping and ones that I do not even know about.

### Boot Parameters

- rcu_nocbs=cpunum ([explanation](https://docs.redhat.com/en/documentation/red_hat_enterprise_linux_for_real_time/7/html/tuning_guide/offloading_rcu_callbacks))
- nohz_full=cpunum ([explanation](https://docs.redhat.com/en/documentation/red_hat_enterprise_linux_for_real_time/7/html/tuning_guide/isolating_cpus_using_tuned-profiles-realtime))
- isolcpus=managed_irq,domain,cpunum ([explanation](https://access.redhat.com/solutions/480473))

[This guide](https://docs.kernel.org/admin-guide/cpu-isolation.html) can help you understand all of the above.
