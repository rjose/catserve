#include "context.h"
#include "errors.h"


/*==============================================================================
 * Public API
 */

/*------------------------------------------------------------------------------
 * Locks main mutex in context
 */
void
lock_main(Context *ctx)
{
        if (pthread_mutex_lock(ctx->main_mutex) != 0)
                pthread_failure(__FILE__, __LINE__);
}

/*------------------------------------------------------------------------------
 * Unlocks main mutex in context
 */
void
unlock_main(Context *ctx)
{
        if (pthread_mutex_unlock(ctx->main_mutex) != 0)
                pthread_failure(__FILE__, __LINE__);
}

