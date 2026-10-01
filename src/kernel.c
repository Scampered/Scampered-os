#include <stdint.h>
#include "string.h"
#include "disk.h"
#include "fat12.h"


void kernel_main(uint32_t mb_info);
void kernel_main(uint32_t mb_info) __attribute__((section(".text")));


/* ===== VGA constants ===== */
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define MAX_FILES 16
#define MAX_FILENAME 16
#define MAX_FILESIZE 512

/* ===== VGA memory ===== */
static uint16_t* vga = (uint16_t*)0xB8000;

/* ===== Cursor ===== */
static int cursor_x=0, cursor_y=1;

/* ===== File system ===== */
typedef struct {
    char name[MAX_FILENAME];
    char content[MAX_FILESIZE];
    int size;
    int used;
} file_t;

file_t files[MAX_FILES]={0};

/* ===== Keyboard I/O ===== */
static unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile("inb %1,%0":"=a"(ret):"Nd"(port));
    return ret;
}

static int kbd_available() { return inb(0x64)&1; }

static unsigned char kbd_read() {
    while(!kbd_available()) __asm__("pause");
    return inb(0x60);
}

/* ===== VGA functions ===== */
static void cls() {
    for(int y=0;y<VGA_HEIGHT;y++)
        for(int x=0;x<VGA_WIDTH;x++)
            vga[y*VGA_WIDTH+x]=' '|0x07<<8;
    cursor_x=0; cursor_y=1;
}

static void putchar_at(char c,int x,int y) { vga[y*VGA_WIDTH+x]=c|0x07<<8; }

static void print(const char* s) {
    while(*s){
        if(*s=='\n'){cursor_x=0;cursor_y++;s++;continue;}
        putchar_at(*s,cursor_x++,cursor_y);
        if(cursor_x>=VGA_WIDTH){cursor_x=0;cursor_y++;}
        if(cursor_y>=VGA_HEIGHT) cursor_y=1;
        s++;
    }
}

/* ===== File operations ===== */
static file_t* find_file(const char* name){
    for(int i=0;i<MAX_FILES;i++) if(files[i].used && strcmp(files[i].name,name)==0) return &files[i];
    return 0;
}

static file_t* create_file(const char* name){
    for(int i=0;i<MAX_FILES;i++){
        if(!files[i].used){
            files[i].used=1;
            for(int j=0;j<MAX_FILENAME;j++) files[i].name[j]=0;
            int k=0; while(name[k] && k<MAX_FILENAME-1){files[i].name[k]=name[k];k++;}
            files[i].size=0;
            for(int m=0;m<MAX_FILESIZE;m++) files[i].content[m]=0;
            return &files[i];
        }
    }
    return 0;
}

static void delete_file(const char* name){
    file_t* f=find_file(name);
    if(f) f->used=0;
}

static void append_to_file(file_t* f, const char* data){
    if(!f) return;
    int dlen = strlen(data);
    if(f->size >= MAX_FILESIZE-1) return;
    int copy = dlen;
    if(f->size + copy > MAX_FILESIZE-1) copy = MAX_FILESIZE-1 - f->size;
    for(int i=0;i<copy;i++) f->content[f->size + i] = data[i];
    f->size += copy;
    f->content[f->size] = 0;
}

/* ===== Scancode to ASCII (full symbols) ===== */
static char scancode_to_char(unsigned char sc){
    static const char map[128] = {
        0, 27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',  // 0-14
        '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0, // 15-30
        'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',       // 31-45
        'z','x','c','v','b','n','m',',','.','/',0,0,0,' ',             // 46-59
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 // rest
    };
    if(sc>=128) return 0;
    return map[sc];
}

/* ===== Taskbar ===== */
typedef enum {TAB_DESKTOP=0,TAB_TERMINAL=1,TAB_SHUTDOWN=2} tab_t;
tab_t current_tab=TAB_DESKTOP;
int selected_tab=0;

static void draw_taskbar(){
    const char* tabs[3]={" Desktop "," Terminal "," Shutdown "};
    /* clear top row */
    for(int x=0;x<VGA_WIDTH;x++) putchar_at(' ',x,0);
    int pos=0;
    for(int i=0;i<3;i++){
        for(int j=0;j<strlen(tabs[i]);j++){
            char c=tabs[i][j];
            if(i==selected_tab) vga[0*VGA_WIDTH + pos]=c|0x70<<8; // highlighted on top row
            else vga[0*VGA_WIDTH + pos]=c|0x07<<8;
            pos++;
        }
    }
}

/* ============================
   DESKTOP DISPLAY
   ============================ */

static int desktop_selected = 0;

static void show_desktop() {
    cls();
    draw_taskbar();

    int y = 2;                   // Start drawing at row 2
    int shown_count = 0;
    int any_files = 0;

    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used) {
            any_files = 1;

            // highlight row background if selected
            if (shown_count == desktop_selected) {
                for (int x = 0; x < VGA_WIDTH; x++)
                    vga[y * VGA_WIDTH + x] = ' ' | (0x70 << 8);
            }

            // print filename
            const char *name = files[i].name;
            for (int j = 0; j < strlen(name) && j < VGA_WIDTH; j++) {
                uint16_t color = (shown_count == desktop_selected)
                                 ? (0x70 << 8)
                                 : (0x0F << 8);
                vga[y * VGA_WIDTH + j] = name[j] | color;
            }

            y++;
            shown_count++;
        }
    }

    if (!any_files) {
        print("Empty!\n");
    }
}


/* ============================
   READ FILE (ENTER)
   ============================ */

static void readfile_desktop() {
    int selected = desktop_selected;
    int index = 0;

    // Locate selected file
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used) {

            if (index == selected) {

                cls();
                draw_taskbar();
                cursor_x = 0;
                cursor_y = 2;

                /* ===== Safe + stable file printing ===== */
                int pos = 0;
                while (pos < files[i].size) {
                    char buf[81];
                    int k = 0;

                    while (k < 80 && pos < files[i].size)
                        buf[k++] = files[i].content[pos++];

                    buf[k] = 0;

                    print(buf);
                    print("\n");
                }

                print("\n-- Press ESC to return --\n");

                /* ===== Wait for ESC ===== */
                while (1) {
                    unsigned char sc = kbd_read();
                    if (sc == 0x01) break;   // ESC key
                }

                show_desktop();
                return;
            }

            index++;
        }
    }
}

/* ===== Boot splash (Option A) ===== */
static void show_boot_splash() {
    cls();

    const char *s = "SCAMPERED";
    int len = strlen(s);

    int center_x = (VGA_WIDTH - len) / 2;
    int center_y = VGA_HEIGHT / 2;

    for (int i = 0; i < len; i++) {
        putchar_at(s[i], center_x + i, center_y);
    }

    /* small delay so it feels like a boot screen */
    for (volatile int i = 0; i < 30000000; i++); // tweak if needed

    /* clear splash; next UI will draw taskbar/desktop */
    cls();
}

/* ===== Terminal ===== */
static void terminal(){
    cls();
    draw_taskbar();
    cursor_x=0; cursor_y=1;
    print("> ");

    char input[512];
    int pos=0;

    while(1){
        unsigned char sc=kbd_read();

        /* Priority handling for taskbar keys (Left/Right/Enter) */
        if(sc==0x4B){ /* Left */
            if(selected_tab>0) selected_tab--;
            draw_taskbar();
            /* continue terminal session unless Enter pressed */
            continue;
        }
        if(sc==0x4D){ /* Right */
            if(selected_tab<2) selected_tab++;
            draw_taskbar();
            continue;
        }
        if(sc==0x1C){ /* Enter key pressed as a taskbar action if a different tab is selected */
            if(selected_tab != TAB_TERMINAL){
                /* switch away immediately */
                if(selected_tab==TAB_DESKTOP){
                    show_desktop();
                    return;
                }else if(selected_tab==TAB_SHUTDOWN){
                    cls(); print("Shutting down...\n");
                    for(;;) __asm__("cli; hlt");
                }
            }
            /* otherwise fall through to terminal-enter handling */
        }

        /* Normal character handling (translate scancode) */
        char c=scancode_to_char(sc);
        if(c==0) continue;

        if(c==8){ // backspace
            if(pos>0){
                pos--;
                if(cursor_x>0) cursor_x--;
                putchar_at(' ',cursor_x,cursor_y);
            }
            continue;
        }
        if(c=='\n'){ // Enter inside terminal input
            input[pos]=0;
            print("\n");

            /* Parse commands */
            if(strncmp(input,"write ",6)==0){
                char* fname=input+6;
                /* trim spaces: simple approach */
                while(*fname==' ') fname++;
                if(*fname==0){ print("Specify filename.\n"); }
                else{
                    file_t* f = find_file(fname);
                    if(!f) f = create_file(fname);
                    if(!f){ print("No space for file!\n"); }
                    else{
                        /* append-mode: collect lines until ".stop" */
                        print("Enter lines. Type .stop alone on a line to finish.\n");
                        char linebuf[256];
                        while(1){
                            cursor_x=0; cursor_y++; /* move to next line prompt */
                            if(cursor_y>=VGA_HEIGHT) cursor_y=VGA_HEIGHT-1;
                            print("> ");
                            int li=0;
                            while(1){
                                unsigned char sc2 = kbd_read();
                                /* taskbar keys still get priority even while writing */
                                if(sc2==0x4B){ if(selected_tab>0){ selected_tab--; draw_taskbar(); } continue; }
                                if(sc2==0x4D){ if(selected_tab<2){ selected_tab++; draw_taskbar(); } continue; }
                                /* Normal char processing for the line */
                                char cc = scancode_to_char(sc2);
                                if(cc==0) continue;
                                if(cc==8){ if(li>0){ li--; if(cursor_x>0) cursor_x--; putchar_at(' ',cursor_x,cursor_y);} continue; }
                                if(cc=='\n'){ linebuf[li]=0; break; }
                                if(li < (int)sizeof(linebuf)-1){
                                    linebuf[li++] = cc;
                                    putchar_at(cc,cursor_x++,cursor_y);
                                }
                            }
                            /* line read in linebuf */
                            if(strcmp(linebuf, ".stop")==0) {
                                print("\nFinished writing.\n");
                                break;
                            } else {
                                /* append line plus newline to file (safely) */
                                int space_left = MAX_FILESIZE - 1 - f->size;
                                if(space_left <= 0) { print("\nFile full. Stopped.\n"); break; }
                                /* append */
                                int added = 0;
                                for(int i=0; linebuf[i] && added < space_left; i++){
                                    f->content[f->size++] = linebuf[i];
                                    added++;
                                }
                                if(added < space_left){
                                    f->content[f->size++] = '\n';
                                    added++;
                                }
                                f->content[f->size] = 0;
                            }
                        } /* end collect lines */
                        print("File saved!\n");
                    }
                }
            }else if(strncmp(input,"read ",5)==0){
                char* fname=input+5;
                while(*fname==' ') fname++;
                file_t* f=find_file(fname);
                if(f) {
                    print("\n");
                    int pos_content = 0;
                    while(pos_content < f->size){
                        char buf[81];
                        int k=0;
                        while(k<80 && pos_content < f->size){
                            buf[k++] = f->content[pos_content++];
                        }
                        buf[k]=0;
                        print(buf);
                        print("\n");
                    }
                    print("\n-- End of file --\n");
                }
                else print("File not found!\n");
            }else if(strncmp(input,"delete ",7)==0){
                char* fname=input+7;
                while(*fname==' ') fname++;
                delete_file(fname);
                print("File deleted if existed.\n");
            }else if(strcmp(input,"ls")==0){
                int has=0;
                for(int i=0;i<MAX_FILES;i++){
                    if(files[i].used){ print(files[i].name); print("\n"); has=1;}
                }
                if(!has) print("Empty!\n");
            }else if(strcmp(input,"shutdown")==0){
                print("Shutting down...\n");
                for(;;) __asm__("cli; hlt");
            }else{
                print("Unknown command.\n");
            }

            /* reset input line */
            pos=0;
            cursor_x=0;
            print("> ");
        }else{
            if(pos<512-1){
                input[pos++]=c;
                putchar_at(c,cursor_x++,cursor_y);
            }
        }
    }
}


/* ===== Main OS loop ===== */
#include "multiboot.h"
void kernel_main(uint32_t mb_info) {
/*========== 1. Read GRUB module via disk_init() ==========*/
    disk_init(mb_info);

    if (fat12_init() != 0)
        print("FAT12 init failed!\n");
    else
        print("FAT12 initialized from GRUB module!\n");

    /*========== 2. UI ==========*/
    show_boot_splash();
    draw_taskbar();
    show_desktop();

    /*========== 3. MAIN OS LOOP (unchanged) ==========*/
    while (1) {

        unsigned char sc = kbd_read();
        if (!sc) continue;

        // Left arrow
        if (sc == 0x4B) {
            if (selected_tab > 0) selected_tab--;
            draw_taskbar();
            continue;
        }

        // Right arrow
        if (sc == 0x4D) {
            if (selected_tab < 2) selected_tab++;
            draw_taskbar();
            continue;
        }

        // Enter key
        if (sc == 0x1C) {
            if (selected_tab == TAB_DESKTOP) {
                show_desktop();
                readfile_desktop();
            }
            else if (selected_tab == TAB_TERMINAL) {
                terminal();
                cls();
                draw_taskbar();
                show_desktop();
            }
            else if (selected_tab == TAB_SHUTDOWN) {
                cls(); print("Shutting down...\n");
                for(;;) __asm__("cli; hlt");
            }
            continue;
        }

        /* Desktop navigation */
        if (selected_tab == TAB_DESKTOP) {

            int file_count = 0;
            for (int i = 0; i < MAX_FILES; i++)
                if (files[i].used) file_count++;

            if (file_count > 0) {
                if (sc == 0x48 && desktop_selected > 0) {
                    desktop_selected--;
                    show_desktop();
                }
                if (sc == 0x50 && desktop_selected < file_count - 1) {
                    desktop_selected++;
                    show_desktop();
                }
            }
        }
    }
}
