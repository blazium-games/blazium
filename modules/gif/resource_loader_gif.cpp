/**************************************************************************/
/*  resource_loader_gif.cpp                                               */
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

#include "resource_loader_gif.h"

#include "gif_animation.h"

Ref<Resource> ResourceFormatLoaderGIF::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, ResourceFormatLoader::CacheMode p_cache_mode) {
	Ref<GIFAnimation> anim;
	anim.instantiate();
	const Error err = anim->load_from_path(p_path);
	if (r_error) {
		*r_error = err;
	}
	ERR_FAIL_COND_V(err != OK, Ref<Resource>());
	return anim;
}

void ResourceFormatLoaderGIF::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("gif");
}

bool ResourceFormatLoaderGIF::handles_type(const String &p_type) const {
	return p_type == "GIFAnimation" || p_type == "Resource";
}

String ResourceFormatLoaderGIF::get_resource_type(const String &p_path) const {
	if (p_path.get_extension().to_lower() == "gif") {
		return "GIFAnimation";
	}
	return "";
}
