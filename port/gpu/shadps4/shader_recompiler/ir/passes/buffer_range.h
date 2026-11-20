// SPDX-FileCopyrightText: Copyright 2026 inFAMOUS PC port
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"

// tlg: Task 8g. GCN's (GFX7, PS4) range check for MUBUF/MTBUF accesses, from the V# the game
// passed. Out of range: loads return 0, stores and atomics are dropped. Sources: Mesa
// si_make_buffer_descriptor (GFX6-7 NUM_RECORDS: bytes if STRIDE == 0, else units of STRIDE used
// with IDXEN), ac_descriptors.c (the structured check "(index >= NUM_RECORDS) || (offset >=
// STRIDE)", RADV's "offset >= stride on GFX6-7" fixes), and the CLRX GCN memory notes (raw:
// "BUFOFFSET >= NUMRECORDS - SGPR_OFFSET"). MoltenVK has no robustBufferAccess2, so on macOS the
// shader applies this itself (resource_patching_pass.cpp).
namespace Shader::Optimization::BufferRange {

/// The address terms of one access, as GCN forms them: index = vgpr index (idxen),
/// offset = inst_offset + vgpr offset (offen), soffset = the SGPR offset (added to the base).
struct Access {
    bool idxen;
    bool add_tid;
    bool swizzle;
    u32 stride;
    u32 num_records;
    u32 index;
    u32 offset;
    u32 soffset;
};

/// Scratch (ADD_TID) and swizzled buffers are not checked: their GFX7 rules are not covered by
/// the sources above.
inline bool Checked(const Access& a) {
    return !a.add_tid && !a.swizzle;
}

inline bool OutOfRange(const Access& a) {
    if (!Checked(a)) {
        return false;
    }
    if (a.stride == 0) {
        // Raw: NUM_RECORDS in bytes; the SGPR offset counts, the index does not (times 0).
        return u64(a.offset) + a.soffset >= a.num_records;
    }
    if (a.idxen) {
        // Structured: the element index against NUM_RECORDS, the offset inside the element
        // against the stride.
        return a.index >= a.num_records || a.offset >= a.stride;
    }
    // shortcut: STRIDE != 0 without IDXEN is not described by the sources; treated as raw over
    // NUM_RECORDS elements of STRIDE bytes (the Vulkan binding's size), upgrade if a shader needs.
    return u64(a.offset) + a.soffset >= u64(a.num_records) * a.stride;
}

/// Marks an out-of-range access in the address operand (units of the access size; no real
/// binding reaches it). The SPIR-V emitter reads 0 and drops writes for it.
constexpr u32 OutOfRangeAddress = 0xFFFFFFFFu;

} // namespace Shader::Optimization::BufferRange
