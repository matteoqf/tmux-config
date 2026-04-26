/*
 * tmux_left.c
 * Powerline 风格左 status bar
 * 检测输入法状态：中文输入法下不显示 prefix 高亮
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CoreGraphics/CoreGraphics.h>
#include <Carbon/Carbon.h>

#define C_BG       "#282A36"
#define C_SESSION  "#50FA7B"
#define C_WINDOW   "#D19A66"
#define C_TEXT_D   "#282A36"
#define C_PRE      "#FFEE00"

#define ARR_SZ 32

/* 检测是否在中文输入法模式 */
static int is_cjk_input(void) {
    TISInputSourceRef src = TISCopyCurrentKeyboardInputSource();
    if (!src) return 0;

    /* 检查 ASCII capable 标志
     * Rime 等输入法在中文模式下 kTISPropertyInputSourceIsASCIICapable = false
     * 在英文/ASCII 模式下 = true */
    CFBooleanRef asciiCap = TISGetInputSourceProperty(src, kTISPropertyInputSourceIsASCIICapable);
    if (asciiCap && !CFBooleanGetValue(asciiCap)) {
        CFRelease(src);
        return 1; /* 中文输入法 */
    }

    CFRelease(src);
    return 0;
}

/* 右指向 : filled(left)=prev_bg, point(right)=next_bg */
static void arr_r(char *out, size_t sz, const char *prev_bg, const char *next_bg) {
    snprintf(out, sz, "#[fg=%s,bg=%s]\uE0B0", next_bg, prev_bg);
}

int main(int argc, char *argv[]) {
    const char *session  = (argc > 1) ? argv[1] : "tmux";
    const char *win_name = (argc > 2) ? argv[2] : "";
    const char *arg3     = (argc > 3) ? argv[3] : "";
    int prefix = (strcmp(arg3, "--prefix") == 0);

    /* strip newline */
    { char *p = strchr(session, '\n');  if (p) *p = 0; }
    { char *p = strchr(win_name, '\n'); if (p) *p = 0; }

    /* 中文输入法模式下，忽略 prefix 高亮 */
    if (prefix && is_cjk_input()) {
        prefix = 0;
    }

    const char *seg_bg, *txt_fg;

    if (prefix) {
        seg_bg  = C_PRE;
        txt_fg  = C_TEXT_D;
    } else {
        seg_bg  = C_SESSION;
        txt_fg  = C_TEXT_D;
    }

    /* 布局: [session] [C_BG] */
    printf("#[fg=%s,bg=%s] %s #[fg=%s,bg=%s]",
           txt_fg, seg_bg,  session,
           txt_fg, C_BG);

    return 0;
}
