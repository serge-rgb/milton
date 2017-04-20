uniform sampler2D u_canvas;
uniform vec2 u_screen_size;
uniform int u_kernel_size;
uniform int u_direction;

float gauss(float v, float sigma_squared)
{
    return pow(2.718, -((v*v)/(2*sigma_squared)));
}

#define KERNEL_MULTIPLE 2
#define GAUSSIAN 0

void
main()
{
#if HAS_TEXTURE_MULTISAMPLE
    #error "Not implemented"
#else
    vec2 screen_point = vec2(gl_FragCoord.x, gl_FragCoord.y);
    vec2 coord = screen_point / u_screen_size;
    vec4 color = texture(u_canvas, coord);
    #if GAUSSIAN
    if ( u_kernel_size > 2 ) {
        if ( u_direction == 0 ) {
            for ( int y = -KERNEL_MULTIPLE*u_kernel_size; y < KERNEL_MULTIPLE*u_kernel_size; ++y ) {
                float val = gauss(y, u_kernel_size*u_kernel_size);
                int x = 0;
                color += val*texture(u_canvas, (screen_point + vec2(x,y)) / u_screen_size);
            }
            color /= sqrt(2 * 3.14 * u_kernel_size*u_kernel_size);
        }
        else {
            for ( int x = -KERNEL_MULTIPLE*u_kernel_size; x < KERNEL_MULTIPLE*u_kernel_size; ++x ) {
                float val = gauss(x, u_kernel_size*u_kernel_size);
                int y = 0;
                color += val*texture(u_canvas, (screen_point + vec2(x,y)) / u_screen_size);
            }
            color /= sqrt(2 * 3.14 * u_kernel_size*u_kernel_size);
        }
    } else {
        out_color = color;
    }
    #else
    // LINEAR
    if ( u_kernel_size > 2 ) {
        if ( u_direction == 0 ) {
            for ( int y = -u_kernel_size; y < u_kernel_size; ++y ) {
                color += texture(u_canvas, (screen_point+vec2(0,y)) / u_screen_size);
            }
        } else {
            for ( int x = -u_kernel_size; x < u_kernel_size; ++x ) {
                color += texture(u_canvas, (screen_point+vec2(x,0)) / u_screen_size);
            }

        }
        color /= 2*u_kernel_size;
    }
    #endif
    out_color = color;
#endif
}
