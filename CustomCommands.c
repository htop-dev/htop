#include "CustomCommands.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <curses.h>

static void show_temp_message(const char* title, const char* msg) {
    clear();
    mvprintw(0, 0, "=== %s ===", title);
    mvprintw(2, 0, "%s", msg);
    mvprintw(LINES-2, 0, "Press any key to return to htop");
    refresh();
    getch();
    clear();
    refresh();
}

static void cmd_ls(ScreenManager* scr) {
    show_temp_message("ls - All Processes", 
                      "Process listing feature\n\n"
                      "To implement: iterate through ProcessList\n"
                      "and display PID, MEM%, Command");
}

static void cmd_pkg_ls(ScreenManager* scr) {
    show_temp_message("pkg-ls - Memory Hog Package", 
                      "Package finding feature\n\n"
                      "To implement: find highest memory process\n"
                      "and query dpkg/rpm for package name");
}

static void cmd_max_ls(ScreenManager* scr) {
    show_temp_message("max-ls - Top Memory Processes", 
                      "Top memory processes feature\n\n"
                      "To implement: sort by memory and show top N");
}

bool handle_custom_command(const char* input, ScreenManager* scr) {
    if (strcmp(input, "ls") == 0) {
        cmd_ls(scr);
        return true;
    } else if (strcmp(input, "pkg-ls") == 0) {
        cmd_pkg_ls(scr);
        return true;
    } else if (strcmp(input, "max-ls") == 0) {
        cmd_max_ls(scr);
        return true;
    }
    return false;
}
