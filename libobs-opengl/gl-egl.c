#include <stdio.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <fcntl.h>
#include <gbm.h>

#include "gl-subsystem.h"
#include <glad/glad_egl.h>

struct gl_windowinfo {
};

struct gl_platform {
	EGLDisplay display;
	EGLConfig config;
	EGLContext context;
	EGLSurface surface;
};

static const EGLint context_attribs[] = {
	EGL_CONTEXT_MAJOR_VERSION, 2,
	EGL_CONTEXT_MINOR_VERSION, 1,
	EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
	EGL_NONE
};

static const EGLint config_attribs[] = {
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_ALPHA_SIZE, 8,
	EGL_DEPTH_SIZE, 0,
	EGL_STENCIL_SIZE, 0,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
	EGL_NONE
};

static struct {
	struct gbm_device *dev;
	struct gbm_surface *surface;
} gbm;

static struct {
	int fd;
	drmModeModeInfo *mode;
	uint32_t crtc_id;
	uint32_t connector_id;
} drm;


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
	EGLint major, minor, n;
	drmModeRes *resources;
	drmModeConnector *connector = NULL;
	drmModeEncoder *encoder = NULL;
	int i, area;

	struct gl_platform *plat = bzalloc(sizeof(struct gl_platform));

	device->plat = plat;

	drm.fd = open("/dev/dri/card0", O_RDWR);
	if (drm.fd < 0) {
		blog(LOG_ERROR, "could not open drm device\n");
		goto fail;
	}

	resources = drmModeGetResources(drm.fd);
	if (!resources) {
		blog(LOG_ERROR, "drmModeGetResources failed: %s\n", strerror(errno));
		goto fail;
	}

	/* find a connected connector: */
	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(drm.fd, resources->connectors[i]);
		if (connector->connection == DRM_MODE_CONNECTED) {
			/* it's connected, let's use this! */
			break;
		}
		drmModeFreeConnector(connector);
		connector = NULL;
	}

	if (!connector) {
		/* we could be fancy and listen for hotplug events and wait for
		* a connector..
		*/
		blog(LOG_ERROR, "no connected connector!\n");
		goto fail;
	}

	/* find prefered mode or the highest resolution mode: */
	for (i = 0, area = 0; i < connector->count_modes; i++) {
		drmModeModeInfo *current_mode = &connector->modes[i];

		if (current_mode->type & DRM_MODE_TYPE_PREFERRED) {
			drm.mode = current_mode;
		}

		int current_area = current_mode->hdisplay * current_mode->vdisplay;
		if (current_area > area) {
			drm.mode = current_mode;
			area = current_area;
		}
	}

	if (!drm.mode) {
		blog(LOG_ERROR, "could not find mode!\n");
		goto fail;
	}

	gbm.dev = gbm_create_device(drm.fd);

	gbm.surface = gbm_surface_create(gbm.dev,
	    drm.mode->hdisplay, drm.mode->vdisplay,
	    GBM_FORMAT_XRGB8888,
	    GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

	if (!gbm.surface) {
		blog(LOG_ERROR, "failed to create gbm surface\n");
		goto fail;
	}

	plat->display = eglGetDisplay(gbm.dev);

	if (!eglInitialize(plat->display, &major, &minor)) {
		blog(LOG_ERROR, "failed to initialize\n");
		goto fail;
	}

	if (!eglBindAPI(EGL_OPENGL_API)) {
		blog(LOG_ERROR, "failed to bind api EGL_OPENGL_API\n");
		goto fail;
        }

	if (!eglChooseConfig(plat->display, config_attribs, &plat->config, 1, &n) || n != 1) {
		blog(LOG_ERROR, "failed to choose config: %d\n", n);
		goto fail;
	}

	plat->context = eglCreateContext(plat->display, plat->config, EGL_NO_CONTEXT, context_attribs);
	if (!plat->context)
		goto fail;

	plat->surface = eglCreateWindowSurface(plat->display, plat->config, gbm.surface, NULL);

	if (plat->surface == EGL_NO_SURFACE) {
		blog(LOG_ERROR, "failed to create egl surface\n");
		goto fail;
	}
	if (!eglMakeCurrent(plat->display, plat->surface, plat->surface, plat->context)) {
		blog(LOG_ERROR, "failed to switch egl current\n");
		goto fail;
	}

	blog(LOG_DEBUG, "Using display %p with EGL version %d.%d\n",
	    plat->display, major, minor);

	blog(LOG_DEBUG, "EGL Version \"%s\"\n", eglQueryString(plat->display, EGL_VERSION));
	blog(LOG_DEBUG, "EGL Vendor \"%s\"\n", eglQueryString(plat->display, EGL_VENDOR));
	blog(LOG_DEBUG, "EGL Extensions \"%s\"\n", eglQueryString(plat->display, EGL_EXTENSIONS));

	if (!gladLoadEGL())
		goto fail;

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
	eglTerminate(plat->display);
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
	if (!eglMakeCurrent(device->plat->display, device->plat->surface, device->plat->surface, device->plat->context)) {
		blog(LOG_ERROR, "failed enter egl current\n");
	}

}

extern void device_leave_context(gs_device_t *device)
{
	if (!eglMakeCurrent(device->plat->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
		blog(LOG_ERROR, "failed leave egl current\n");
	}
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
