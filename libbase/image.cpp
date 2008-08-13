// image.cpp	-- Thatcher Ulrich <tu@tulrich.com> 2002

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Handy image utilities for RGB surfaces.

#include <cstring>
#include <memory>		// for auto_ptr
#include <boost/scoped_array.hpp>
#include <boost/shared_ptr.hpp>

#include "gnash.h" // for image file types
#include "image.h"
#include "GnashImage.h"
#include "GnashImagePng.h"
#include "GnashImageGif.h"
#include "GnashImageJpeg.h"
#include "IOChannel.h"
#include "log.h"

namespace gnash
{
namespace image
{
	//
	// image_base
	//

	/// Create an image taking ownership of the given buffer, supposedly of height*pitch bytes
	image_base::image_base(boost::uint8_t* data, int width, int height, int pitch, ImageType type)
		:
		_type(type),
		m_size(height*pitch),
		m_width(width),
		m_height(height),
		m_pitch(pitch),
		m_data(data)
	{
	}

	/// Create an image allocating a buffer of height*pitch bytes
	image_base::image_base(int width, int height, int pitch, ImageType type)
		:
		_type(type),
		m_size(height*pitch),
		m_width(width),
		m_height(height),
		m_pitch(pitch),
		m_data(new boost::uint8_t[m_size])
	{
		assert(pitch >= width);
	}

	void image_base::update(boost::uint8_t* data)
	{
		std::memcpy(m_data.get(), data, m_size);
	}

	void image_base::update(const image_base& from)
	{
		assert(from.m_pitch == m_pitch);
		assert(m_size <= from.m_size);
		assert(_type == from._type);
		std::memcpy(m_data.get(), const_cast<image_base&>(from).data(), m_size);
	}

    void image_base::clear(const boost::uint8_t byteValue)
    {
        std::memset(m_data.get(), byteValue, m_size);
    }

	boost::uint8_t* image_base::scanline(size_t y)
	{
		assert(y < m_height);
		return m_data.get() + m_pitch * y;
	}


	//
	// rgb
	//

	rgb::rgb(int width, int height)
		:
		image_base( width, height,
			(width * 3 + 3) & ~3, // round pitch up to nearest 4-byte boundary
			GNASH_IMAGE_RGB)
	{
		assert(width > 0);
		assert(height > 0);
		assert(m_pitch >= m_width * 3);
		assert((m_pitch & 3) == 0);
	}

	rgb::~rgb()
	{
	}


	//
	// rgba
	//


	rgba::rgba(int width, int height)
		:
		image_base(width, height, width * 4, GNASH_IMAGE_RGBA)
	{
		assert(width > 0);
		assert(height > 0);
		assert(m_pitch >= m_width * 4);
		assert((m_pitch & 3) == 0);
	}

	rgba::~rgba()
	{
	}


	void rgba::set_pixel(size_t x, size_t y, boost::uint8_t r, boost::uint8_t g, boost::uint8_t b, boost::uint8_t a)
	// Set the pixel at the given position.
	{
		assert(x < m_width);
		assert(y < m_height);

		boost::uint8_t*	data = scanline(y) + 4 * x;

		data[0] = r;
		data[1] = g;
		data[2] = b;
		data[3] = a;
	}


    void rgba::mergeAlpha(const boost::uint8_t* alphaData, const size_t bufferLength)
    {
        assert (bufferLength * 4 <= m_size);

        for (size_t i = 0; i < bufferLength; i++) {
            m_data[4 * i + 3] = alphaData[i];
        }
    }

	//
	// alpha
	//


	alpha::alpha(int width, int height)
		:
		image_base(width, height, width, GNASH_IMAGE_ALPHA)
	{
		assert(width > 0);
		assert(height > 0);
	}


	alpha::~alpha()
	{
	}

	//
	// utility
	//

	// Write the given image to the given out stream, in jpeg format.
	void writeImageData(FileType type, boost::shared_ptr<IOChannel> out, image::image_base* image, int quality)
	{
		
		const size_t width = image->width();
		const size_t height = image->height();
				
		std::auto_ptr<ImageOutput> outChannel;

        switch (type)
        {
            case GNASH_FILETYPE_PNG:
                outChannel = PngImageOutput::create(out, width, height, quality);
                break;
            case GNASH_FILETYPE_JPEG:
                outChannel = JpegImageOutput::create(out, width, height, quality);
                break;
            default:
                log_error("Requested to write image as unsupported filetype");
                break;
        }

        switch (image->type())
        {
            case GNASH_IMAGE_RGB:
                outChannel->writeImageRGB(image->data());
                break;
            case GNASH_IMAGE_RGBA:
                outChannel->writeImageRGBA(image->data());
                break;
            default:
                break;
        }

	}

    // See gnash.h for file types.
    std::auto_ptr<rgb> readImageData(boost::shared_ptr<IOChannel> in, FileType type)
    {
        std::auto_ptr<rgb> im (NULL);
        std::auto_ptr<ImageInput> inChannel;

        switch (type)
        {
            case GNASH_FILETYPE_PNG:
                inChannel = PngImageInput::create(in);
                break;
            case GNASH_FILETYPE_GIF:
                inChannel = GifImageInput::create(in);
                break;
            case GNASH_FILETYPE_JPEG:
                inChannel = JpegImageInput::create(in);
                break;
            default:
                break;
        }
        
        if (!inChannel.get()) return im;
        
        const size_t height = inChannel->getHeight();
        const size_t width = inChannel->getWidth();
        
        im.reset(new image::rgb(width, height));
        
        for (size_t i = 0; i < height; ++i)
        {
            inChannel->readScanline(im->scanline(i));
        }
        return im;
    }

	std::auto_ptr<rgb> readSWFJpeg2WithTables(JpegImageInput* j_in)
	// Create and read a new image, using a input object that
	// already has tables loaded.  The IJG documentation describes
	// this as "abbreviated" format.
	{
		assert(j_in);

		j_in->startImage();

		std::auto_ptr<rgb> im(new image::rgb(j_in->getWidth(), j_in->getHeight()));

		for (size_t y = 0; y < j_in->getHeight(); y++) {
			j_in->readScanline(im->scanline(y));
		}

		j_in->finishImage();

		return im;
	}


	// For reading SWF JPEG3-style image data, like ordinary JPEG, 
	// but stores the data in rgba format.
	std::auto_ptr<rgba> readSWFJpeg3(boost::shared_ptr<gnash::IOChannel> in)
	{
	
	    std::auto_ptr<rgba> im(NULL);

        // Calling with headerBytes as 0 has a special effect...
		std::auto_ptr<JpegImageInput> j_in ( JpegImageInput::createSWFJpeg2HeaderOnly(in, 0) );
		if ( ! j_in.get() ) return im;
		
		j_in->startImage();

		im.reset(new image::rgba(j_in->getWidth(), j_in->getHeight()));

		boost::scoped_array<boost::uint8_t> line ( new boost::uint8_t[3*j_in->getWidth()] );

		for (size_t y = 0; y < j_in->getHeight(); y++) 
		{
			j_in->readScanline(line.get());

			boost::uint8_t*	data = im->scanline(y);
			for (size_t x = 0; x < j_in->getWidth(); x++) 
			{
				data[4*x+0] = line[3*x+0];
				data[4*x+1] = line[3*x+1];
				data[4*x+2] = line[3*x+2];
				data[4*x+3] = 255;
			}
		}

		j_in->finishImage();

		return im;
	}

} // namespace image
} // namespace gnash

// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
