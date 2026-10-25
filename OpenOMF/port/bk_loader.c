#include <aster_imports.h>
#include "resources/bk_loader.h"
#include "formats/bk.h"
#include "formats/error.h"
#include "utils/log.h"

int load_bk_file(bk *b, const path *filename) {
    uint32_t began=aster_time_now_ms();aster_printf("OMF LOAD BK begin %s",path_c(filename));
    // Load up BK file from libSD
    sd_bk_file tmp;
    sd_bk_create(&tmp);
    int result=sd_bk_load(&tmp, filename);
    if(result != SD_SUCCESS) {
        log_error("Original BK loader rc=%d (%s) file=%s", result, sd_get_error(result), path_c(filename));
        sd_bk_free(&tmp);
        return 1;
    }

    str fn;
    path_stem(filename, &fn);

    // Convert
    aster_printf("OMF LOAD BK parsed_ms=%u",aster_time_now_ms()-began);
    bk_create(b, &tmp, &fn);
    sd_bk_free(&tmp);
    str_free(&fn);
    aster_printf("OMF LOAD BK complete_ms=%u",aster_time_now_ms()-began);
    return 0;
}
