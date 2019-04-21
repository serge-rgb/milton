#undef main // SDL does things we don't want

#define INVALIDATE_COUNT(ptr, count) memset((u8*)(ptr), -1, sizeof(*(ptr)) * (count))

#define INVALIDATE(ptr) INVALIDATE_COUNT(ptr, 1)


#define COMPARE_BYTES_COUNT(ptrA, ptrB, count) \
    assert(sizeof(*(ptrA)) == sizeof(*(ptrB))), \
    compare_bytes((u8*)(ptrA), (u8*)(ptrB), sizeof(*(ptrA)) * (count) )

#define COMPARE_BYTES(ptrA, ptrB) \
    COMPARE_BYTES_COUNT(ptrA, ptrB, 1)

#define EXPECT_TRUE(expr) \
    if (!(expr)) { \
        failed_test(#expr); \
    }


void
failed_test(char* expr)
{
    fprintf(stderr, "Failed expression: [ %s ]\n", expr);
}

b32
compare_bytes(u8* bufA, u8* bufB, sz size)
{
    b32 equal = true;

    for (sz i = 0; i < size; ++i) {
        if (bufA[i] != bufB[i]) {
            equal = false;
            break;
        }
    }

    return equal;
}

void
test_save_load()
{
    Milton milton = {};

    PATH_CHAR* path = TO_PATH_STR("TEST_loading.mlt");

    milton_init(&milton, 0, 0, 1, path, MiltonInit_FOR_TEST);
    milton_reset_canvas_and_set_default(&milton);
    milton.persist->mlt_file_path = path;
    milton_save(&milton);

    Milton loaded_milton = {};

    milton_init(&loaded_milton, 0, 0, 1, TO_PATH_STR("TEST_loading.mlt"), MiltonInit_FOR_TEST);

    INVALIDATE(loaded_milton.view);
    loaded_milton.view->screen_size = milton.view->screen_size;

    INVALIDATE_COUNT(loaded_milton.brushes, 3);
    INVALIDATE_COUNT(loaded_milton.brush_sizes, 3); // We only save PEN, ERASER and PRIMITIVE

    milton_load(&loaded_milton);

    EXPECT_TRUE( COMPARE_BYTES(milton.view, loaded_milton.view) );

    EXPECT_TRUE( milton.canvas->layer_guid == loaded_milton.canvas->layer_guid );

    EXPECT_TRUE( COMPARE_BYTES_COUNT(milton.brushes, loaded_milton.brushes, BrushEnum_COUNT) );
    EXPECT_TRUE( COMPARE_BYTES_COUNT(milton.brush_sizes, loaded_milton.brush_sizes, BrushEnum_COUNT) );
}

extern "C" int
main()
{
    test_save_load();
    return 0;
}
