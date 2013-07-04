#include <stdio.h>
#include <stdlib.h>

#include <barrelfish/barrelfish.h>

#include <dev/omap/omap44xx_cam_prm_dev.h>
#include <dev/omap/omap44xx_cam_cm2_dev.h>
#include <dev/omap/omap44xx_fdif_dev.h>
#include <dev/omap/omap44xx_sr_mpu_dev.h>
#include <dev/omap/omap44xx_device_prm_dev.h>

#include "fdif.h"

#define PRINT_BUFFER_SIZE (1024*1024)
static char printbuf[PRINT_BUFFER_SIZE];

static omap44xx_cam_prm_t dev;
static omap44xx_fdif_t devfdif;
static omap44xx_cam_cm2_t devclk;
//static omap44xx_sr_mpu_t devvolt;
static omap44xx_device_prm_t devprm;

extern struct gimage lena_image;

// XXX: you need to have this functions in user space...
// not sure about these two, tough
//void cp15_invalidate_tlb(void);
//void cp15_invalidate_i_and_d_caches(void);

static lpaddr_t* vbase_glbl;

static void manage_clocks(void)
{
    printf("Enable the clocks in domain CD_CAM\n");

    // Clock domain CAM
    lvaddr_t* vbase;
    errval_t err;
    err = map_device_register(0x4A009000, 4096, &vbase);
    assert(err_is_ok(err));
    FDIF_DEBUG("vbase points to %p\n", vbase);

    omap44xx_cam_cm2_initialize(&devclk, (mackerel_addr_t)vbase);
    //omap44xx_cam_cm2_pm_cam_pwrstctrl_powerstate_wrf(&dev, omap44xx_cam_prm_POWERSTATE_2);
    omap44xx_cam_cm2_cm_cam_clkstctrl_clktrctrl_wrf(&devclk, 0x2);
    omap44xx_cam_cm2_cm_cam_fdif_clkctrl_modulemode_wrf(&devclk, 0x2);

    //omap44xx_cam_cm2_pr(printbuf, PRINT_BUFFER_SIZE, &devclk);
    //printf("%s\n", printbuf);

    //printf("Enable all the dependencies we can\n");
    //omap44xx_cam_cm2_cm_cam_staticdep_l3_1_statdep_wrf(&devclk, 0x1);
    //omap44xx_cam_cm2_cm_cam_staticdep_memif_statdep_wrf(&devclk, 0x1);
    //omap44xx_cam_cm2_cm_cam_staticdep_ivahd_statdep_wrf(&devclk, 0x1);

    //omap44xx_cam_cm2_pr(printbuf, PRINT_BUFFER_SIZE, &devclk);
    //printf("%s\n", printbuf);


    printf("Handle voltage for domain: VDD_CORE_L\n");
    
    // TODO access to smartreflex register not working, why?
    //offset = (0x4A0DD000 & ARM_L1_SECTION_MASK);
    //omap44xx_sr_mpu_initialize(&devvolt, (mackerel_addr_t)vbase+offset);
    //omap44xx_sr_mpu_srstatus_pr(printbuf, PRINT_BUFFER_SIZE-1, &devvolt);
    //printf("%s\n", printbuf);

    err = map_device_register(0x4A307000, 4096, &vbase_glbl);
    assert(err_is_ok(err));
    lpaddr_t offset = (0x4A307B00 & 0xFFF);
    omap44xx_device_prm_initialize(&devprm, (mackerel_addr_t)vbase_glbl+offset);
    //omap44xx_device_prm_pr(printbuf, PRINT_BUFFER_SIZE, &devprm);
    //printf("%s\n", printbuf);

    // Init voltage controller
    printf("Done handling voltage\n");
}

static void manage_power(void) 
{
    printf("Power-on the PD_CAM domain for fdif\n");

    // Power domain CAM
    lvaddr_t* vbase;
    errval_t err;
    err = map_device_register(0x4A307000, 4096, &vbase);
    assert(err_is_ok(err));

    omap44xx_cam_prm_initialize(&dev, (mackerel_addr_t)vbase);
    omap44xx_cam_prm_pm_cam_pwrstctrl_powerstate_wrf(&dev, omap44xx_cam_prm_POWERSTATE_3);

    while(omap44xx_cam_prm_pm_cam_pwrstst_powerstatest_rdf(&dev)
          != omap44xx_cam_prm_POWERSTATEST_3_r)
    {}

    //omap44xx_cam_prm_pr(printbuf, PRINT_BUFFER_SIZE, &dev);
    //printf("%s\n", printbuf);

    // Face detect Module
    err = map_device_register(0x4A10A000, 4096, &vbase);
    assert(err_is_ok(err));

    omap44xx_fdif_initialize(&devfdif, (mackerel_addr_t)vbase);

    // Set this to 0x1 to force the device off the standby mode
    omap44xx_fdif_fdif_sysconfig_standbymode_wrf(&devfdif, 0x2);

    omap44xx_fdif_fdif_sysconfig_pr(printbuf, PRINT_BUFFER_SIZE, &devfdif);
    printf("%s\n", printbuf);

    omap44xx_cam_cm2_pr(printbuf, PRINT_BUFFER_SIZE, &devclk);
    printf("%s\n", printbuf);

}

int main(void) {
    init_memory_manager();

    manage_clocks();
    manage_power();
    //manage_voltage();

    printf("FDIF Global Initialization\n");
    omap44xx_fdif_fdif_sysconfig_softreset_wrf(&devfdif, 1);
    while (omap44xx_fdif_fdif_sysconfig_softreset_rdf(&devfdif) != 0);

    omap44xx_fdif_fdif_ctrl_max_tags_wrf(&devfdif, 0xA);

    omap44xx_fdif_fdif_irqenable_set_finish_irq_wrf(&devfdif, 0, 1);
    omap44xx_fdif_fdif_irqenable_set_finish_irq_wrf(&devfdif, 1, 1);
    omap44xx_fdif_fdif_irqenable_set_finish_irq_wrf(&devfdif, 2, 1);

    printf("Polling Method\n");
    printf("Set Image Parameters\n");

    size_t img_size = 320*240*8; // 75 KB
    size_t working_size = img_size; // 51.25 KB is enough
    size_t retbytes;
    void* workarea;
    uint8_t* image;
    errval_t err;

    struct capref img_cap;
    struct capref workarea_cap;

    err = frame_alloc(&img_cap, img_size, &retbytes);
    assert(err_is_ok(err));
    assert(retbytes >= img_size);

    err = frame_alloc(&workarea_cap, working_size, &retbytes);
    assert(err_is_ok(err));
    assert(retbytes >= working_size);

    err = vspace_map_one_frame((void**)&image, img_size, img_cap,
                              NULL, NULL);
    assert(err_is_ok(err));
    err = vspace_map_one_frame(&workarea, working_size, workarea_cap,
                              NULL, NULL);
    assert(err_is_ok(err));

    printf("Set up the image ...\n");
    printf("RGB image type: width:%d height:%d bytes/pixel:%d\n", lena_image.width, lena_image.height, lena_image.bytes_per_pixel);
    printf("Convert to grayscale, store in image buffer...\n");

    for (int i=0; i<lena_image.width*lena_image.height; i+=1) {
        image[i] = lena_image.pixel_data[i];
    }

    // TODO We should make sure here that image is actually fully in memory
    // and not still hanging around in the cache

    // Does this do cache to mem writeback?
    //cp15_invalidate_tlb();
    //cp15_invalidate_i_and_d_caches();
    struct frame_identity ret;
    err = invoke_frame_identify(img_cap, &ret);
    assert (err_is_ok(err));
    omap44xx_fdif_fdif_picaddr_wr(&devfdif, ret.base); // make sure 5 least significant bits are 0!

    err = invoke_frame_identify(workarea_cap, &ret);
    assert (err_is_ok(err));
    omap44xx_fdif_fdif_wkaddr_wr(&devfdif, ret.base); // make sure 5 least significant bits are 0!
    
    omap44xx_fdif_fd_dcond_min_wrf(&devfdif, 0x0); // 40 pixel
    omap44xx_fdif_fd_dcond_dir_wrf(&devfdif, 0x0); // up?

    omap44xx_fdif_fd_startx_startx_wrf(&devfdif, 0x0);
    omap44xx_fdif_fd_starty_starty_wrf(&devfdif, 0x0);

    omap44xx_fdif_fd_sizex_sizex_wrf(&devfdif, 0x140); // TODO
    omap44xx_fdif_fd_sizey_sizey_wrf(&devfdif, 0xf0); // TODO
    omap44xx_fdif_fd_lhit_lhit_wrf(&devfdif, 0x5);

    omap44xx_fdif_fd_ctrl_run_wrf(&devfdif, 0x1);

    while(omap44xx_fdif_fd_ctrl_finish_rdf(&devfdif) != 0x1);

    printf("Face detection completed:\n");
    printf("Read the results...\n");
    
    int faces = omap44xx_fdif_fd_dnum_dnum_rdf(&devfdif);
    printf("Faces found: %d\n", faces);
    //omap44xx_fdif_pr(printbuf, PRINT_BUFFER_SIZE, &devfdif);
    //printf("%s\n", printbuf);

    for (int i=0; i<faces; i++) {
        printf("Face %d:\n", i);
        int x = omap44xx_fdif_fd_centerx_centerx_rdf(&devfdif, i);
        int y = omap44xx_fdif_fd_centery_centery_rdf(&devfdif, i);
        printf("Position (X,Y): %d %d\n", x, y);

        int size = omap44xx_fdif_fd_confsize_size_rdf(&devfdif, i);
        int confidence = omap44xx_fdif_fd_confsize_conf_rdf(&devfdif, i);
        int angle = omap44xx_fdif_fd_angle_angle_rdf(&devfdif, i);
        printf("Size: %d Confidence: %d Angle: %d\n", size, confidence, angle);
    }

    return 0;
}
