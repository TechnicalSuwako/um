#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#define MAX_ITEMS 512
#define MAX_NAME_LEN 256

typedef struct {
  char name[MAX_NAME_LEN];
  char keys[MAX_NAME_LEN];
  char exec[MAX_NAME_LEN];
  int x, y, width, height;
} Program;

const char *sofname = "um";
const char *version = "0.1.0";

Program programs[MAX_ITEMS];
int programcount = 0;
int topidx = 0;

int window_width = 300;
int window_height = 240;
int item_height = 20;
int display_items = 10;

XftColor color, selcolor;
Colormap colormap;
XftDraw *draw;
XftFont *font;

void parse_desktop_file(const char *filepath) {
  FILE *file = fopen(filepath, "r");
  if (!file) return;

  char line[512];
  Program program = {0};

  while (fgets(line, sizeof(line), file)) {
    if (
      strncmp(line, "Name=", 5) == 0 ||
      strncmp(line, "Name[", 5) == 0
    ) {
      if (strncmp(line, "Name[", 5) == 0) {
        char *locale = line + 5;
        char *end = strchr(locale, ']');
        if (end) {
          *end = '\0';
          if (strncmp(locale, getenv("LANG"), 2) == 0) {
            strncpy(program.name, end + 2, MAX_NAME_LEN - 1);
            program.name[strcspn(program.name, "\n")] = '\0';
          }
        }
      } else {
        strncpy(program.name, line + 5, MAX_NAME_LEN - 1);
        program.name[strcspn(program.name, "\n")] = '\0';
      }
    } else if (strncmp(line, "Keywords=", 9) == 0) {
      strncpy(program.keys, line + 9, MAX_NAME_LEN - 1);
      program.keys[strcspn(program.keys, "\n")] = '\0';
    } else if (strncmp(line, "Exec=", 5) == 0) {
      strncpy(program.exec, line + 5, MAX_NAME_LEN - 1);
      program.exec[strcspn(program.exec, "\n")] = '\0';
      char *p = program.exec;
      while ((p = strpbrk(p, "%")) != NULL) {
        *p = '\0';
        strncat(program.exec, p + 2, strlen(p + 2));
      }
    }
  }

  fclose(file);

  if (program.name[0] && program.exec[0]) {
    programs[programcount++] = program;
  }
}

void scan_desktop_files(const char *directory) {
  DIR *dir = opendir(directory);
  if (!dir) return;

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcasestr(entry->d_name, ".desktop")) {
      char filepath[512];
      snprintf(filepath, sizeof(filepath), "%s/%s", directory, entry->d_name);
      parse_desktop_file(filepath);
    }
  }

  closedir(dir);
}

void fetch_programs() {
  char *xdg_data_home = getenv("XDG_DATA_HOME");
  if (!xdg_data_home) {
    xdg_data_home = getenv("HOME");
    if (xdg_data_home) {
      char path[MAX_NAME_LEN];
      snprintf(path, sizeof(path), "%s/.local/share/applications", xdg_data_home);
      scan_desktop_files(path);
    }
  } else {
    char path[MAX_NAME_LEN];
    snprintf(path, sizeof(path), "%s/applications", xdg_data_home);
    scan_desktop_files(path);
  }

  scan_desktop_files("/usr/share/applications");
  scan_desktop_files("/usr/local/share/applications");
#if defined(__NetBSD__)
  scan_desktop_files("/usr/pkg/share/applications");
#endif
}

void drawtext(
  Display *display,
  Window window,
  GC gc,
  int x,
  int y,
  const char *text,
  int sel
) {
  if (sel) {
    XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
    XFillRectangle(
      display,
      window,
      gc,
      0,
      y - item_height + 5,
      window_width,
      item_height
    );
    XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
  } else {
    XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
  }

  XftDrawStringUtf8(
    draw,
    &color,
    font,
    x,
    y,
    (const FcChar8 *)text,
    strlen(text)
  );
}

void filterdisplay(
  Display *display,
  Window window,
  GC gc,
  const char *input,
  int sel
) {
  XClearWindow(display, window);

  // 検索ボックス
  drawtext(display, window, gc, 10, item_height, input, 0);

  int y = item_height * 2;
  int idx = 0;

  for (int i = 0; i < programcount; i++) {
    if (strcasestr(programs[i].name, input) || strcasestr(programs[i].keys, input)) {
      if (idx >= topidx && idx < topidx + display_items) {
        drawtext(display, window, gc, 10, y, programs[i].name, idx == sel);
        y += item_height;
      }
      idx++;
    }
  }
}

int filtercount(const char *input) {
  int count = 0;

  for (int i = 0; i < programcount; i++) {
    if (strcasestr(programs[i].name, input) || strcasestr(programs[i].keys, input))
      count++;
  }

  return count;
}

void launch_program(const char *exec) {
  if (fork() == 0) {
    setsid();
    execl("/bin/sh", "sh", "-c", exec, NULL);
    exit(0);
  }
}

void calculate_dimensions(int win_width, int win_height) {
  window_width = win_width;
  window_height = win_height;
  item_height = win_height / (display_items + 2);
}

int main() {
  Display *display;
  Window window;
  XEvent event;
  int screen;
  GC gc;
  XGCValues values;
  char input[MAX_NAME_LEN] = {0};
  int sel = 0;

  fetch_programs();

  display = XOpenDisplay(NULL);
  if (display == NULL) {
    fprintf(stderr, "画面を開けられません。\n");
    exit(1);
  }

  screen = DefaultScreen(display);

  int screen_width = DisplayWidth(display, screen);
  int screen_height = DisplayHeight(display, screen);
  window_width = screen_width / 4;
  window_height = screen_height / 4;
  int window_x = (screen_width - window_width) / 2;
  int window_y = (screen_height - window_height) / 2;

  window = XCreateSimpleWindow(
    display,
    RootWindow(display, screen),
    window_x, window_y, window_width, window_height,
    1,
    BlackPixel(display, screen),
    BlackPixel(display, screen)
  );

  XSetWindowBackground(display, window, 0x551A8B); // RGB (85, 26, 139)

  XSelectInput(
    display,
    window,
    ExposureMask | KeyPressMask | StructureNotifyMask | ButtonPressMask
  );

  gc = XCreateGC(display, window, 0, &values);
  if (!gc) {
    fprintf(stderr, "GCを作れません。\n");
    exit(1);
  }

  Visual *visual = DefaultVisual(display, screen);

  colormap = XCreateColormap(display, window, visual, AllocNone);
  if (colormap == None) {
    XFreeGC(display, gc);
    fprintf(stderr, "カラーマップを作れません。\n");
    exit(1);
  }

  draw = XftDrawCreate(display, window, visual, colormap);
  if (!draw) {
    XFreeGC(display, gc);
    XFreeColormap(display, colormap);
    fprintf(stderr, "ドローを作れません。\n");
    exit(1);
  }

  font = XftFontOpenName(display, screen, "Noto Sans CJK-12");
  if (!font) {
    XFreeGC(display, gc);
    XFreeColormap(display, colormap);
    XftDrawDestroy(draw);
    fprintf(stderr, "フォントの読み込みに失敗。\n");
    exit(1);
  }

  if (!XftColorAllocName(display, visual, colormap, "white", &color)) {
    XFreeGC(display, gc);
    XFreeColormap(display, colormap);
    XftDrawDestroy(draw);
    XftFontClose(display, font);
    fprintf(stderr, "色の役割に失敗。\n");
  }

  if (!XftColorAllocName(display, visual, colormap, "black", &selcolor)) {
    XFreeGC(display, gc);
    XFreeColormap(display, colormap);
    XftDrawDestroy(draw);
    XftFontClose(display, font);
    XftColorFree(display, visual, colormap, &color);
    fprintf(stderr, "選択色の役割に失敗。\n");
  }

  XMapWindow(display, window);

  while (1) {
    XNextEvent(display, &event);

    if (event.type == Expose || event.type == ConfigureNotify) {
      if (event.type == ConfigureNotify) {
        calculate_dimensions(event.xconfigure.width, event.xconfigure.height);
      }
      filterdisplay(display, window, gc, input, sel);
    } else if (event.type == KeyPress) {
      char buf[32];
      KeySym keysym;
      int len = XLookupString(&event.xkey, buf, sizeof(buf) - 1, &keysym, NULL);

      if (keysym == XK_Up) {
        if (sel > 0) {
          sel--;
          if (sel < topidx) topidx--;
        }
      } else if (keysym == XK_Down) {
        int filteredcount = filtercount(input);
        if (sel < filteredcount - 1) {
          sel++;
          if (sel >= topidx + display_items) topidx++;
        }
      } else if (keysym == XK_BackSpace && strlen(input) > 0) {
        input[strlen(input) - 1] = '\0';
        sel = 0;
        topidx = 0;
      } else if (keysym == XK_Return) {
        int visible_index = 0;
        for (int i = 0; i < programcount; i++) {
          if (strcasestr(programs[i].name, input)) {
            if (visible_index == sel) {
              launch_program(programs[i].exec);
              break;
            }
            visible_index++;
          }
        }
        break;
      } else if (keysym == XK_Escape) {
        break;
      } else if (len > 0 && len < MAX_NAME_LEN - 1) {
        strncat(input, buf, len);
        sel = 0;
        topidx = 0;
      }

      filterdisplay(display, window, gc, input, sel);
    } else if (event.type == ButtonPress) {
      int x = event.xbutton.x;
      int y = event.xbutton.y;

      for (int i = 0; i < programcount; i++) {
        if (x >= programs[i].x && x <= programs[i].x + programs[i].width &&
            y >= programs[i].y - item_height && y <= programs[i].y) {
          launch_program(programs[i].exec);
          break;
        }
      }
    }
  }

  XftColorFree(display, visual, colormap, &color);
  XftColorFree(display, visual, colormap, &selcolor);
  if (font) XftFontClose(display, font);
  if (draw) XftDrawDestroy(draw);
  XftColorFree(display, DefaultVisual(display, screen), colormap, &color);
  if (gc) XFreeGC(display, gc);
  if (window) XDestroyWindow(display, window);
  if (display) XCloseDisplay(display);

  return 0;
}
