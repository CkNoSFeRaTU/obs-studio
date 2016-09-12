#define OSMESA_MAJOR_VERSION 10
#define OSMESA_MINOR_VERSION 0
#define OSMESA_PATCH_VERSION 0



/*
 * Values for the format parameter of OSMesaCreateContext()
 * New in version 2.0.
 */
#define OSMESA_COLOR_INDEX	GL_COLOR_INDEX
#define OSMESA_RGBA		GL_RGBA
#define OSMESA_BGRA		0x1
#define OSMESA_ARGB		0x2
#define OSMESA_RGB		GL_RGB
#define OSMESA_BGR		0x4
#define OSMESA_RGB_565		0x5


/*
 * OSMesaPixelStore() parameters:
 * New in version 2.0.
 */
#define OSMESA_ROW_LENGTH	0x10
#define OSMESA_Y_UP		0x11


/*
 * Accepted by OSMesaGetIntegerv:
 */
#define OSMESA_WIDTH		0x20
#define OSMESA_HEIGHT		0x21
#define OSMESA_FORMAT		0x22
#define OSMESA_TYPE		0x23
#define OSMESA_MAX_WIDTH	0x24  /* new in 4.0 */
#define OSMESA_MAX_HEIGHT	0x25  /* new in 4.0 */


typedef struct osmesa_context *OSMesaContext;


/*
 * Create an Off-Screen Mesa rendering context.  The only attribute needed is
 * an RGBA vs Color-Index mode flag.
 *
 * Input:  format - one of OSMESA_COLOR_INDEX, OSMESA_RGBA, OSMESA_BGRA,
 *                  OSMESA_ARGB, OSMESA_RGB, or OSMESA_BGR.
 *         sharelist - specifies another OSMesaContext with which to share
 *                     display lists.  NULL indicates no sharing.
 * Return:  an OSMesaContext or 0 if error
 */
GLAPI OSMesaContext APIENTRY
OSMesaCreateContext( GLenum format, OSMesaContext sharelist );



/*
 * Create an Off-Screen Mesa rendering context and specify desired
 * size of depth buffer, stencil buffer and accumulation buffer.
 * If you specify zero for depthBits, stencilBits, accumBits you
 * can save some memory.
 *
 * New in Mesa 3.5
 */
GLAPI OSMesaContext APIENTRY
OSMesaCreateContextExt( GLenum format, GLint depthBits, GLint stencilBits,
                        GLint accumBits, OSMesaContext sharelist);


/*
 * Destroy an Off-Screen Mesa rendering context.
 *
 * Input:  ctx - the context to destroy
 */
GLAPI void APIENTRY
OSMesaDestroyContext( OSMesaContext ctx );



/*
 * Bind an OSMesaContext to an image buffer.  The image buffer is just a
 * block of memory which the client provides.  Its size must be at least
 * as large as width*height*sizeof(type).  Its address should be a multiple
 * of 4 if using RGBA mode.
 *
 * Image data is stored in the order of glDrawPixels:  row-major order
 * with the lower-left image pixel stored in the first array position
 * (ie. bottom-to-top).
 *
 * Since the only type initially supported is GL_UNSIGNED_BYTE, if the
 * context is in RGBA mode, each pixel will be stored as a 4-byte RGBA
 * value.  If the context is in color indexed mode, each pixel will be
 * stored as a 1-byte value.
 *
 * If the context's viewport hasn't been initialized yet, it will now be
 * initialized to (0,0,width,height).
 *
 * Input:  ctx - the rendering context
 *         buffer - the image buffer memory
 *         type - data type for pixel components, only GL_UNSIGNED_BYTE
 *                supported now
 *         width, height - size of image buffer in pixels, at least 1
 * Return:  GL_TRUE if success, GL_FALSE if error because of invalid ctx,
 *          invalid buffer address, type!=GL_UNSIGNED_BYTE, width<1, height<1,
 *          width>internal limit or height>internal limit.
 */
GLAPI GLboolean APIENTRY
OSMesaMakeCurrent( OSMesaContext ctx, void *buffer, GLenum type,
                   GLsizei width, GLsizei height );




/*
 * Return the current Off-Screen Mesa rendering context handle.
 */
GLAPI OSMesaContext APIENTRY
OSMesaGetCurrentContext( void );



/*
 * Set pixel store/packing parameters for the current context.
 * This is similar to glPixelStore.
 * Input:  pname - OSMESA_ROW_LENGTH
 *                    specify actual pixels per row in image buffer
 *                    0 = same as image width (default)
 *                 OSMESA_Y_UP
 *                    zero = Y coordinates increase downward
 *                    non-zero = Y coordinates increase upward (default)
 *         value - the value for the parameter pname
 *
 * New in version 2.0.
 */
GLAPI void APIENTRY
OSMesaPixelStore( GLint pname, GLint value );



/*
 * Return an integer value like glGetIntegerv.
 * Input:  pname -
 *                 OSMESA_WIDTH  return current image width
 *                 OSMESA_HEIGHT  return current image height
 *                 OSMESA_FORMAT  return image format
 *                 OSMESA_TYPE  return color component data type
 *                 OSMESA_ROW_LENGTH return row length in pixels
 *                 OSMESA_Y_UP returns 1 or 0 to indicate Y axis direction
 *         value - pointer to integer in which to return result.
 */
GLAPI void APIENTRY
OSMesaGetIntegerv( GLint pname, GLint *value );



/*
 * Return the depth buffer associated with an OSMesa context.
 * Input:  c - the OSMesa context
 * Output:  width, height - size of buffer in pixels
 *          bytesPerValue - bytes per depth value (2 or 4)
 *          buffer - pointer to depth buffer values
 * Return:  GL_TRUE or GL_FALSE to indicate success or failure.
 *
 * New in Mesa 2.4.
 */
GLAPI GLboolean APIENTRY
OSMesaGetDepthBuffer( OSMesaContext c, GLint *width, GLint *height,
                      GLint *bytesPerValue, void **buffer );



/*
 * Return the color buffer associated with an OSMesa context.
 * Input:  c - the OSMesa context
 * Output:  width, height - size of buffer in pixels
 *          format - buffer format (OSMESA_FORMAT)
 *          buffer - pointer to depth buffer values
 * Return:  GL_TRUE or GL_FALSE to indicate success or failure.
 *
 * New in Mesa 3.3.
 */
GLAPI GLboolean APIENTRY
OSMesaGetColorBuffer( OSMesaContext c, GLint *width, GLint *height,
                      GLint *format, void **buffer );



/**
 * This typedef is new in Mesa 6.3.
 */
typedef void (*OSMESAproc)();


/*
 * Return pointer to the named function.
 * New in Mesa 4.1
 * Return OSMESAproc in 6.3.
 */
GLAPI OSMESAproc APIENTRY
OSMesaGetProcAddress( const char *funcName );



/**
 * Enable/disable color clamping, off by default.
 * New in Mesa 6.4.2
 */
GLAPI void APIENTRY
OSMesaColorClamp(GLboolean enable);


/**
 * Enable/disable Gallium post-process filters.
 * This should be called after a context is created, but before it is
 * made current for the first time.  After a context has been made
 * current, this function has no effect.
 * If the enable_value param is zero, the filter is disabled.  Otherwise
 * the filter is enabled, and the value may control the filter's quality.
 * New in Mesa 10.0
 */
GLAPI void APIENTRY
OSMesaPostprocess(OSMesaContext osmesa, const char *filter,
                  unsigned enable_value);

