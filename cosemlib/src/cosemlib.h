/**
 * \file cosemlib.h
 * \brief OpenDLMS — DLMS/COSEM protocol stack (IEC 62056)
 *
 *  Umbrella header for the cosemlib library.
 *  Include this single header to access all public APIs.
 *
 *  Copyright (c) 2024, OpenDLMS contributors
 *  SPDX-License-Identifier: MIT
 */

#ifndef COSEMLIB_H
#define COSEMLIB_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── Core types and definitions ──────────────────────────────────────────── */
#include "csm_definitions.h"
#include "csm_array.h"

/* ── Channel / Association layer ─────────────────────────────────────────── */
#include "csm_channel.h"
#include "csm_association.h"

/* ── Services (GET/SET/ACTION) ───────────────────────────────────────────── */
#include "csm_services.h"

/* ── Block Transfer (GBT) ────────────────────────────────────────────────── */
#include "csm_block_transfer.h"

/* ── High-level Server / Client ──────────────────────────────────────────── */
#include "csm_server.h"

/* ── Version information ─────────────────────────────────────────────────── */
#define COSEMLIB_VERSION_MAJOR  1
#define COSEMLIB_VERSION_MINOR  1
#define COSEMLIB_VERSION_PATCH  0
#define COSEMLIB_VERSION_STRING "1.1.0"

#ifdef __cplusplus
}
#endif

#endif /* COSEMLIB_H */
