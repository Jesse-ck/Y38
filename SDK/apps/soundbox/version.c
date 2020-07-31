#include "system/includes.h"
#include "generic/log.h"
#include "app_config.h"

_INLINE_
int app_version_check()
{
    lib_version_check();

#if (defined(CONFIG_FATFS_ENBALE) && CONFIG_FATFS_ENBALE)
    VERSION_CHECK(fatfs, FATFS_VERSION);
#endif

#ifdef SDFILE_VERSION
    VERSION_CHECK(sdfile, SDFILE_VERSION);
#endif
    return 0;
}


