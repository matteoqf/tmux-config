/*
 * tmux_left_bin.c
 * Powerline 风格左 status bar
 * 左侧 Powerline 风格: 文字块 + 箭头在文字右侧
 * 镜像右侧: 箭头在文字左侧
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define C_BG       "#282A36"
#define C_SESSION  "#50FA7B"
#define C_WINDOW   "#D19A66"
#define C_TEXT_D   "#282A36"
#define C_PRE      "#FFEE00"

#define ARR_SZ 32

/* 右指向 : filled(left)=prev_bg, point(right)=next_bg */
static void arr_r(char *out, size_t sz, const char *prev_bg, const char *next_bg) {
    snprintf(out, sz, "#[fg=%s,bg=%s]\uE0B0", next_bg, prev_bg);
}

/* 左指向 : filled(right)=prev_bg, point(left)=next_bg */
static void arr_l(char *out, size_t sz, const char *prev_bg, const char *next_bg) {
    snprintf(out, sz, "#[fg=%s,bg=%s]\uE0B2", prev_bg, next_bg);
}

int main(int argc, char *argv[]) {
    const char *session   = (argc > 1) ? argv[1] : "tmux";
    const char *win_name  = (argc > 2) ? argv[2] : "";
    const char *arg3      = (argc > 3) ? argv[3] : "";
    int prefix = (strcmp(arg3, "--prefix") == 0);

    /* strip newline */
    { char *p = strchr(session, '\n');  if (p) *p = 0; }
    { char *p = strchr(win_name, '\n'); if (p) *p = 0; }

    const char *seg_bg, *win_bg, *txt_fg;

    if (prefix) {
        seg_bg  = C_PRE;
        win_bg  = C_PRE;
        txt_fg  = C_TEXT_D;
    } else {
        seg_bg  = C_SESSION;
        win_bg  = C_WINDOW;
        txt_fg  = C_TEXT_D;
    }

    char a2[ARR_SZ];

    /* 布局: [session] a2 [C_BG]
     * a2: 右指向箭头，在 session 右侧作为 session 右边界。
     *     filled left=session色(绿)，point right=C_BG色(深)
     */
    arr_r(a2, sizeof(a2), seg_bg, C_BG);

    /* 格式: [session] [C_BG] */
    printf("#[fg=%s,bg=%s] %s #[fg=%s,bg=%s]",
           txt_fg, seg_bg,  session,
           txt_fg, C_BG);

    return 0;
}
