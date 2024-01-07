/* -*-c++-*-
 * This file is part of pottendos-playground.
 * 
 * FE playground is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * FE playground is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with FE playground.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef __MANDELBROT_H__
#define __MANDELBROT_H__

#include <stdlib.h>
#include <unistd.h>
#include <complex>
#include <iostream>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include <time.h>

typedef struct { uint16_t x; uint16_t y; } point_t;

template <typename myDOUBLE>
class mandel
{
    typedef char* canvas_t;
    typedef uint16_t coord_t;
    typedef uint16_t color_t;
    struct tparam_t
    {
        int tno, width, height;
        myDOUBLE xl, yl, xh, yh, incx, incy;
        int xoffset, yoffset;
        pthread_mutex_t go;
        sem_t &sem;
        mandel<myDOUBLE> *mo;

        tparam_t(int t, int w, int h, myDOUBLE x1, myDOUBLE y1, myDOUBLE x2, myDOUBLE y2, myDOUBLE ix, myDOUBLE iy,
                 int xo, int yo, sem_t &se, mandel<myDOUBLE> *th)
            : tno(t), width(w), height(h), xl(x1), yl(y1), xh(x2), yh(y2), incx(ix), incy(iy), xoffset(xo), yoffset(yo), sem(se), mo(th)
        {
            pthread_mutex_init(&go, nullptr);
            //log_msg("%s: helper mutex initialized: %p\n", __FUNCTION__, &go);
            P(go); // lock it
            std::cout << *this << '\n';
        }
        ~tparam_t()
        {
            //log_msg("%s: cleaning up params\n", __FUNCTION__);
            //pthread_mutex_destroy(&go);
        }
        friend std::ostream &operator<<(std::ostream &ostr, tparam_t &t)
        {
            ostr << "t=" << t.tno << ", w=" << t.width << ",h=" << t.height
                 << ",xl=" << t.xl << ",yl=" << t.yl << ",xh=" << t.xh << ",yh=" << t.yh
                 << ",incx=" << t.incx << ",incy=" << t.incy
                 << ",xo=" << t.xoffset << ",yo=" << t.yoffset << ", go: " << &t.go;
            return ostr;
        }
    };

    tparam_t *tp[NO_THREADS];
    //char *stacks[NO_THREADS];
    int max_iter = MAX_ITER;

    /* class local variables */
    pthread_attr_t attr[NO_THREADS];
    canvas_t canvas;
    char *stacks;
    uint16_t xres, yres;
    color_t col_pal[PAL_SIZE] = { 0, 1, 2, 3 };
    coord_t mark_x1, mark_y1, mark_x2, mark_y2;
    myDOUBLE last_xr, last_yr, ssw, ssh, transx, transy, xratio;
    int coffset;
    uint32_t pixel_mask;
    struct timespec tstart, tend;

    pthread_mutex_t canvas_sem;
    sem_t master_sem;
    pthread_t worker_tasks[NO_THREADS];

    static void P(pthread_mutex_t &m)
    {
        pthread_mutex_lock(&m);   
    }
    static void V(pthread_mutex_t &m)
    {
        pthread_mutex_unlock(&m);
    }
    static void PSem(sem_t &s)
    {
        sem_wait(&s);
    }
    static void VSem(sem_t &s)
    {
        sem_post(&s);
    }

    void canvas_setpx(canvas_t &canvas, coord_t x, coord_t y, color_t c)
    {
        uint32_t h = x % (8 / PIXELW);
        uint32_t shift = (8 / PIXELW - 1) - h;
        uint32_t val = (c << (shift * PIXELW));

        const uint32_t lineb = IMG_W / 8 * 8; // line 8 bytes per 8x8 pixel
        const uint32_t colb = 8;              // 8 bytes per 8x8 pixel

        // log_msg("x/y %d/%d offs %d/%d\n", x, y, (x / (8 / PIXELW)) * colb, (y / 8) * lineb  + (y % 8));
        uint32_t cidx = (y / 8) * lineb + (y % 8) + (x / (8 / PIXELW)) * colb;
        if (cidx >= CSIZE)
        {
            // log_msg("Exceeding canvas!! %d, %d/%d\n", cidx, x, y);
            // delay (100 * 1000);
            return;
        }
        char t = canvas[cidx];
        t &= ~pixel_mask;
        t |= val;
        canvas[cidx] = t;
#ifdef __ZEPHYR__
        volatile char *led= (char *)0xf0002000;
        *led = ((*led) + 1);
#endif
#ifdef __CIRCLE__ 
extern void RPiConsole_put_pixel(uint32_t x, uint32_t y, uint16_t);
        RPiConsole_put_pixel(x, y, c);
        sched_yield();
#endif
    }

    void dump_bits(uint8_t c)
    {
        for (int i = 7; i >= 0; i--)
        {
            log_msg("%c", (c & (1 << i)) ? '*' : '.');
        }
    }

    void canvas_dump(canvas_t c)
    {
        for (int y = 0; y < IMG_H; y++)
        {
            log_msg("%02d: ", y);
            int offs = (y / 8) * IMG_W + (y % 8);
            for (auto i = 0; i < IMG_W/2 ; i += 8)
            {
                // log_msg("idx: %032d ", offs + i);
                dump_bits(c[offs + i]);
            }
            log_msg("\n");
        }
    }

    /* class private functinos */
    inline void timespec_diff(struct timespec *a, struct timespec *b, struct timespec *result)
    {
    	result->tv_sec  = a->tv_sec  - b->tv_sec;
    	result->tv_nsec = a->tv_nsec - b->tv_nsec;
    	if (result->tv_nsec < 0)
    	{
	        --result->tv_sec;
	        result->tv_nsec += 1000000000L;
	    }
    }

    // abs() would calc sqr() as well, we don't need that for this fractal
    inline myDOUBLE abs2(std::complex<myDOUBLE> f)
    {
        myDOUBLE r = f.real(), i = f.imag();
        return r * r + i * i;
    }

    int mandel_calc_point(myDOUBLE x, myDOUBLE y)
    {
        const std::complex<myDOUBLE> point{x, y};
        // std::cout << "calc: " << point << '\n';
        std::complex<myDOUBLE> z = point;
        int nb_iter = 1;
        while (abs2(z) < 4 && nb_iter <= max_iter)
        {
            z = z * z + point;
            nb_iter++;
        }
        if (nb_iter < max_iter)
            return (col_pal[(nb_iter % (PAL_SIZE - 1)) + 1]);
        else
            return 0;
    }

    void mandel_helper(myDOUBLE xl, myDOUBLE yl, myDOUBLE xh, myDOUBLE yh, myDOUBLE incx, myDOUBLE incy, int xo, int yo, int width, int height)
    {
        myDOUBLE x, y;
        int xk = xo;
        int yk = yo;
        if ((xl == xh) || (yl == yh))
        {
            log_msg("assertion failed: ");
            std::cout << "xl=" << xl << ",xh=" << xh << ", yl=" << yl << ",yh=" << yh << '\n';
            return;
        }
        x = xl;
        for (xk = 0; xk < width; xk++)
        {
            y = yl;
            for (yk = 0; yk < height; yk++)
            {
                int d = mandel_calc_point(x, y);
                //P(canvas_sem);
                canvas_setpx(canvas, xk + xo, yk + yo, d);
                //V(canvas_sem);
                y += incy;
            }
            x += incx;
        }
    }

    static void *mandel_wrapper(void *param)
    {
        tparam_t *p = static_cast<tparam_t *>(param);
        p->mo->mandel_wrapper_2(param);
        return nullptr;
    }

    int mandel_wrapper_2(void *param)
    {
        tparam_t *p = (tparam_t *)param;
#if defined(__ZEPHYR__) && defined(CONFIG_FPU)
	// make sure FPU regs are saved during context switch
	int r;
	if ((r = k_float_enable(k_current_get(), 0)) != 0)
	{
	    log_msg("%s: k_float_enable() failed: %d.\n", __FUNCTION__, r);
	}
#endif
        P(p->go);
        sched_param sp;
        int pol;
#if 0
            int ret;
            pthread_getschedparam(worker_tasks[p->tno], &pol, &sp);
            sp.sched_priority = sp.sched_priority + (p->tno % 3);
            if ((ret = pthread_setschedparam(worker_tasks[p->tno], pol, &sp)) != 0)
                log_msg("pthread setschedparam failed for thread %d, %d\n", p->tno, ret);
#endif
        pthread_getschedparam(worker_tasks[p->tno], &pol, &sp);
        log_msg("starting thread %d with priority %d\n", p->tno, sp.sched_priority);

        sched_yield();
        mandel_helper(p->xl, p->yl, p->xh, p->yh, p->incx, p->incy, p->xoffset, p->yoffset, p->width, p->height);
        log_msg("finished thread %d\n", p->tno);
        VSem(p->sem); // report we've done our job

        return 0;
    }

    void mandel_setup(const int thread_no, myDOUBLE sx, myDOUBLE sy, myDOUBLE tx, myDOUBLE ty)
    {
        int t = 0;
        last_xr = (tx - sx);
        last_yr = (ty - sy);
        ssw = last_xr / xres;
        ssh = last_yr / yres;
        transx = sx;
        transy = sy;
        myDOUBLE stepx = (xres / thread_no) * ssw * xratio;
        myDOUBLE stepy = (yres / thread_no) * ssh;

        pthread_t th;
        log_msg("%s - 1\n", __FUNCTION__);
        if (thread_no > 16)
        {
            if (thread_no != 100)   /* 100 is a dummy to just initialize, don't confuse us with this log */
                log_msg("%s: too many threads(%d)... giving up.\n", __FUNCTION__, thread_no);
            return;
        }
        int w = (xres / thread_no);
        int h = (yres / thread_no);

        for (int tx = 0; tx < thread_no; tx++)
        {
            int xoffset = w * tx;
            for (int ty = 0; ty < thread_no; ty++)
            {
                int yoffset = h * ty;
                tp[t] = new tparam_t(t,
                                     w, h,
                                     tx * stepx + transx,
                                     ty * stepy + transy,
                                     tx * stepx + stepx + transx,
                                     ty * stepy + stepy + transy,
                                     ssw, ssh, xoffset, yoffset,
                                     master_sem, this);
        log_msg("%s - 2\n", __FUNCTION__);
                pthread_attr_init(&attr[t]);
                pthread_attr_setstack(&attr[t], stacks + t * STACK_SIZE, STACK_SIZE);
                pthread_attr_setschedpolicy(&attr[t], SCHED_RR);
                int ret;
        log_msg("%s - 3\n", __FUNCTION__);
                if ((ret = pthread_create(&th, &attr[t], mandel_wrapper, tp[t])) != 0)
                    log_msg("pthread create failed for thread %d, %d\n", t, ret);
                worker_tasks[t] = th;
                t++;
        log_msg("%s - 4\n", __FUNCTION__);

                pthread_detach(th);
        log_msg("%s - 5\n", __FUNCTION__);

                //usleep(20*1000); // needed to make zephyr happy when loggin is enabled. Seems some race with the the KickOff mutex. Maybe some bug?
        log_msg("%s - 6\n", __FUNCTION__);

            }
        }
    }

    void go(void)
    {
        memset(canvas, 0, CSIZE);

        if (clock_gettime(CLOCK_REALTIME, &tstart) < 0)
            perror("clock_gettime()");

        for (int i = 0; i < NO_THREADS; i++)
        {
            //usleep(250 * 1000);
            V(tp[i]->go);
        }
        log_msg("main thread waiting for %i threads...\n", NO_THREADS);
        for (int i = NO_THREADS; i; i--)
        {
            //log_msg("main thread waiting for %i threads...\n", i);
            PSem(master_sem); // wait until all workers have finished
        }
        if (clock_gettime(CLOCK_REALTIME, &tend) < 0)
            perror("clock_gettime()");

        log_msg("all threads finished.\n");
        free_ressources();
    }

    void free_ressources(void)
    {
        for (int i = 0; i < NO_THREADS; i++)
        {
            int ret;
#ifdef __ZEPHYR__
            // needed to cleanup all resources, namely a mutex within a pthread
            void *retval;
            if ((ret = pthread_join(worker_tasks[i], &retval)) != 0)
                log_msg("pthread_join failed: %d\n", ret);
#endif
            if ((ret = pthread_attr_destroy(&attr[i])) != 0)
                log_msg("pthread_attr_destroy failed: %d\n", ret);

            delete tp[i];
        }
    }

public:
    mandel(canvas_t c, char *st, myDOUBLE xl, myDOUBLE yl, myDOUBLE xh, myDOUBLE yh, uint16_t xr, uint16_t yr, myDOUBLE xrat = 1.0, int coffs = 0)
        : canvas(c), stacks(st), xres(xr), yres(yr), xratio(xrat), coffset(coffs), pixel_mask(0)
    {
#if 0            
        for (int i = 0; i < PAL_SIZE / 2; i++)
        {
            col_pal[i] = (i + coffset) * 2;
            col_pal[PAL_SIZE-1 - i] = (i + coffset) * 2;
#else
        for (int i = 0; i < PAL_SIZE; i++)
        {
            col_pal[i] = i;
#endif
        }
        for (int i = 0; i < PIXELW; i++)
            pixel_mask = (pixel_mask << 1) | 0b1;
        pthread_mutex_init(&canvas_sem, nullptr);
        log_msg("%s: canvas_mutex initialized: %p\n", __FUNCTION__, &canvas_sem);
        sem_init(&master_sem, 0, 0);
        mandel_setup(sqrt(NO_THREADS), xl, yl, xh, yh); // initialize some stuff, but don't calculate
        go();
        dump_result();
    }
    ~mandel()
    {
        log_msg("%s destructor\n", __FUNCTION__);
        pthread_mutex_destroy(&canvas_sem);
        sem_destroy(&master_sem);
    };
    
    char *get_stacks(void) { return stacks; }

    void dump_result(void)
    {
#ifndef C64
        canvas_dump(canvas);
#endif
        struct timespec dt;
        timespec_diff(&tend, &tstart, &dt);
        log_msg("mandelbrot set done in: %ld.%lds\n", dt.tv_sec, dt.tv_nsec / 1000000L);
    }

    void select_start(point_t &p)
    {
        mark_x1 = p.x; // - ((LV_HOR_RES_MAX - IMG_W) / 2);
        mark_y1 = p.y; // - ((LV_VER_RES_MAX - IMG_H) / 2);
        if (mark_x1 < 0)
            mark_x1 = 0;
        if (mark_y1 < 0)
            mark_y1 = 0;
    }

    void select_end(point_t &p)
    {
        mark_x2 = p.x; // - ((LV_HOR_RES_MAX - IMG_W) / 2);
        mark_y2 = p.y; // - ((LV_VER_RES_MAX - IMG_H) / 2);
        if (mark_x2 < 0)
            mark_x2 = 0;
        if (mark_y2 < 0)
            mark_y2 = 0;
        log_msg("rect coord: [%d,%d]x[%d,%d]\n", mark_x1, mark_y1, mark_x2, mark_y2);
        mandel_setup(sqrt(NO_THREADS),
                     static_cast<myDOUBLE>(mark_x1 * ssw + transx),
                     static_cast<myDOUBLE>(mark_y1 * ssh + transy),
                     static_cast<myDOUBLE>(mark_x2 * ssw + transx),
                     static_cast<myDOUBLE>(mark_y2 * ssh + transy));
        go();
        dump_result();
        mark_x1 = -1;
        mark_x2 = mark_x1;
        mark_y2 = mark_y1;
    }

#if 0
    void select_update(point_t &p)
    {
        if (mark_x1 < 0)
            return;
        coord_t tx = mark_x2;
        coord_t ty = mark_y2;
        mark_x2 = p.x; // - ((LV_HOR_RES_MAX - IMG_W) / 2);
        mark_y2 = p.y; // - ((LV_VER_RES_MAX - IMG_H) / 2);
        if (mark_x2 < 0)
            mark_x2 = 0;
        if (mark_y2 < 0)
            mark_y2 = 0;
        if ((tx == mark_x2) && (ty == mark_y2))
            return;
        //log_msg("rect coord: %dx%d - %dx%d\n", mark_x1, mark_y1, mark_x2, mark_y2);
    }
#endif
};

#endif
