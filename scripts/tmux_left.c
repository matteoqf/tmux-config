/*
 * tmux_left_bin.c
 * Powerline 风格左 status bar
 * [IME]  [session]  [BG]
 *
 * argv[1] = session name
 * argv[2] = window name
 * argv[3] = "--prefix" if prefix held
 * argv[4] = IME state: "EN" or "CN"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define C_BG       "#282A36"
#define C_SESSION  "#50FA7B"
#define C_WINDOW   "#D19A66"
#define C_TEXT_D   "#282A36"
#define C_PRE      "#FFEE00"
#define C_IME_EN   "#FF5555"
#define C_IME_CN   "#6272A4"

#define ARR_SZ 32

/* 右指向 : filled(left)=prev_bg, point(right)=next_bg */
static void arr_r(char *out, size_t sz, const char *prev_bg, const char *next_bg) {
    snprintf(out, sz, "#[fg=%s,bg=%s]\uE0B0", next_bg, prev_bg);
}

int main(int argc, char *argv[]) {
    const char *session   = (argc > 1) ? argv[1] : "tmux";
    const char *win_name  = (argc > 2) ? argv[2] : "";
    const char *arg3      = (argc > 3) ? argv[3] : "";
    const char *ime_state = (argc > 4) ? argv[4] : "CN";

    int prefix = (strcmp(arg3, "--prefix") == 0);
    int is_en  = (strcmp(ime_state, "EN") == 0);

    { char *p = strchr(session, '\n');  if (p) *p = 0; }
    { char *p = strchr(win_name, '\n'); if (p) *p = 0; }

    /* IME block */
    const char *ime_bg = is_en ? C_IME_EN : C_IME_CN;
    /* Session block */
    const char *sess_bg = C_SESSION;
    const char *txt_fg  = C_TEXT_D;

    /* [IME] [session] */

    /* 格式: [IME] [session] */
    printf("#[fg=%s,bg=%s] %s #[fg=%s,bg=%s] %s",
           txt_fg, ime_bg, ime_state,
           txt_fg, sess_bg, session);

    return 0;
}
