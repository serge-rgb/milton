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


// NOTE
//  Since we are using NSEvent to get tablet data, we are forcing OSX >= 10.4

#ifdef __cplusplus
extern "C" {
#endif

void     milton_osx_tablet_hook(/* NSEvent* */void* event);
float*   milton_osx_poll_pressures(int* out_num_pressures);

#ifdef __cplusplus
}
#endif
