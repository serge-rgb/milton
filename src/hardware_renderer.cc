// Copyright (c) 2015-2016 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

#if defined(__cplusplus)
extern "C" {
#endif

// Milton GPU renderer.
//
// Current Plan -- Draw a point (GL_POINTS) per point per stroke to develop the
// vertex shader. Make sure that zooming and panning works
//
//
//
// Rough outline for the future:
//
// The vertex shader will rasterize bounding slabs for each segment of the stroke
//  a    b      c    d
// o-----o------o----o (a four point stroke)
//
//    ___
// a|  / | b   the stroke gets drawn within slabs ab, bc, cd,
//  |/__|
//
// There will be overlap with each consecutive slab, so a problem to overcome
// will be overdraw. It is looking like the anti-aliasing solution for the GPU
// renderer will be simpler than the software renderer and it won't affect the
// rendering algorithm, but it is too early to tell.
//
// The vertex shader.
//
//      The slabs get updated, and they get interpolated onto the pixel shader.
//      Most of the hard work is in clipping and sending less work than is
//      necessary. Look into glMultiDraw* to send multiple VBOs.
//
//
// The pixel shader.
//
//      - To avoid overdraw, more than one option?
//          - Using the stencil buffer with one byte. Increment per stroke.
//          Early-out if stencil value equals current [0,255] value.
//          - ?? More ideas?
//      - Check distance to line. (1) get closest point. (2) euclidean dist.
//      (3) brush function
//      - If it is inside, blend color.
//
// Sandwich buffers.
//
//      Most of the time, we only need to update the working stroke.
//      Use cases.
//          - Painting. Update the working stroke. Layers above and below are the same.
//          - Panning: most of the screen can be copied!
//          - Zooming: Everything must be updated.
//          - Toggle layer visibility: ???
//
//      To start, always redraw everything.
//      Then, start keeping track of sandwich layers... do it incrementally.
//
//

bool hw_renderer_init()
{

}

#if defined(__cplusplus)
}
#endif
