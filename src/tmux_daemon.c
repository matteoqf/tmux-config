/*
 * tmux_status_daemon.c
 * 高性能 tmux 状态栏数据采集守护进程
 * macOS Apple Silicon 专用
 *
 * 架构:
 *   - 常驻内存，每2秒采集一次系统数据
 *   - 写入 POSIX 共享内存 /tmux_status
 *   - tmux_status_read 通过共享内存读取，无进程启动开销
 *
 * 编译: clang -O2 -o tmux_status_daemon tmux_status_daemon.c -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <libproc.h>
#include <time.h>

#define SHM_NAME       "/tmux_status"
#define SEM_NAME       "/tmux_status_sem"
#define WEATHER_CACHE   "/tmp/tmux_weather_cache"
#define WEATHER_TTL     300  /* 5分钟 */
#define DAEMON_INTERVAL 2    /* 秒 */

/* ---------- 共享数据结构 ---------- */
typedef struct {
    /* CPU: 百分比 */
    double  cpu_user;
    double  cpu_system;
    double  cpu_idle;

    /* RAM: MB */
    uint64_t ram_total;
    uint64_t ram_free;
    uint64_t ram_avail;

    /* NET: 字节 (差值由 reader 计算) */
    uint64_t net_rx_bytes;
    uint64_t net_tx_bytes;
    time_t   net_ts;

    /* 天气: 缓存文本 */
    char     weather[128];
    time_t   weather_ts;

    /* 时间戳: 最后更新 */
    time_t   updated_at;

    /* 版本号: 每次更新递增，reader 验活 */
    uint64_t version;
} SharedStatus;

/* ---------- 全局变量 ---------- */
static SharedStatus *g_shm = NULL;
static int           g_shm_fd = -1;
static int           g_sem_id = -1;
static int           g_run = 1;

/* ---------- 信号处理 ---------- */
static void signal_handler(int sig) {
    (void)sig;
    g_run = 0;
}

/* ---------- POSIX 信号量 (替代 System V) ---------- */
static int sem_create(const char *name, int init_val) {
    sem_t *s = sem_open(name, O_CREAT | O_EXCL, 0644, init_val);
    if (s != SEM_FAILED) return 0;
    /* 已存在则打开 */
    s = sem_open(name, 0);
    if (s == SEM_FAILED) return -1;
    return 0;
}

static void sem_lock(void) {
    sem_t *s = sem_open(SEM_NAME, 0);
    if (s != SEM_FAILED) { sem_wait(s); sem_close(s); }
}

static void sem_unlock(void) {
    sem_t *s = sem_open(SEM_NAME, 0);
    if (s != SEM_FAILED) { sem_post(s); sem_close(s); }
}

/* ---------- 共享内存初始化 ---------- */
static int shm_init(void) {
    /* 创建共享内存对象 */
    g_shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0644);
    if (g_shm_fd == -1) {
        perror("shm_open");
        return -1;
    }

    /* 设置大小 */
    if (ftruncate(g_shm_fd, sizeof(SharedStatus)) == -1) {
        perror("ftruncate");
        return -1;
    }

    /* 映射到进程空间 */
    g_shm = mmap(NULL, sizeof(SharedStatus),
                 PROT_READ | PROT_WRITE, MAP_SHARED, g_shm_fd, 0);
    if (g_shm == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    /* 初始化 */
    memset(g_shm, 0, sizeof(SharedStatus));

    /* 信号量 */
    sem_unlink(SEM_NAME); /* 清理旧的 */
    if (sem_create(SEM_NAME, 1) != 0) {
        fprintf(stderr, "sem_create failed\n");
    }

    return 0;
}

static void shm_close(void) {
    if (g_shm && g_shm != MAP_FAILED) {
        munmap(g_shm, sizeof(SharedStatus));
    }
    if (g_shm_fd != -1) {
        close(g_shm_fd);
    }
    shm_unlink(SHM_NAME);
    sem_unlink(SEM_NAME);
}

/* ---------- CPU 采集 (macOS) ---------- */
static processor_info_array_t g_cpu_info_prev = NULL;
static mach_msg_type_number_t  g_cpu_info_count = 0;
static int                     g_cpu_prev_init = 0;

static void update_cpu(void) {
    mach_port_t port = mach_host_self();
    processor_info_array_t cpu_info;
    mach_msg_type_number_t cpu_info_count;

    natural_t num_cpu;
    kern_return_t err = host_processor_info(port, PROCESSOR_CPU_LOAD_INFO,
                                            &num_cpu, &cpu_info, &cpu_info_count);
    if (err != KERN_SUCCESS) {
        mach_port_deallocate(mach_task_self(), port);
        return;
    }

    double total_user = 0, total_system = 0, total_idle = 0;
    natural_t num = cpu_info_count / CPU_STATE_MAX;

    for (natural_t i = 0; i < num; i++) {
        int offset = i * CPU_STATE_MAX;
        if (g_cpu_prev_init && g_cpu_info_prev) {
            uint32_t user   = cpu_info[offset + CPU_STATE_USER]   - g_cpu_info_prev[offset + CPU_STATE_USER];
            uint32_t system = cpu_info[offset + CPU_STATE_SYSTEM] - g_cpu_info_prev[offset + CPU_STATE_SYSTEM];
            uint32_t idle   = cpu_info[offset + CPU_STATE_IDLE]    - g_cpu_info_prev[offset + CPU_STATE_IDLE];
            uint32_t total  = user + system + idle;
            if (total > 0) {
                total_user   += 100.0 * user   / total;
                total_system += 100.0 * system / total;
                total_idle   += 100.0 * idle   / total;
            }
        }
    }

    g_shm->cpu_user   = total_user   / num;
    g_shm->cpu_system = total_system / num;
    g_shm->cpu_idle   = total_idle   / num;

    if (g_cpu_info_prev) munmap(g_cpu_info_prev, g_cpu_info_count * sizeof(int));
    g_cpu_info_prev  = cpu_info;
    g_cpu_info_count = cpu_info_count;
    g_cpu_prev_init  = 1;
    mach_port_deallocate(mach_task_self(), port);
}

/* ---------- RAM 采集 (macOS) ---------- */
static void update_ram(void) {
    mach_port_t port = mach_host_self();
    struct vm_statistics64 vm;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

    kern_return_t err = host_statistics64(port, HOST_VM_INFO64, (host_info64_t)&vm, &count);
    if (err != KERN_SUCCESS) {
        mach_port_deallocate(mach_task_self(), port);
        return;
    }

    uint64_t page_size = (uint64_t)vm_kernel_page_size;
    if (page_size == 0) page_size = 4096;

    g_shm->ram_total  = (uint64_t)vm.free_count   * page_size;
    g_shm->ram_free   = (uint64_t)vm.free_count   * page_size;
    g_shm->ram_avail  = (uint64_t)vm.active_count * page_size +
                        (uint64_t)vm.inactive_count * page_size +
                        (uint64_t)vm.free_count    * page_size;

    /* macOS: total RAM 从 sysctl hw.memsize 获取 */
    size_t mlen = sizeof(uint64_t);
    uint64_t mem_total;
    if (sysctlbyname("hw.memsize", &mem_total, &mlen, NULL, 0) == 0) {
        g_shm->ram_total = mem_total / (1024 * 1024); /* MB */
    } else {
        g_shm->ram_total = 0;
    }

    mach_port_deallocate(mach_task_self(), port);
}

/* ---------- 网络采集 (macOS) ---------- */
static uint64_t g_net_rx_prev = 0;
static uint64_t g_net_tx_prev = 0;
static time_t   g_net_ts_prev = 0;
static int      g_net_first = 1;

static void update_net(void) {
    struct ifaddrs *ifaddr, *ifa;
    uint64_t total_rx = 0, total_tx = 0;

    if (getifaddrs(&ifaddr) == -1) return;

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_LINK) continue;
        /* 跳过 loopback */
        if (strcmp(ifa->ifa_name, "lo0") == 0) continue;

        struct if_data *ifdata = (struct if_data *)ifa->ifa_data;
        if (ifdata == NULL) continue;
        total_rx += ifdata->ifi_ibytes;
        total_tx += ifdata->ifi_obytes;
    }
    freeifaddrs(ifaddr);

    time_t now = time(NULL);
    if (!g_net_first && g_net_ts_prev > 0) {
        int dt = (int)(now - g_net_ts_prev);
        if (dt <= 0) dt = 1;
        /* 差分计算速率 */
        g_shm->net_rx_bytes = (total_rx >= g_net_rx_prev) ? (total_rx - g_net_rx_prev) / dt : 0;
        g_shm->net_tx_bytes = (total_tx >= g_net_tx_prev) ? (total_tx - g_net_tx_prev) / dt : 0;
    } else {
        g_shm->net_rx_bytes = 0;
        g_shm->net_tx_bytes = 0;
        g_net_first = 0;
    }

    g_net_rx_prev = total_rx;
    g_net_tx_prev = total_tx;
    g_net_ts_prev = now;
    g_shm->net_ts = now;
}

/* ---------- 天气 (调用外部脚本缓存) ---------- */
static void update_weather(void) {
    time_t now = time(NULL);
    if (g_shm->weather_ts > 0 && (now - g_shm->weather_ts) < WEATHER_TTL) {
        return; /* 缓存有效 */
    }

    /* 读取缓存文件 */
    FILE *fp = fopen(WEATHER_CACHE, "r");
    if (fp) {
        char buf[128] = {0};
        if (fgets(buf, sizeof(buf), fp)) {
            /* 去掉换行 */
            size_t len = strlen(buf);
            if (len > 0 && buf[len-1] == '\n') buf[len-1] = 0;
            strncpy(g_shm->weather, buf, sizeof(g_shm->weather) - 1);
            g_shm->weather_ts = now;
        }
        fclose(fp);
    } else {
        /* 文件不存在，尝试触发获取（由 wttr_wrapper.sh 负责） */
        /* 这里只是读缓存，触发由外部 cron 或 install.sh 处理 */
    }
}

/* ---------- 主循环 ---------- */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    /* 守护进程化 */
    daemon(0, 0);

    /* 信号处理 */
    signal(SIGTERM, signal_handler);
    signal(SIGINT,  signal_handler);

    /* 初始化共享内存 */
    if (shm_init() != 0) {
        fprintf(stderr, "shm_init failed\n");
        return 1;
    }

    /* 初始采集 */
    update_cpu();
    update_ram();
    update_net();
    update_weather();
    g_shm->updated_at = time(NULL);
    g_shm->version = 1;

    /* 等待所有 reader 准备好 (短暂睡眠) */
    usleep(200000);

    /* 主循环 */
    while (g_run) {
        sleep(DAEMON_INTERVAL);

        sem_lock();
        update_cpu();
        update_ram();
        update_net();
        update_weather();
        g_shm->updated_at = time(NULL);
        g_shm->version++;
        sem_unlock();
    }

    /* 清理 */
    shm_close();
    return 0;
}
