////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2012 Laurent Gomila (laurent.gom@gmail.com)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

/* !!! THIS IS AN EXTREMELY ALTERED AND PURPOSE-BUILT VERSION OF SFML !!!
 * This distribution is designed to possess only a limited subset of the
 * original library's functionality and to only build on Win32 platforms.
 * The original distribution of this software has many more features and
 * supports more platforms.
 */

#ifndef SFML_CLOCK_HPP
#define SFML_CLOCK_HPP

namespace sf
{
class Clock {
public :
    // The clock starts automatically after being constructed.
    Clock();

    /* This function returns the time elapsed since the last call
     * to restart() (or the construction of the instance if restart()
     * has not been called).
     */
    unsigned int getElapsedTime() const;

    /* This function puts the time counter back to zero.
     * It also returns the time elapsed since the clock was started.
     */
    unsigned int restart();

private :
    static unsigned int getCurrentTime();

    unsigned int m_startTime; // Time of last reset, in milliseconds
};

} // namespace sf

#endif // SFML_CLOCK_HPP
