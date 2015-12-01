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


struct WorkerParams {
    MiltonState* milton_state;
    i32 worker_id;
};

// Declared here so that the workers get launched from the init function.
int renderer_worker_thread(/* WorkerParams* */void* data);

// Blocking function. When it returns, the framebuffer is updated to the
// current state. It does a lot of smart things to do as little work as
// possible. Most users on most machines should get interactive framerates.
void milton_render(MiltonState* milton_state, MiltonRenderFlags render_flags);

