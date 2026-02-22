// RGX-OS 3DS Homebrew - Port of RGX Python v3
// Requires: devkitARM + libctru + citro2d

#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define MAX_CMD_LEN      256
#define MAX_OUTPUT_LINES 20
#define MAX_LINE_LEN     64
#define SCREEN_TOP_W     400
#define SCREEN_TOP_H     240
#define SCREEN_BOT_W     320
#define SCREEN_BOT_H     240
#define KEY_OFFSET_Y     55

// Colors
#define COL_GREEN    C2D_Color32(0,   255,  80,  255)
#define COL_YELLOW   C2D_Color32(255, 255,  80,  255)
#define COL_WHITE    C2D_Color32(255, 255, 255,  255)
#define COL_PURPLE   C2D_Color32(80,    0, 120,  255)
#define COL_DARK     C2D_Color32(15,   15,  30,  255)
#define COL_DARKBOT  C2D_Color32(20,   10,  35,  255)
#define COL_KEY      C2D_Color32(60,   20, 100,  255)
#define COL_ENTER    C2D_Color32(0,   120,  60,  255)
#define COL_BACK     C2D_Color32(150,  40,   0,  255)
#define COL_SPACE    C2D_Color32(40,   40, 100,  255)
#define COL_BORDER   C2D_Color32(180, 100, 255,  255)
#define COL_INPUTBAR C2D_Color32(40,    0,  60,  255)

// ==========================================
// KEYBOARD
// ==========================================
typedef struct { const char *label; int x, y, w, h; char value; } Key;

static Key keyboard[] = {
    {"1",  0,  0,28,28,'1'},{"2", 29, 0,28,28,'2'},{"3", 58, 0,28,28,'3'},
    {"4", 87,  0,28,28,'4'},{"5",116, 0,28,28,'5'},{"6",145, 0,28,28,'6'},
    {"7",174,  0,28,28,'7'},{"8",203, 0,28,28,'8'},{"9",232, 0,28,28,'9'},
    {"0",261,  0,28,28,'0'},{"-",290, 0,28,28,'-'},
    {"q",  0, 30,28,28,'q'},{"w", 29,30,28,28,'w'},{"e", 58,30,28,28,'e'},
    {"r", 87, 30,28,28,'r'},{"t",116,30,28,28,'t'},{"y",145,30,28,28,'y'},
    {"u",174, 30,28,28,'u'},{"i",203,30,28,28,'i'},{"o",232,30,28,28,'o'},
    {"p",261, 30,28,28,'p'},
    {"a",  0, 60,28,28,'a'},{"s", 29,60,28,28,'s'},{"d", 58,60,28,28,'d'},
    {"f", 87, 60,28,28,'f'},{"g",116,60,28,28,'g'},{"h",145,60,28,28,'h'},
    {"j",174, 60,28,28,'j'},{"k",203,60,28,28,'k'},{"l",232,60,28,28,'l'},
    {"z",  0, 90,28,28,'z'},{"x", 29,90,28,28,'x'},{"c", 58,90,28,28,'c'},
    {"v", 87, 90,28,28,'v'},{"b",116,90,28,28,'b'},{"n",145,90,28,28,'n'},
    {"m",174, 90,28,28,'m'},{".",203,90,28,28,'.'},{"/"  ,232,90,28,28,'/'},
    {"_",261, 90,28,28,'_'},
    {"SPACE",0,120,140,28,' '},{"<-",142,120,56,28,'\b'},{"ENTER",200,120,80,28,'\n'},
    {":",  0,150,28,28,':'},{"!",29,150,28,28,'!'},{"?",58,150,28,28,'?'},
    {"@", 87,150,28,28,'@'},{"#",116,150,28,28,'#'},
};
#define KEY_COUNT (sizeof(keyboard)/sizeof(keyboard[0]))

// ==========================================
// TERMINAL STATE
// ==========================================
static char output[MAX_OUTPUT_LINES][MAX_LINE_LEN];
static int  output_count = 0;
static char cmd_buffer[MAX_CMD_LEN];
static int  cmd_len = 0;
static char cwd[512];

// ==========================================
// OUTPUT HELPERS
// ==========================================
void term_print(const char *msg) {
    char tmp[MAX_LINE_LEN * 4];
    strncpy(tmp, msg, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = 0;
    char *line = strtok(tmp, "\n");
    while (line) {
        if (output_count >= MAX_OUTPUT_LINES) {
            memmove(output[0], output[1], (MAX_OUTPUT_LINES-1)*MAX_LINE_LEN);
            output_count = MAX_OUTPUT_LINES - 1;
        }
        strncpy(output[output_count], line, MAX_LINE_LEN-1);
        output[output_count][MAX_LINE_LEN-1] = 0;
        output_count++;
        line = strtok(NULL, "\n");
    }
}

void term_printf(const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    term_print(buf);
}

void term_clear() {
    output_count = 0;
    memset(output, 0, sizeof(output));
}

// ==========================================
// COMMANDS
// ==========================================
void cmd_help(char *args) {
    term_print("help echo clear dir mkdir cd pwd");
    term_print("touch rm cat cp mv neofetch");
    term_print("RGX-install exit");
}

void cmd_echo(char *args)  { term_print(args ? args : ""); }
void cmd_clear(char *args) { term_clear(); }

void cmd_dir(char *args) {
    DIR *d = opendir(cwd);
    if (!d) { term_print("Cannot open dir"); return; }
    struct dirent *entry;
    while ((entry = readdir(d))) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", cwd, entry->d_name);
        struct stat st;
        stat(path, &st);
        if (S_ISDIR(st.st_mode)) term_printf("<DIR> %s", entry->d_name);
        else                      term_printf("      %s", entry->d_name);
    }
    closedir(d);
}

void cmd_pwd(char *args) { term_print(cwd); }

void cmd_mkdir(char *args) {
    if (!args || !args[0]) { term_print("Usage: mkdir <name>"); return; }
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", cwd, args);
    if (mkdir(path, 0755) == 0) term_printf("Created: %s", args);
    else term_printf("Error: %s exists", args);
}

void cmd_cd(char *args) {
    if (!args || !args[0]) { term_print("Usage: cd <folder>"); return; }
    if (strcmp(args, "..") == 0) {
        char *slash = strrchr(cwd, '/');
        if (slash && slash != cwd) *slash = 0;
        else strcpy(cwd, "/");
    } else {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", cwd, args);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
            strncpy(cwd, path, sizeof(cwd)-1);
        else term_printf("Not found: %s", args);
    }
}

void cmd_touch(char *args) {
    if (!args || !args[0]) { term_print("Usage: touch <file>"); return; }
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", cwd, args);
    FILE *f = fopen(path, "a");
    if (f) { fclose(f); term_printf("Created: %s", args); }
    else term_print("Error creating file");
}

void cmd_rm(char *args) {
    if (!args || !args[0]) { term_print("Usage: rm <file>"); return; }
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", cwd, args);
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            if (rmdir(path) == 0) term_printf("Removed dir: %s", args);
            else term_print("Dir not empty");
        } else {
            if (remove(path) == 0) term_printf("Removed: %s", args);
            else term_print("Error removing");
        }
    } else term_printf("Not found: %s", args);
}

void cmd_cat(char *args) {
    if (!args || !args[0]) { term_print("Usage: cat <file>"); return; }
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", cwd, args);
    FILE *f = fopen(path, "r");
    if (!f) { term_printf("Not found: %s", args); return; }
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        term_print(line);
    }
    fclose(f);
}

void cmd_cp(char *args) {
    if (!args) { term_print("Usage: cp <src> <dst>"); return; }
    char s[128], d[128];
    if (sscanf(args, "%127s %127s", s, d) != 2) { term_print("Usage: cp <src> <dst>"); return; }
    char src[512], dst[512];
    snprintf(src, sizeof(src), "%s/%s", cwd, s);
    snprintf(dst, sizeof(dst), "%s/%s", cwd, d);
    FILE *fs = fopen(src, "rb"); if (!fs) { term_printf("Not found: %s", s); return; }
    FILE *fd = fopen(dst, "wb"); if (!fd) { fclose(fs); term_print("Cannot create dst"); return; }
    char buf[512]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fs)) > 0) fwrite(buf, 1, n, fd);
    fclose(fs); fclose(fd);
    term_printf("Copied %s -> %s", s, d);
}

void cmd_mv(char *args) {
    if (!args) { term_print("Usage: mv <src> <dst>"); return; }
    char s[128], d[128];
    if (sscanf(args, "%127s %127s", s, d) != 2) { term_print("Usage: mv <src> <dst>"); return; }
    char src[512], dst[512];
    snprintf(src, sizeof(src), "%s/%s", cwd, s);
    snprintf(dst, sizeof(dst), "%s/%s", cwd, d);
    if (rename(src, dst) == 0) term_printf("Moved %s -> %s", s, d);
    else term_print("Error moving");
}

void cmd_neofetch(char *args) {
    term_print("  ____  ____ __  __      ___  ____  ");
    term_print(" |  _ \\/ ___|  \\/  |    / _ \\/ ___| ");
    term_print(" | |_) \\___ \\ |\\/| |   | | | \\___ \\ ");
    term_print(" |  _ < ___) | |  | |__| |_| |___) |");
    term_print(" |_| \\_\\____/|_|  |_|    \\___/|____/ ");
    term_print("OS: RGX-OS 3DS v3");
    term_print("Dev: xhacking");
    term_printf("Path: %s", cwd);
}

void cmd_rgx_install(char *args) {
    if (!args || strcmp(args, "base-od") != 0) { term_print("Usage: RGX-install base-od"); return; }
    char path[512];
    snprintf(path, sizeof(path), "%s/RGX", cwd);      mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/RGX/base", cwd); mkdir(path, 0755);
    term_print("RGX base installation complete!");
}

// ==========================================
// COMMAND DISPATCHER
// ==========================================
typedef struct { const char *name; void (*fn)(char*); } Command;
static Command commands[] = {
    {"help", cmd_help}, {"echo", cmd_echo}, {"clear", cmd_clear},
    {"dir",  cmd_dir},  {"mkdir",cmd_mkdir},{"cd",    cmd_cd},
    {"pwd",  cmd_pwd},  {"touch",cmd_touch},{"rm",    cmd_rm},
    {"cat",  cmd_cat},  {"cp",   cmd_cp},   {"mv",    cmd_mv},
    {"neofetch", cmd_neofetch}, {"RGX-install", cmd_rgx_install},
};
#define CMD_COUNT (sizeof(commands)/sizeof(commands[0]))

void execute(const char *input) {
    char buf[MAX_CMD_LEN];
    strncpy(buf, input, MAX_CMD_LEN-1);
    char prompt_line[MAX_CMD_LEN + 64];
    snprintf(prompt_line, sizeof(prompt_line), "> %s", input);
    term_print(prompt_line);
    char *cmd_name = strtok(buf, " ");
    if (!cmd_name) return;
    char *args = strtok(NULL, "");
    if (args) while (*args == ' ') args++;
    if (strcmp(cmd_name, "exit") == 0 || strcmp(cmd_name, "quit") == 0) {
        term_print("Goodbye!"); return;
    }
    for (int i = 0; i < (int)CMD_COUNT; i++) {
        if (strcmp(commands[i].name, cmd_name) == 0) { commands[i].fn(args); return; }
    }
    term_printf("unknown: %s", cmd_name);
}

// ==========================================
// DRAW
// ==========================================
void draw_terminal(C2D_TextBuf tbuf) {
    // Background
    C2D_DrawRectSolid(0, 0, 0, SCREEN_TOP_W, SCREEN_TOP_H, COL_DARK);
    // Title bar
    C2D_DrawRectSolid(0, 0, 0, SCREEN_TOP_W, 14, COL_PURPLE);
    C2D_Text t;
    C2D_TextBufClear(tbuf);
    C2D_TextParse(&t, tbuf, "RGX-OS v3 | 3DS Port");
    C2D_TextOptimize(&t);
    C2D_DrawText(&t, C2D_AlignLeft, 4, 2, 0.5f, 0.45f, 0.45f, COL_WHITE);

    // Output lines - ALL GREEN
    float y = 18.0f;
    for (int i = 0; i < output_count; i++) {
        if (output[i][0] == 0) { y += 11.0f; continue; }
        C2D_Text lt;
        C2D_TextBufClear(tbuf);
        C2D_TextParse(&lt, tbuf, output[i]);
        C2D_TextOptimize(&lt);
        C2D_DrawText(&lt, C2D_AlignLeft, 4, y, 0.5f, 0.42f, 0.42f, COL_GREEN);
        y += 11.0f;
    }

    // Input bar at bottom - YELLOW
    C2D_DrawRectSolid(0, 228, 0, SCREEN_TOP_W, 12, COL_INPUTBAR);
    char input_line[320];
    snprintf(input_line, sizeof(input_line), "%s> %s_", cwd, cmd_buffer);
    C2D_Text it;
    C2D_TextBufClear(tbuf);
    C2D_TextParse(&it, tbuf, input_line);
    C2D_TextOptimize(&it);
    C2D_DrawText(&it, C2D_AlignLeft, 4, 229, 0.5f, 0.40f, 0.40f, COL_YELLOW);
}

void draw_keyboard(C2D_TextBuf tbuf) {
    C2D_DrawRectSolid(0, 0, 0, SCREEN_BOT_W, SCREEN_BOT_H, COL_DARKBOT);
    C2D_DrawRectSolid(0, 0, 0, SCREEN_BOT_W, 14, COL_PURPLE);
    C2D_Text kt;
    C2D_TextBufClear(tbuf);
    C2D_TextParse(&kt, tbuf, "Keyboard - ENTER=run  <-=del");
    C2D_TextOptimize(&kt);
    C2D_DrawText(&kt, C2D_AlignLeft, 4, 2, 0.5f, 0.38f, 0.38f, COL_GREEN);

    for (int i = 0; i < (int)KEY_COUNT; i++) {
        Key *k = &keyboard[i];
        int kx = k->x, ky = k->y + KEY_OFFSET_Y, kw = k->w, kh = k->h;
        u32 bg = COL_KEY;
        if (k->value == '\n') bg = COL_ENTER;
        else if (k->value == '\b') bg = COL_BACK;
        else if (k->value == ' ' && kw > 50) bg = COL_SPACE;
        C2D_DrawRectSolid(kx, ky, 0, kw-2, kh-2, bg);
        C2D_DrawRectSolid(kx, ky, 0, kw-2, 1, COL_BORDER);
        C2D_DrawRectSolid(kx, ky, 0, 1, kh-2, COL_BORDER);
        C2D_Text key_t;
        C2D_TextBufClear(tbuf);
        C2D_TextParse(&key_t, tbuf, k->label);
        C2D_TextOptimize(&key_t);
        float sc = (kw > 40) ? 0.38f : 0.42f;
        C2D_DrawText(&key_t, C2D_AlignCenter, kx + kw/2 - 1, ky + 8, 0.5f, sc, sc, COL_WHITE);
    }
}

// ==========================================
// TOUCH
// ==========================================
void handle_touch(touchPosition touch) {
    int tx = touch.px, ty = touch.py;
    for (int i = 0; i < (int)KEY_COUNT; i++) {
        Key *k = &keyboard[i];
        int kx = k->x, ky = k->y + KEY_OFFSET_Y;
        if (tx >= kx && tx < kx+k->w && ty >= ky && ty < ky+k->h) {
            if (k->value == '\n') {
                if (cmd_len > 0) {
                    cmd_buffer[cmd_len] = 0;
                    execute(cmd_buffer);
                    cmd_len = 0;
                    memset(cmd_buffer, 0, sizeof(cmd_buffer));
                }
            } else if (k->value == '\b') {
                if (cmd_len > 0) cmd_buffer[--cmd_len] = 0;
            } else {
                if (cmd_len < MAX_CMD_LEN-1) {
                    cmd_buffer[cmd_len++] = k->value;
                    cmd_buffer[cmd_len] = 0;
                }
            }
            break;
        }
    }
}

// ==========================================
// MAIN
// ==========================================
int main() {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget *top    = C2D_CreateScreenTarget(GFX_TOP,    GFX_LEFT);
    C3D_RenderTarget *bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    C2D_TextBuf tbuf = C2D_TextBufNew(4096);

    strcpy(cwd, "sdmc:/");
    term_print("Welcome to RGX-OS 3DS!");
    term_print("Type 'help' for commands");
    term_print("");

    bool was_touching = false;

    while (aptMainLoop()) {
        hidScanInput();
        u32 held = hidKeysHeld();
        u32 down = hidKeysDown();
        if (down & KEY_START) break;

        touchPosition touch;
        hidTouchRead(&touch);
        bool touching = (held & KEY_TOUCH) != 0;
        if (touching && !was_touching) handle_touch(touch);
        was_touching = touching;

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(top,    COL_DARK);
        C2D_SceneBegin(top);
        draw_terminal(tbuf);
        C2D_TargetClear(bottom, COL_DARKBOT);
        C2D_SceneBegin(bottom);
        draw_keyboard(tbuf);
        C3D_FrameEnd(0);
    }

    C2D_TextBufDelete(tbuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
