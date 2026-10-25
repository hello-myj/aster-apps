#include "resources/resource_paths.h"
#include <aster_imports.h>
path get_config_dir(void){path p;path_from_c(&p,"/data/config");return p;}
path get_state_dir(void){path p;path_from_c(&p,"/data/state");return p;}
path get_resource_dir(void){path p;path_from_c(&p,"/data");return p;}
bool resource_path_init(void){return true;}
void resource_path_create_dirs(void){aster_fs_mkdir("/data/config",12);aster_fs_mkdir("/data/state",11);}
