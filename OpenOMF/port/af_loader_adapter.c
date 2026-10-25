#include <aster_imports.h>
#include "resources/af_loader.h"

#include "formats/af.h"
#include "formats/error.h"
#include "resources/ids.h"
#include "resources/resource_files.h"

int load_af_file(af *a, int id) {
    // Get directory + filename
    const path filename = get_resource_filename(get_resource_file(id));

    str fn;
    path_stem(&filename, &fn);

    uint32_t began=aster_time_now_ms();aster_printf("OMF LOAD AF begin %s",path_c(&filename));
    // Load up AF file from libSD
    sd_af_file tmp;
    sd_af_create(&tmp);
    if(sd_af_load(&tmp, &filename) != SD_SUCCESS) {
        sd_af_free(&tmp);
        str_free(&fn);
        return 1;
    }

    // Convert
    aster_printf("OMF LOAD AF parsed_ms=%u",aster_time_now_ms()-began);
    af_create(a, &tmp, &fn);
    sd_af_free(&tmp);
    str_free(&fn);
    aster_printf("OMF LOAD AF complete_ms=%u",aster_time_now_ms()-began);
    return 0;
}
