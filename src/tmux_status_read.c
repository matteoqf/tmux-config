/*
 * tmux_status_read.c
 * tmux #() 调用专用 — 五段 Powerline
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>

#define SHM_NAME   "/tmux_status"
#define DAEMON_TTL 10

typedef struct {
    double   cpu_user, cpu_system, cpu_idle;
    uint64_t ram_total, ram_free, ram_avail;
    uint64_t net_rx_bytes, net_tx_bytes;
    time_t   net_ts;
    char     weather[128];
    time_t   weather_ts;
    time_t   updated_at;
    uint64_t version;
} SharedStatus;

static const char *C_BG  = "#282A36";
static const char *C_CPU = "#E2B18A";
static const char *C_RAM = "#98C379";
static const char *C_NET = "#61AFEF";
static const char *C_WTH = "#C678DD";
static const char *C_TME = "#ABB2BF";
static const char *C_PFX = "#FF5555";

/* Powerline 右指向: 无间隙三角形 */
static void arr(char *out, size_t sz, const char *prev, const char *curr) {
    snprintf(out, sz, "#[fg=%s,bg=%s]\uE0B2", curr, prev);
}

static SharedStatus *g_shm = NULL;
static int           g_shm_fd = -1;
static time_t         g_last_check = 0;

static int shm_connect(void) {
    if (g_shm != NULL) return 0;
    g_shm_fd = shm_open(SHM_NAME, O_RDONLY, 0);
    if (g_shm_fd == -1) return -1;
    g_shm = mmap(NULL, sizeof(SharedStatus), PROT_READ, MAP_SHARED, g_shm_fd, 0);
    if (g_shm == MAP_FAILED) { close(g_shm_fd); g_shm_fd = -1; return -1; }
    return 0;
}
static void shm_disconnect(void) {
    if (g_shm && g_shm != MAP_FAILED) { munmap(g_shm, sizeof(SharedStatus)); g_shm = NULL; }
    if (g_shm_fd != -1) { close(g_shm_fd); g_shm_fd = -1; }
}

static void fmt_speed(uint64_t bps, char *out, size_t sz) {
    uint64_t bits = bps * 8;
    if (bits >= 1e9)       snprintf(out, sz, "%4.1fG", bits/1e9);
    else if (bits >= 1e6)  snprintf(out, sz, "%4.0fM", bits/1e6);
    else if (bits >= 1e3)  snprintf(out, sz, "%4.0fK", bits/1e3);
    else                    snprintf(out, sz, "%4lluB", (unsigned long long)bits);
}

static const char *WDAY_ZH[] = {"周日","周一","周二","周三","周四","周五","周六"};
static void fmt_time(char *out, size_t sz) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    int wday = (tm->tm_wday + 6) % 7;
    snprintf(out, sz, "%s %02d/%02d %02d:%02d",
             WDAY_ZH[wday], tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);
}

static int daemon_alive(void) {
    time_t now = time(NULL);
    if (g_last_check > 0 && (now - g_last_check) < 5)
        return (g_shm != NULL && g_shm->version > 0);
    g_last_check = now;
    if (!g_shm) return 0;
    return (g_shm->version > 0 && (now - g_shm->updated_at) <= DAEMON_TTL);
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    char buf[256];
    char a1[128], a2[128], a3[128], a4[128], a5[128];

    if (shm_connect() != 0) {
        arr(a1, sizeof(a1), C_BG,  C_CPU);
        arr(a2, sizeof(a2), C_CPU, C_RAM);
        arr(a3, sizeof(a3), C_RAM, C_NET);
        arr(a4, sizeof(a4), C_NET, C_WTH);
        arr(a5, sizeof(a5), C_WTH, C_TME);
        printf("%s#[bg=%s,fg=%s] N/A %s#[bg=%s,fg=%s] N/A %s#[bg=%s,fg=%s] N/A %s#[bg=%s,fg=%s] N/A %s#[bg=%s,fg=%s] N/A ",
            a1, C_CPU, C_BG,
            a2, C_RAM, C_BG,
            a3, C_NET, C_BG,
            a4, C_WTH, C_BG,
            a5, C_TME, C_BG);
        return 0;
    }

    if (!daemon_alive()) {
        arr(a1, sizeof(a1), C_BG,  C_PFX);
        arr(a2, sizeof(a2), C_PFX, C_RAM);
        arr(a3, sizeof(a3), C_RAM, C_NET);
        arr(a4, sizeof(a4), C_NET, C_WTH);
        arr(a5, sizeof(a5), C_WTH, C_TME);
        printf("%s#[bg=%s,fg=%s] WAIT %s#[bg=%s,fg=%s] -- %s#[bg=%s,fg=%s] -- %s#[bg=%s,fg=%s] -- %s#[bg=%s,fg=%s] -- ",
            a1, C_PFX, C_BG,
            a2, C_RAM, C_BG,
            a3, C_NET, C_BG,
            a4, C_WTH, C_BG,
            a5, C_TME, C_BG);
        shm_disconnect(); return 0;
    }

    /* CPU */
    arr(a1, sizeof(a1), C_BG,  C_CPU);
    arr(a2, sizeof(a2), C_CPU, C_RAM);
    int cpu_pct = (int)(g_shm->cpu_user + g_shm->cpu_system + 0.5);
    if (cpu_pct > 99) cpu_pct = 99;
    printf("%s#[bg=%s,fg=%s] CPU %2d%% ", a1, C_CPU, C_BG, cpu_pct);

    /* RAM */
    arr(a1, sizeof(a1), C_CPU, C_RAM);
    arr(a2, sizeof(a2), C_RAM, C_NET);
    uint64_t used_mb = (g_shm->ram_total * 1024 * 1024 >= g_shm->ram_avail)
                        ? (g_shm->ram_total - g_shm->ram_avail / (1024*1024)) : 0;
    snprintf(buf, sizeof(buf), "RAM %1.1fG/32G", used_mb / 1024.0);
    printf("%s#[bg=%s,fg=%s] %s ", a1, C_RAM, C_BG, buf);

    /* NET */
    arr(a1, sizeof(a1), C_RAM, C_NET);
    arr(a2, sizeof(a2), C_NET, C_WTH);
    char rx[32], tx[32];
    fmt_speed(g_shm->net_rx_bytes, rx, sizeof(rx));
    fmt_speed(g_shm->net_tx_bytes, tx, sizeof(tx));
    snprintf(buf, sizeof(buf), "↓%4s ↑%4s", rx, tx);
    printf("%s#[bg=%s,fg=%s] %s ", a1, C_NET, C_BG, buf);

    /* WEATHER */
    arr(a1, sizeof(a1), C_NET, C_WTH);
    arr(a2, sizeof(a2), C_WTH, C_TME);
    const char *wth = g_shm->weather[0] ? g_shm->weather : "N/A";
    printf("%s#[bg=%s,fg=%s] %s ", a1, C_WTH, C_BG, wth);

    /* TIME */
    arr(a1, sizeof(a1), C_WTH, C_TME);
    fmt_time(buf, sizeof(buf));
    printf("%s#[bg=%s,fg=%s] %s ", a1, C_TME, C_BG, buf);

    shm_disconnect();
    return 0;
}
