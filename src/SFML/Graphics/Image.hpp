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

#ifndef SFML_IMAGE_HPP
#define SFML_IMAGE_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Color.hpp"
#include "../System/Vector2.hpp"
#include <cstdint>
#include <string>
#include <vector>


namespace sf
{

////////////////////////////////////////////////////////////
/// \brief Class for loading, manipulating and saving images
///
////////////////////////////////////////////////////////////
class Image
{
public :

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    /// Creates an empty image.
    ///
    ////////////////////////////////////////////////////////////
    Image();

    ////////////////////////////////////////////////////////////
    /// \brief Create the image from an array of pixels
    ///
    /// The \a pixel array is assumed to contain 32-bits RGBA pixels,
    /// and have the given \a width and \a height. If not, this is
    /// an undefined behavior.
    /// If \a pixels is null, an empty image is created.
    ///
    /// \param width  Width of the image
    /// \param height Height of the image
    /// \param pixels Array of pixels to copy to the image
    ///
    ////////////////////////////////////////////////////////////
    void create(unsigned int width, unsigned int height, const uint8_t* pixels);

    ////////////////////////////////////////////////////////////
    /// \brief Load the image from a file in memory
    ///
    /// The supported image formats are bmp, png, tga, jpg, gif,
    /// psd, hdr and pic. Some format options are not supported,
    /// like progressive jpeg.
    /// If this function fails, the image is left unchanged.
    ///
    /// \param data Pointer to the file data in memory
    /// \param size Size of the data to load, in bytes
    ///
    /// \return True if loading was successful
    ///
    /// \see loadFromFile, loadFromStream
    ///
    ////////////////////////////////////////////////////////////
    bool loadFromMemory(const void* data, std::size_t dataSize);

    ////////////////////////////////////////////////////////////
    /// \brief Save the image to a file on disk
    ///
    /// The format of the image is automatically deduced from
    /// the extension. The supported image formats are bmp, png,
    /// tga and jpg. The destination file is overwritten
    /// if it already exists. This function fails if the image is empty.
    ///
    /// \param filename Path of the file to save
    ///
    /// \return True if saving was successful
    ///
    /// \see create, loadFromFile, loadFromMemory
    ///
    ////////////////////////////////////////////////////////////
    bool saveToFile(const std::string& filename) const;

    ////////////////////////////////////////////////////////////
    /// \brief Return the size of the image
    ///
    /// \return Size in pixels
    ///
    ////////////////////////////////////////////////////////////
    Vector2<unsigned int> getSize() const;

    ////////////////////////////////////////////////////////////
    /// \brief Change the color of a pixel
    ///
    /// This function doesn't check the validity of the pixel
    /// coordinates, using out-of-range values will result in
    /// an undefined behavior.
    ///
    /// \param x     X coordinate of pixel to change
    /// \param y     Y coordinate of pixel to change
    /// \param color New color of the pixel
    ///
    /// \see getPixel
    ///
    ////////////////////////////////////////////////////////////
    void setPixel(unsigned int x, unsigned int y, const Color& color);

    ////////////////////////////////////////////////////////////
    /// \brief Get the color of a pixel
    ///
    /// This function doesn't check the validity of the pixel
    /// coordinates, using out-of-range values will result in
    /// an undefined behaviour.
    ///
    /// \param x X coordinate of pixel to get
    /// \param y Y coordinate of pixel to get
    ///
    /// \return Color of the pixel at coordinates (x, y)
    ///
    /// \see setPixel
    ///
    ////////////////////////////////////////////////////////////
    Color getPixel(unsigned int x, unsigned int y) const;

    ////////////////////////////////////////////////////////////
    /// \brief Get a read-only pointer to the array of pixels
    ///
    /// The returned value points to an array of RGBA pixels made of
    /// 8 bits integers components. The size of the array is
    /// width * height * 4 (getSize().x * getSize().y * 4).
    /// Warning: the returned pointer may become invalid if you
    /// modify the image, so you should never store it for too long.
    /// If the image is empty, a null pointer is returned.
    ///
    /// \return Read-only pointer to the array of pixels
    ///
    ////////////////////////////////////////////////////////////
    const uint8_t* getPixelsPtr() const;

private :

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    Vector2<unsigned int> m_size;  ///< Image size
    std::vector<uint8_t> m_pixels; ///< Pixels of the image
};

} // namespace sf


#endif // SFML_IMAGE_HPP
