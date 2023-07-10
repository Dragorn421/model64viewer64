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

    GLfloat angle = 0;

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

    rdpq_debug_start();

    while (true)
    {
        surface_t *disp = display_get();

        frame_starts[cur_frame_ring_i] = get_ticks();

        rdpq_attach(disp, &zbuffer);
        gl_context_begin();

        glClearColor(0.3, 0.2, 0.7, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_MULTISAMPLE_ARB);

        glEnable(GL_DEPTH_TEST);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(80.0f, 4.0f / 3.0f, 1.0f, 10.0f);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(
            -5, 3, 1,
            0, 2, 0,
            0, 1, 0);

        // make the object rotate
        glRotatef(angle, 0, 1, 0);
        angle += 360.0f / 100;

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
            graphics_draw_text(disp, 10, 5, str);
        }

        display_show(disp);
    }
}
