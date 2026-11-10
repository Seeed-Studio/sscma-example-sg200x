/* rebot-vision options for the patched apriltag in this directory (BSD-2-Clause, like the
 * files it patches; see apriltag.cmake). Zeroed options give upstream behaviour. */
#pragma once

#include <stdbool.h>

#include "apriltag.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rv_apriltag_options {
    /* Gradient clusters: skip a pixel whose right, lower and lower diagonal neighbours are
     * not of the opposite colour before looking up its union-find set (same result; upstream
     * looks the set up first). */
    bool lazy_clusters;
    /* Keep the union-find, the gradient-cluster hash map and the cluster point arrays from
     * frame to frame instead of allocating and freeing them per frame, and build the
     * clusters in one pass (nthreads == 1 only). Same markers; candidate quads may come in
     * another order. */
    bool reuse_buffers;
    /* Drop candidate quads whose shortest side (full-resolution pixels, before edge
     * refinement) is below this before refining and decoding them (0 = off). */
    double min_quad_side;
} rv_apriltag_options_t;

/* Options of a detector from apriltag_detector_create() / apriltag_detector_copy() of this
 * directory's apriltag.c: they are stored right after the detector. */
rv_apriltag_options_t *rv_apriltag_options(apriltag_detector_t *td);

/* Internal (apriltag.c / apriltag_quad_thresh.c): the buffers kept for reuse_buffers. */
struct rv_quad_scratch;
struct rv_quad_scratch **rv_apriltag_scratch(apriltag_detector_t *td);
void rv_quad_scratch_destroy(struct rv_quad_scratch *s);

#ifdef __cplusplus
}
#endif
