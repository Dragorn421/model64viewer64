#include <libdragon.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/gl_integration.h>
#include <model64.h>
#include <math.h>
#include <malloc.h>
#include <dir.h>
#include <usb.h>

#include "../inject_offset.c"

int main()
{
    debug_init_usblog();

    controller_init();

    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 3, GAMMA_NONE, ANTIALIAS_RESAMPLE_FETCH_ALWAYS);
    rdpq_init();

    gl_init();

    surface_t zbuffer = surface_alloc(FMT_RGBA16, display_get_width(), display_get_height());

    int meta_dmasize = (META_SIZE + 15) / 16 * 16;
    void *valsbuf = memalign(16, meta_dmasize);
    dma_read(valsbuf, 0x10000000 | INJECT_OFFSET_META, meta_dmasize);
    data_cache_hit_invalidate(valsbuf, meta_dmasize);

    int size, offset;
    read_meta(valsbuf, &offset, &size);

    uint32_t pi_addr_offset = 0x10000000 | offset;

    debugf("pi_addr_offset = 0x%08lX\n", pi_addr_offset);
    debugf("size = 0x%X\n", size);

    uint32_t dma_size = (size + 15) / 16 * 16;
    debugf("dma_size = 0x%lX\n", dma_size);

    void *buf = memalign(16, dma_size);
    dma_read(buf, pi_addr_offset, dma_size);
    data_cache_hit_invalidate(buf, dma_size);

    model64_t *model = model64_load_buf(buf, size);

    float atToEyeAngle = 0, atToEyeDist = 5;
    float atToEyeY = 0, atToEye[3];
    float eyeX, eyeY, eyeZ,
        atX = 0, atY = 0, atZ = 0;

    // profiling
    int n_frames_store_starts = 50;
    int cur_frame_ring_i = 0;
    unsigned long frame_starts[n_frames_store_starts];
    unsigned long frame_ends[n_frames_store_starts];
    bool frame_starts_initialized = false;

    // use_the_dlist: true to generate a dlist for displaying the mesh and use it every frame
    bool use_the_dlist = true;
    GLuint the_dlist;
    bool the_dlist_is_built = false;

    bool enable_aa = true;
    bool show_hud = true;

    rdpq_debug_start();

    uint32_t ticks_last = TICKS_READ();

    while (true)
    {
        surface_t *disp = display_get();

        frame_starts[cur_frame_ring_i] = get_ticks();

        rdpq_attach(disp, &zbuffer);
        gl_context_begin();

        glClearColor(0.3, 0.2, 0.7, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (enable_aa)
            glEnable(GL_MULTISAMPLE_ARB);

        glEnable(GL_DEPTH_TEST);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(80.0f, 4.0f / 3.0f, 1.0f, 10.0f);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        atToEye[0] = atToEyeDist * cos(atToEyeAngle);
        atToEye[2] = atToEyeDist * sin(atToEyeAngle);
        atToEye[1] = atToEyeY;
        eyeX = atToEye[0] + atX;
        eyeY = atToEye[1] + atY;
        eyeZ = atToEye[2] + atZ;
        gluLookAt(
            eyeX, eyeY, eyeZ,
            atX, atY, atZ,
            0, 1, 0);

        if (!use_the_dlist || !the_dlist_is_built)
        {
            if (use_the_dlist)
            {
                the_dlist_is_built = true;

                the_dlist = glGenLists(1);
                glNewList(the_dlist, GL_COMPILE);
            }

            model64_draw(model);

            if (use_the_dlist)
            {
                glEndList();
            }
        }

        if (use_the_dlist)
        {
            glCallList(the_dlist);
        }

        gl_context_end();

        // wait for rdp to be done with the framebuffer to call graphics_draw_text (cpu framebuffer writing) below
        rdpq_detach_wait();

        frame_ends[cur_frame_ring_i] = get_ticks();
        cur_frame_ring_i++;
        if (cur_frame_ring_i >= n_frames_store_starts)
        {
            cur_frame_ring_i = 0;
            frame_starts_initialized = true;
        }
        if (frame_starts_initialized)
        {
            unsigned long average_processing_ticks_per_frame = 0;
            for (int i = 0; i < n_frames_store_starts; i++)
            {
                average_processing_ticks_per_frame += TICKS_DISTANCE(frame_starts[i], frame_ends[i]) / n_frames_store_starts;
            }
            float average_processing_ms_per_frame = (float)average_processing_ticks_per_frame / TICKS_PER_SECOND * 1000;
            char str[1000];
            snprintf(str, sizeof(str), "ms/frame=%f frame/s=%f",
                     average_processing_ms_per_frame, 1000 / average_processing_ms_per_frame);
            if (show_hud)
                graphics_draw_text(disp, 10, 5, str);
        }

        if (show_hud)
        {
            char str[1000];
            snprintf(str, sizeof(str), "AA:%s DList:%s (press A/B)",
                     enable_aa ? "ON" : "OFF", use_the_dlist ? "Yes" : "No");
            graphics_draw_text(disp, 10, 15, str);
            graphics_draw_text(disp, 10, 25, "Controls: stick=left/right/down/up");
            graphics_draw_text(disp, 10, 35, "dpad-up/down=nearer/further");
            graphics_draw_text(disp, 10, 45, "C-up/down=target up/down");
            graphics_draw_text(disp, 10, 55, "Z=toggle 'hud'");
        }

        display_show(disp);

        controller_scan();
        struct controller_data down = get_keys_down();
        struct controller_data pressed = get_keys_pressed();

        if (down.c[0].A)
        {
            enable_aa = !enable_aa;
        }

        if (down.c[0].B)
        {
            use_the_dlist = !use_the_dlist;
        }

        if (down.c[0].Z)
        {
            show_hud = !show_hud;
        }

        uint32_t ticks_now = TICKS_READ();
        float deltat = TICKS_DISTANCE(ticks_last, ticks_now) / (float)TICKS_PER_SECOND;
        ticks_last = ticks_now;

        // debugf("%08lx-%08lx deltat=%f %d\n", ticks_last, ticks_now, deltat, pressed.c[0].x);

        float eyeAngleSpeed = 10.0f, eyedistSpeed = 5.0f, eyeYspeed = 4.0f, atYspeed = 5.0f;

        float stickthreshold = 20;
        if (fabsf(pressed.c[0].x) > stickthreshold)
            atToEyeAngle += (pressed.c[0].x - stickthreshold) / (127 - stickthreshold) * eyeAngleSpeed * deltat;

        if (fabsf(pressed.c[0].y) > stickthreshold)
            atToEyeY += (pressed.c[0].y - stickthreshold) / (127 - stickthreshold) * eyedistSpeed * deltat;

        if (pressed.c[0].down || pressed.c[0].up)
            atToEyeDist += (pressed.c[0].down ? -1 : 1) * eyeYspeed * deltat;

        if (pressed.c[0].C_down || pressed.c[0].C_up)
            atY += (pressed.c[0].C_down ? -1 : 1) * atYspeed * deltat;

        float atToEyeDist_min = 0.1;
        if (atToEyeDist < atToEyeDist_min)
            atToEyeDist = atToEyeDist_min;
    }
}
