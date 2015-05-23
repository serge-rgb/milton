// Simple platform abstraction layer.

static void platform_update_view(CanvasView* view, v2i screen_size, v2i pan_delta)
{
    view->screen_size = screen_size;
    view->screen_center =
        add_v2i(
                view->screen_center,
                pan_delta
               );
}
