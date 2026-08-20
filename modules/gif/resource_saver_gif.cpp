/**************************************************************************/
/*  resource_saver_gif.cpp                                                */
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

#include "resource_saver_gif.h"

#include "gif_animation.h"
#include "gif_encode.h"

#include "core/io/image.h"
#include "scene/resources/image_texture.h"

Error ResourceSaverGIF::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	Ref<GIFAnimation> anim = p_resource;
	if (anim.is_valid()) {
		return anim->save_to_path(p_path);
	}

	Ref<Image> img = p_resource;
	if (img.is_null()) {
		Ref<ImageTexture> tex = p_resource;
		ERR_FAIL_COND_V_MSG(tex.is_null(), ERR_INVALID_PARAMETER, "Can't save this resource as GIF.");
		img = tex->get_image();
	}
	ERR_FAIL_COND_V_MSG(img.is_null() || img->is_empty(), ERR_INVALID_PARAMETER, "Can't save empty image as GIF.");
	Vector<uint8_t> buffer;
	const Error err = gif_encode_still(img, buffer, true);
	ERR_FAIL_COND_V(err != OK, err);
	return gif_encode_write_file(p_path, buffer);
}

bool ResourceSaverGIF::recognize(const Ref<Resource> &p_resource) const {
	return p_resource.is_valid() && (p_resource->is_class("GIFAnimation") || p_resource->is_class("Image") || p_resource->is_class("ImageTexture"));
}

void ResourceSaverGIF::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (recognize(p_resource)) {
		p_extensions->push_back("gif");
	}
}
