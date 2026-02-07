#ifndef NK_STATE_H
#define NK_STATE_H

#include "nk.h"
#include <stdbool.h>

/**
 * nk_state_save - Save container state to disk
 * @container: Container to save
 *
 * Returns: 0 on success, -1 on error
 */
int nk_state_save(const nk_container_t *container);

/**
 * nk_state_load - Load container state from disk
 * @container_id: Container ID to load
 *
 * Returns: Container structure, or NULL on error
 */
nk_container_t *nk_state_load(const char *container_id);

/**
 * nk_state_delete - Delete container state from disk
 * @container_id: Container ID to delete
 *
 * Returns: 0 on success, -1 on error
 */
int nk_state_delete(const char *container_id);

/**
 * nk_state_exists - Check if container state exists
 * @container_id: Container ID to check
 *
 * Returns: true if state exists, false otherwise
 */
bool nk_state_exists(const char *container_id);

#endif /* NK_STATE_H */
