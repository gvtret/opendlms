/**
 * OpenDLMS reader — preset auth profiles for lab / SPODUS roles.
 */

#include "opendlms_reader_auth.h"

#include <string.h>

void opendlms_reader_auth_public(opendlms_reader_auth_t *auth)
{
    if (auth == NULL)
    {
        return;
    }

    memset(auth, 0, sizeof(*auth));
    auth->auth_level          = CSM_AUTH_LOWEST_LEVEL;
    auth->application_context = (uint8_t)LN_REF;
    auth->ciphering           = 0U;
}

void opendlms_reader_auth_reader(opendlms_reader_auth_t *auth)
{
    if (auth == NULL)
    {
        return;
    }

    memset(auth, 0, sizeof(*auth));
    auth->auth_level          = CSM_AUTH_LOW_LEVEL;
    auth->application_context = (uint8_t)LN_REF;
    auth->ciphering           = 0U;
}

void opendlms_reader_auth_configurator(opendlms_reader_auth_t *auth)
{
    if (auth == NULL)
    {
        return;
    }

    memset(auth, 0, sizeof(*auth));
    auth->auth_level              = CSM_AUTH_HIGH_LEVEL_GMAC;
    auth->ciphering               = 1U;
    auth->application_context     = (uint8_t)LN_REF_WITH_CYPHERING;
    auth->use_invocation_counter  = 0U;
    auth->ic_obis.A               = 0U;
    auth->ic_obis.B               = 0U;
    auth->ic_obis.C               = 43U;
    auth->ic_obis.D               = 1U;
    auth->ic_obis.E               = 0U;
    auth->ic_obis.F               = 255U;
    auth->ic_class_id             = 1U;
    auth->ic_attribute_id         = 2U;
}

void opendlms_reader_auth_configurator_plain(opendlms_reader_auth_t *auth)
{
    opendlms_reader_auth_configurator(auth);
    auth->ciphering           = 0U;
    auth->application_context = (uint8_t)LN_REF;
}
