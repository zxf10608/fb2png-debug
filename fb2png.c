/**
 * fb2png  Save screenshot into .png.
 *
 * Copyright (C) 2012  Kyan <kyan.ql.he@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <errno.h>

// for S_IREAD|S_IWRITE
#include <sys/stat.h>

#include "log.h"
#include "fb2png.h"
#include "fb.h"

/**
 * Get the {@code struct fb} from device's framebuffer.
 * Return
 *      0 for success.
 */
int get_device_fb(const char* path, struct fb *fb)
{
    int fd;
    int bytespp;
    int offset;
    char *x;
    struct fb_var_screeninfo vinfo;

    fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    if(ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        D("ioctl failed, %s\n", strerror(errno));
        return -1;
    }

    bytespp = vinfo.bits_per_pixel / 8;

    fb->bpp = vinfo.bits_per_pixel;
    fb->size = vinfo.xres * vinfo.yres * bytespp;
    fb->width = vinfo.xres;
    fb->height = vinfo.yres;
    fb->red_offset = vinfo.red.offset;
    fb->red_length = vinfo.red.length;
    fb->green_offset = vinfo.green.offset;
    fb->green_length = vinfo.green.length;
    fb->blue_offset = vinfo.blue.offset;
    fb->blue_length = vinfo.blue.length;
    fb->alpha_offset = vinfo.transp.offset;
    fb->alpha_length = vinfo.transp.length;

#ifdef ANDROID
    /* HACK: for several of 3d cores a specific alignment
     * is required so the start of the fb may not be an integer number of lines
     * from the base.  As a result we are storing the additional offset in
     * xoffset. This is not the correct usage for xoffset, it should be added
     * to each line, not just once at the beginning */

    offset = vinfo.xoffset * bytespp;

    /* Check if Android use double-buffer, capture 2nd */
    struct fb_fix_screeninfo fi;
    if (ioctl(fd, FBIOGET_FSCREENINFO, &fi) < 0) {
        D("failed to get fb0 info\n");
        return -1;
    }
    if (vinfo.yres * fi.line_length * 2 > fi.smem_len)
        offset += vinfo.xres * vinfo.yoffset * bytespp;
#else
    offset = 0;
#endif

    x = malloc(fb->size);
    if (!x) return -1;

    lseek(fd, offset, SEEK_SET);

    if (read(fd, x ,fb->size) != fb->size) goto oops;

    fb->data = x;

    // debug, write the raw buffer
    int fp_out = open("/sdcard/fbshot.raw", O_CREAT|O_TRUNC|O_RDWR, S_IRUSR|S_IWUSR);
    if (fp_out < 0) {
        D("failed to create raw image\n");
        close(fd);
        return -1;
    }
    if (write(fp_out, fb->data, fb->size) != fb->size) {
        D("failed to write raw image\n");
        close(fd);
        close(fp_out);
        return -1;
    }
    close(fp_out);
    //----- end debug write raw buffer

    close(fd);

    return 0;

oops:
    close(fd);
    free(x);
    return -1;
}

int fb2png(const char *path)
{
    struct fb fb;
    int ret;

#ifdef ANDROID
    ret = get_device_fb("/dev/graphics/fb0", &fb);
#else
    ret = get_device_fb("/dev/fb0", &fb);
#endif

    if (ret) {
        D("Failed to read framebuffer.");
        return -1;
    }

    fb_dump(&fb);

    return fb_save_png(&fb, path);
}

