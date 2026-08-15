/**************************************************************************/
/*  export.hpp                                                            */
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

#pragma once

// DLL export/import macros for Windows shared library builds

#ifdef _WIN32
#ifdef TURNBATTLE_SDK_EXPORTS
#define TURNBATTLE_API __declspec(dllexport)
#elif defined(TURNBATTLE_SDK_IMPORTS)
#define TURNBATTLE_API __declspec(dllimport)
#else
#define TURNBATTLE_API
#endif
#else
// Unix-like systems: use visibility attributes
#if defined(__GNUC__) && __GNUC__ >= 4
#ifdef TURNBATTLE_SDK_EXPORTS
#define TURNBATTLE_API __attribute__((visibility("default")))
#else
#define TURNBATTLE_API
#endif
#else
#define TURNBATTLE_API
#endif
#endif
