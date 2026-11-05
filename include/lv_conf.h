#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565
// Size the TLSF allocator and its backing pool together. A 64 KiB-configured
// TLSF instance cannot accept a single 512 KiB pool added after startup.
#define LV_MEM_SIZE (512U * 1024U)
#define LV_MEM_POOL_INCLUDE <esp_heap_caps.h>
#define LV_MEM_POOL_ALLOC(size) heap_caps_malloc((size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
// A failed assertion must produce a panic/backtrace rather than spin silently.
#define LV_ASSERT_HANDLER_INCLUDE <stdlib.h>
#define LV_ASSERT_HANDLER abort();
// hud_font_vietnamese uses the font converter's compressed bitmap format.
#define LV_USE_FONT_COMPRESSED 1
#define LV_USE_TJPGD 1
#define LV_USE_FS_MEMFS 1
#define LV_FS_MEMFS_LETTER 'M'

#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_30 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_34 1
#define LV_FONT_MONTSERRAT_48 1

#endif
