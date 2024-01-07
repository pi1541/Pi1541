#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <cstring>
#include <math.h>
#include <vector>

#include "fake-posix.h"

#define NO_THREADS 4       // max 16 for Orangecart!
#define STACK_SIZE 1024
#define PIXELW 2 // 2
#define MAX_ITER 256
#ifdef SKVIC20
#define IMG_W 192 // 320
#define IMG_H 160 // 200
#else
#define IMG_W 1280 // 320
#define IMG_H 512 // 200
#endif
#define MTYPE double

#define CSIZE (IMG_W * IMG_H) / 8
#define PAL_SIZE 256 // (2 * PIXELW) // 256 for RPi Console

// set this to enable direct output on C64 gfx mem.
//#define C64

#ifdef C64
#define NO_LOG
#endif

//#define NO_LOG
#ifdef NO_LOG
#define log_msg(...) 
#else
#define log_msg printf
#endif

#ifdef C64
#include "c64-lib.h"
#else
static char cv[CSIZE] = {};
char *mandel_canvas = cv;
#endif

#ifdef __ZEPHYR__
#if (NO_THREADS > 16)
#error "too many threads for Orangencart's STACK_SIZE"
#endif
static char *stacks = (char *)0x10000000;   // fast SRAM on Orangecart, only 16k! so NO_THREADS <= 16

#else

#undef STACK_SIZE 
#define STACK_SIZE PTHREAD_STACK_MIN
static char *stacks;

#endif

#include "mandelbrot.h"
MTYPE xrat = 1.0;

typedef struct { point_t lu; point_t rd; } rec_t;
std::vector<rec_t> recs = { 
        {{00, 00},{80,100}}, 
        {{80, 100},{159,199}}, 
        {{00, 50},{40,100}},        
        {{80, 110}, {120, 160}},
        {{60,75}, {100, 125}},
        {{60,110}, {100, 160}},
        {{60,85}, {100, 135}},
        {{60,85}, {100, 135}},
        {{40,60}, {80, 110}},
        {{120,85}, {159, 135}},
};

int mandel_zoom(mandel<MTYPE> *m, size_t i)
{
#ifdef SKVIC20
    MTYPE scalex = 192/320.0;
    MTYPE scaley = 160/200.0;
#else
    MTYPE scalex = 1.0;
    MTYPE scaley = 1.0;
#endif
    point_t lu, rd;
    auto r = &recs[i];
     //log_msg("Zooming into [%d,%d]x[%d,%d]...stacks=%p\n", it->lu.x, it->lu.y, it->rd.x, it->rd.y, m->get_stacks());
    lu.x = r->lu.x * scalex;
    lu.y = r->lu.y * scaley;
    rd.x = r->rd.x * scalex;
    rd.y = r->rd.y * scaley;
    m->select_start(lu);
    m->select_end(rd);
    return 0;
}

mandel<MTYPE> *tmandel = nullptr;
int mandel_iterate(int usec)
{
    static size_t it = 0;
    static char *sts = new char[STACK_SIZE * NO_THREADS]();
    if (tmandel == nullptr)
    {
        tmandel = new mandel<MTYPE>{cv, sts, -1.5, -1.0, 0.5, 1.0, IMG_W / PIXELW, IMG_H, xrat, 50};
        return 1;
    }
//    usleep(usec);
    if (it < recs.size())
    {
        mandel_zoom(tmandel, it);
        it++;
        return 1;
    }
    else
    {
        it = 0;
        delete tmandel;
        tmandel = new mandel<MTYPE>{cv, sts, -1.5, -1.0, 0.5, 1.0, IMG_W / PIXELW, IMG_H, xrat, 50};
        return 0;
    }
}

int mandel_driver(void)
{
#ifndef __ZEPHYR__
    stacks = new char[STACK_SIZE * NO_THREADS]();
#endif
#ifdef C64
    c64 c64;
    std::cout << "C64 memory @0x" << std::hex << int(c64.get_mem()) << std::dec << '\n';
    char *cv = (char *)&c64.get_mem()[0x4000];
    c64.screencols(VIC::BLACK, VIC::BLACK);
    c64.gfx(VICBank1, VICModeGfxMC, 15);
    //xrat = 16.0 / 9.0;
#endif
    int col1, col2, col3;
    col1 = 0xb;
    col2 = 0xc;
    col3 = 14; // VIC::LIGHT_BLUE;
    log_msg("%s - 1\n", __FUNCTION__);

    for (int i = 0; i < 1; i++) 
    {
#ifdef C64
            memset(&cv[0x3c00], (col1<<4)|col2, 1000);
            memset(&c64.get_mem()[0xd800], col3, 1000);
#endif 
        mandel<MTYPE> *m = new mandel<MTYPE>{cv, stacks, -1.5, -1.0, 0.5, 1.0, IMG_W / PIXELW, IMG_H, xrat, 50};
        for (size_t iz = 0; iz < recs.size(); iz++)
        {
            mandel_zoom(m, iz);
            sleep(1);
        }
        col1++; col2++; col3++;
        col1 %= 0xf; if (col1 == 0) col1++;
        col2 %= 0xf; if (col2 == 0) col2++;
        col3 %= 0xf; if (col3 == 0) col3++;
        delete m;
    }
#ifdef __ZEPHYR__
    while(1)
    {
        std::cout << "system halted.\n";
        sleep(10);
    }
#else
    delete[] stacks;
#endif
    usleep(2 * 1000 * 1000);
    return 0;
}
