// This file is part of Milton.
//
// Milton is free software: you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Milton is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
// more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Milton.  If not, see <http://www.gnu.org/licenses/>.


#pragma once

#include "common.h"

#include "milton.h"

#if defined(__cplusplus)
extern "C" {
#endif


void milton_load(MiltonState* milton_state);

void milton_save(MiltonState* milton_state);

void milton_save_buffer_to_file(char* fname, u8* buffer, i32 w, i32 h);


#if defined(__cplusplus)
}
#endif
