/**************************************************************************/
/*  evidence_writer.cpp                                                   */
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

#include "io/evidence_writer.h"

#include "core/io/file_access.h"
#include "core/io/json.h"

static String dict_to_md(const Dictionary &p_scan) {
	String md = "# Obfuscation evidence\n\n";
	md += vformat("- signature_valid: %s\n", bool(p_scan.get("signature_valid", false)) ? "true" : "false");
	md += vformat("- score: %s\n", String::num((double)p_scan.get("score", 0.0), 3));
	md += vformat("- matched/total: %d/%d\n", (int)p_scan.get("matched", 0), (int)p_scan.get("total", 0));
	md += vformat("- owner_id: %s\n\n", String(p_scan.get("owner_hex", "")));
	md += "| path | kind | matched |\n|---|---|---|\n";
	Array marks = p_scan.get("marks", Array());
	for (int i = 0; i < marks.size(); i++) {
		Dictionary m = marks[i];
		md += vformat("| %s | %s | %s |\n", String(m.get("path", "")), String(m.get("kind", "")), bool(m.get("matched", false)) ? "yes" : "no");
	}
	return md;
}

static PackedByteArray simple_pdf(const String &p_text) {
	String escaped;
	for (int i = 0; i < p_text.length() && i < 4000; i++) {
		char32_t c = p_text[i];
		if (c == '(' || c == ')' || c == '\\') {
			escaped += "\\";
		}
		if (c == '\n') {
			escaped += "\\n";
		} else if (c >= 32 && c < 127) {
			escaped += String::chr(c);
		} else {
			escaped += " ";
		}
	}
	String stream = "BT /F1 10 Tf 48 750 Td (" + escaped + ") Tj ET";
	PackedByteArray stream_b = stream.to_utf8_buffer();
	String pdf = "%PDF-1.4\n";
	pdf += "1 0 obj << /Type /Catalog /Pages 2 0 R >> endobj\n";
	pdf += "2 0 obj << /Type /Pages /Kids [3 0 R] /Count 1 >> endobj\n";
	pdf += "3 0 obj << /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >> endobj\n";
	pdf += vformat("4 0 obj << /Length %d >> stream\n", stream_b.size());
	pdf += stream + "\nendstream endobj\n";
	pdf += "5 0 obj << /Type /Font /Subtype /Type1 /BaseFont /Courier >> endobj\n";
	pdf += "xref\n0 6\n0000000000 65535 f \ntrailer << /Size 6 /Root 1 0 R >>\nstartxref\n0\n%%EOF\n";
	return pdf.to_utf8_buffer();
}

Error ObfuscationEvidenceWriter::write(const Dictionary &p_scan, const String &p_path) {
	String ext = p_path.get_extension().to_lower();
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		return ERR_CANT_OPEN;
	}
	if (ext == "json") {
		f->store_string(JSON::stringify(p_scan, "\t"));
	} else if (ext == "pdf") {
		PackedByteArray pdf = simple_pdf(dict_to_md(p_scan));
		f->store_buffer(pdf);
	} else {
		f->store_string(dict_to_md(p_scan));
	}
	return OK;
}
