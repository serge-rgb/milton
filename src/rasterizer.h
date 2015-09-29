//    Milton Paint
//    Copyright (C) 2015  Sergio Gonzalez
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

// TODO: There should be a way to deal with high density displays.

// We might want to split this file into
//  - Actual rasterization of implicitly defined geometry (including strokes.)
//  - Render jobs and spacial division
//  - Bitmap loading and resource management.

typedef struct
{
    MiltonState* milton_state;
    i32 worker_id;
} WorkerParams;

// Thread function.
int render_worker(void* data);

void milton_render(MiltonState* milton_state, MiltonRenderFlags render_flags);

