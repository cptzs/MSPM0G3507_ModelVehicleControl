#include "user_ui_internal.h"

/**
 * @file user_ui_legacy_pages.c
 * @brief Transitional wrapper for legacy UI page implementations.
 *
 * The old User_Application/user_ui.c still contains both the legacy page drawing
 * functions and the old USER_UI_Task() entry.  The new UI core defines its own
 * USER_UI_Task(), so the final IAR project switch must not compile the old file
 * directly together with UI/user_ui_core.c.
 *
 * This wrapper reuses the legacy page implementation while renaming only the old
 * task entry.  During the final basic.ewp update, replace User_Application/user_ui.c
 * with this file plus the new UI source files.  After all pages are migrated into
 * dedicated page files, this wrapper can be deleted.
 */
#define USER_UI_Task USER_UI_LegacyTask
#include "../user_ui.c"
#undef USER_UI_Task
