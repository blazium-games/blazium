/**************************************************************************/
/*  image_loader_gif.cpp                                                  */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "image_loader_gif.h"

#include "gif_decode.h"
#include "gif_encode.h"

#include "core/io/file_access.h"
#include "core/io/image.h"

static Ref<Image> _gif_mem_loader_func(const uint8_t *p_gif_data, int p_size) {
	Ref<Image> img;
	const Error err = gif_decode_first_frame(p_gif_data, p_size, img);
	ERR_FAIL_COND_V(err != OK, Ref<Image>());
	return img;
}

static Error _save_gif_func(const String &p_path, const Ref<Image> &p_img) {
	Vector<uint8_t> buffer;
	const Error err = gif_encode_still(p_img, buffer, true);
	ERR_FAIL_COND_V(err != OK, err);
	return gif_encode_write_file(p_path, buffer);
}

static Vector<uint8_t> _save_gif_buffer_func(const Ref<Image> &p_img) {
	Vector<uint8_t> buffer;
	if (gif_encode_still(p_img, buffer, true) != OK) {
		return Vector<uint8_t>();
	}
	return buffer;
}

Error ImageLoaderGIF::load_image(Ref<Image> p_image, Ref<FileAccess> f, BitField<ImageFormatLoader::LoaderFlags> p_flags, float p_scale) {
	ERR_FAIL_COND_V(p_image.is_null() || f.is_null(), ERR_INVALID_PARAMETER);
	const uint64_t len = f->get_length();
	ERR_FAIL_COND_V(len == 0 || len > 0x7FFFFFFF, ERR_FILE_CORRUPT);
	Vector<uint8_t> src;
	src.resize(int(len));
	f->get_buffer(src.ptrw(), len);
	Ref<Image> decoded;
	const Error err = gif_decode_first_frame(src.ptr(), src.size(), decoded);
	ERR_FAIL_COND_V(err != OK || decoded.is_null(), err == OK ? ERR_FILE_CORRUPT : err);
	p_image->copy_internals_from(decoded);
	return OK;
}

void ImageLoaderGIF::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("gif");
}

ImageLoaderGIF::ImageLoaderGIF() {
	Image::_gif_mem_loader_func = _gif_mem_loader_func;
	Image::save_gif_func = _save_gif_func;
	Image::save_gif_buffer_func = _save_gif_buffer_func;
}
