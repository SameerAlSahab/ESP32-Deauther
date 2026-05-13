/**
 * @file management_helper.h
 * @author Sameer Al Sahab (sameeralsahab54@gmail.com)
 * @date 2026-02-03
 * @copyright Copyright (c) 2026
 *
 * @brief helpers for other utils.
 */

#ifndef MANAGEMENT_HELPER_H
#define MANAGEMENT_HELPER_H

#ifdef __cplusplus
extern "C" {
    #endif

    char *load_html_from_spiffs(const char *path);
    void restore_management_system(void);

    #ifdef __cplusplus
}
#endif

#endif
