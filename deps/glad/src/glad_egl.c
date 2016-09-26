#include <string.h>
#include <glad/glad_egl.h>

int 
gladLoadEGL(void) {
    return gladLoadEGLLoader((GLADloadproc)eglGetProcAddress);
}

static int find_extensionsEGL(void) {
	return 1;
}

static void find_coreEGL(void) {
}

int gladLoadEGLLoader(GLADloadproc load) {
	find_coreEGL();

	if (!find_extensionsEGL()) return 0;
	return 1;
}
