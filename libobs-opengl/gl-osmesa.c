#include <stdio.h>

#include "gl-subsystem.h"
#include <glad/glad_osmesa.h>

#define MESA_WIDTH 1920
#define MESA_HEIGHT 1080

struct gl_windowinfo {
};

struct gl_platform {
	OSMesaContext context;
	void *buffer;
};

extern struct gl_windowinfo *gl_windowinfo_create(const struct gs_init_data *info)
{
	UNUSED_PARAMETER(info);
	return bmalloc(sizeof(struct gl_windowinfo));
}

extern void gl_windowinfo_destroy(struct gl_windowinfo *info)
{
	UNUSED_PARAMETER(info);
	bfree(info);
}

extern struct gl_platform *gl_platform_create(gs_device_t *device,
		uint32_t adapter)
{
	struct gl_platform *plat = bzalloc(sizeof(struct gl_platform));

	device->plat = plat;

	plat->context = OSMesaCreateContextExt(OSMESA_RGBA, 8, 0, 0, NULL);
	if (!plat->context)
		goto fail;
	plat->buffer = malloc(MESA_WIDTH * MESA_HEIGHT * 4 * sizeof(GLubyte));

	OSMesaMakeCurrent(plat->context, plat->buffer, GL_UNSIGNED_BYTE, MESA_WIDTH, MESA_HEIGHT);

	if (!gladLoadGL())
		goto fail;

	return plat;

fail:
	blog(LOG_ERROR, "gl_platform_create failed");
	gl_platform_destroy(plat);

	UNUSED_PARAMETER(device);
	UNUSED_PARAMETER(adapter);
	return NULL;
}

extern void gl_platform_destroy(struct gl_platform *plat)
{
	OSMesaDestroyContext(plat->context);
	free(plat->buffer);
}

extern bool gl_platform_init_swapchain(struct gs_swap_chain *swap)
{
	UNUSED_PARAMETER(swap);
	return true;
}

extern void gl_platform_cleanup_swapchain(struct gs_swap_chain *swap)
{
	UNUSED_PARAMETER(swap);
}

extern void device_enter_context(gs_device_t *device)
{
	OSMesaMakeCurrent(device->plat->context, device->plat->buffer, GL_UNSIGNED_BYTE, MESA_WIDTH, MESA_HEIGHT);
}

extern void device_leave_context(gs_device_t *device)
{
	OSMesaMakeCurrent(device->plat->context, NULL, GL_UNSIGNED_BYTE, 0, 0);
}

extern void gl_getclientsize(const struct gs_swap_chain *swap,
			     uint32_t *width, uint32_t *height)
{
	if (width) *width = swap->info.cx;
	if (height) *height = swap->info.cy;

	UNUSED_PARAMETER(swap);
}

extern void gl_update(gs_device_t *device)
{
	UNUSED_PARAMETER(device);
}

extern void device_load_swapchain(gs_device_t *device, gs_swapchain_t *swap)
{
	UNUSED_PARAMETER(device);
	UNUSED_PARAMETER(swap);
}

extern void device_present(gs_device_t *device)
{
	UNUSED_PARAMETER(device);
}
