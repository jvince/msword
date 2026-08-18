#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <climits>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

extern "C" int OpusModernDocxToRtfFile(const char*, const char*);
extern "C" int OpusModernDocxToTextFile(const char*, const char*);
extern "C" int OpusModernRtfFileToDocx(const char*, const char*);
extern "C" int OpusModernOdtToRtfFile(const char*, const char*);
extern "C" int OpusModernOdtToTextFile(const char*, const char*);
extern "C" int OpusModernRtfFileToOdt(const char*, const char*);
extern "C" int OpusModernPathIsOdt(const char*);
extern "C" int OpusModernRtfFileToPdf(const char*, const char*);
extern "C" int OpusModernBindPendingDocxUnicode(int);
extern "C" unsigned int OpusUnicodeScalarAt(int, long);
extern "C" int OpusPdfSnapshotBegin(int, int, int, int, int, int);
extern "C" int OpusPdfSnapshotAddParagraph(int, int, int, int, int, int,
                                            int, int, int, int, int);
extern "C" int OpusPdfSnapshotAddRun(const char*, int, const char*, int,
                                      int, int, int, int, int, int, int, int);

std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

std::string decode_base64(const char* encoded) {
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    unsigned accumulator = 0;
    int bits = 0;
    for (const unsigned char character : std::string(encoded)) {
        if (character == '=') break;
        const char* found = std::strchr(alphabet, character);
        if (found == nullptr) continue;
        accumulator = (accumulator << 6) |
            static_cast<unsigned>(found - alphabet);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            output.push_back(static_cast<char>((accumulator >> bits) & 0xff));
        }
    }
    return output;
}

int main(const int argument_count, char** arguments) {
    if (argument_count == 4 && std::strcmp(arguments[1], "--text") == 0) {
        const bool odt = OpusModernPathIsOdt(arguments[2]) != 0;
        const bool converted = (odt ?
            OpusModernOdtToTextFile(arguments[2], arguments[3]) :
            OpusModernDocxToTextFile(arguments[2], arguments[3])) != 0;
        std::cout << (odt ? "ODT" : "DOCX") << " text import "
                  << (converted ? "passed" : "failed") << '\n';
        return converted ? 0 : 4;
    }
    if (argument_count == 3) {
        const bool odt = OpusModernPathIsOdt(arguments[1]) != 0;
        const bool converted = (odt ?
            OpusModernOdtToRtfFile(arguments[1], arguments[2]) :
            OpusModernDocxToRtfFile(arguments[1], arguments[2])) != 0;
        std::cout << (odt ? "ODT" : "DOCX") << " import "
                  << (converted ? "passed" : "failed")
                  << '\n';
        return converted ? 0 : 3;
    }
    if (argument_count != 1) {
        std::cerr << "usage: opus_modern_formats_test [input.docx output.rtf]\n";
        return 1;
    }
    char temporary[MAX_PATH]{};
    char seed[MAX_PATH]{};
    if (GetTempPathA(MAX_PATH, temporary) == 0 ||
        GetTempFileNameA(temporary, "OWF", 0, seed) == 0) {
        return 1;
    }
    DeleteFileA(seed);
    const std::string base = seed;
    const std::string rtf = base + ".rtf";
    const std::string docx = base + ".docx";
    const std::string odt = base + ".odt";
    const std::string roundtrip = base + ".roundtrip.rtf";
    const std::string odt_roundtrip = base + ".odt-roundtrip.rtf";
    const std::string imported_text = base + ".unicode.txt";
    const std::string odt_imported_text = base + ".odt-unicode.txt";
    const std::string compressed_odt = base + ".compressed.odt";
    const std::string compressed_rtf = base + ".compressed.rtf";
    const std::string oversized = base + ".oversized.rtf";
    const std::string preserved = base + ".preserved.pdf";
    char requested_pdf[32768]{};
    const DWORD requested_pdf_length = GetEnvironmentVariableA(
        "WORD1_TEST_KEEP_PDF", requested_pdf,
        static_cast<DWORD>(std::size(requested_pdf)));
    const bool keep_pdf = requested_pdf_length > 0 &&
                          requested_pdf_length < std::size(requested_pdf);
    const std::string pdf = keep_pdf ? requested_pdf : base + ".pdf";
    DeleteFileA(pdf.c_str());
    {
        std::ofstream output(rtf, std::ios::binary);
        output << "{\\rtf1\\ansi{\\fonttbl{\\f0 Arial;}}"
                  "{\\colortbl;\\red255\\green0\\blue0;}"
                  "\\f0\\fs24 Plain {\\b Bold} {\\i Italic} "
                  "{\\ul Underline} {\\cf1 Red} "
                  "{\\lang1049 \\u1055?\\u1088?\\u1080?\\u1074?\\u1077?\\u1090?}"
                  " {\\lang1032 \\u915?\\u949?\\u953?\\u940?}"
                  " {\\lang1025 \\u1605?\\u1585?\\u1581?\\u1576?\\u1575?}"
                  " {\\lang1041 \\u12371?\\u12435?\\u12395?\\u12385?\\u12399?}"
                  "\\par Second paragraph}";
    }
    const bool written = OpusModernRtfFileToDocx(rtf.c_str(), docx.c_str()) != 0;
    const bool read = written &&
        OpusModernDocxToRtfFile(docx.c_str(), roundtrip.c_str()) != 0;
    const bool unicode_imported = written &&
        OpusModernDocxToTextFile(docx.c_str(), imported_text.c_str()) != 0 &&
        OpusModernBindPendingDocxUnicode(42) != 0;
    bool cyrillic_sidecar = false;
    bool greek_sidecar = false;
    bool arabic_sidecar = false;
    bool japanese_sidecar = false;
    if (unicode_imported) {
        const std::string imported_bytes = read_file(imported_text);
        for (long cp = 0; cp < static_cast<long>(imported_bytes.size()); ++cp) {
            const unsigned int scalar = OpusUnicodeScalarAt(42, cp);
            if (scalar == 1055) cyrillic_sidecar = true;
            if (scalar == 915) greek_sidecar = true;
            if (scalar == 1605) arabic_sidecar = true;
            if (scalar == 12371) japanese_sidecar = true;
        }
    }
    const bool odt_written = OpusModernRtfFileToOdt(
        rtf.c_str(), odt.c_str()) != 0;
    const bool odt_read = odt_written &&
        OpusModernOdtToRtfFile(odt.c_str(), odt_roundtrip.c_str()) != 0;
    const bool odt_unicode_imported = odt_written &&
        OpusModernOdtToTextFile(odt.c_str(), odt_imported_text.c_str()) != 0 &&
        OpusModernBindPendingDocxUnicode(43) != 0;
    bool odt_cyrillic_sidecar = false;
    bool odt_greek_sidecar = false;
    bool odt_arabic_sidecar = false;
    bool odt_japanese_sidecar = false;
    if (odt_unicode_imported) {
        const std::string imported_bytes = read_file(odt_imported_text);
        for (long cp = 0; cp < static_cast<long>(imported_bytes.size()); ++cp) {
            const unsigned int scalar = OpusUnicodeScalarAt(43, cp);
            if (scalar == 1055) odt_cyrillic_sidecar = true;
            if (scalar == 915) odt_greek_sidecar = true;
            if (scalar == 1605) odt_arabic_sidecar = true;
            if (scalar == 12371) odt_japanese_sidecar = true;
        }
    }
    static constexpr const char* compressed_odt_base64 =
        "UEsDBBQAAAAAADKSC11exjIMJwAAACcAAAAIAAAAbWltZXR5cGVhcHBsaWNhdGlvbi92bmQub2FzaXMub3BlbmRvY3VtZW50LnRl"
        "eHRQSwMEFAAAAAgAMpILXfNZG7MaAQAAmwIAAAsAAABjb250ZW50LnhtbI1STW+EIBD9K4SerdtbQ4A9tPHaQ7c/ABEtiTKGwVb/"
        "fUFc6x42WQ4wH+/Nm5nAz/PQkx/j0YIT9OX5RIlxGhrrOkG/LlXxSs+SQ9tabVgDehqMC4UGF+JLItkhy1lBJ+8YKLTInBoMsqAZ"
        "jMZdWeyIZqtUjgQzh0fZCXvkYlj6h6VX8JHdwqPUGfuihTj4MKpg663Mvhk1BRhiQherBkqetdabZDuVF/QzeHAd3WKtGmy/CJrG"
        "oldScorRxy58sAZJC7HRuPVfY7vvuKka+oamqIYevKBPVXWKh5aSlwfZ6N3tbkvU0Cy7k2QlXxc8yrc4qDeIpiE5hKNyJFupRnEz"
        "jfx4v8QkBl7uYLnZ438fWaG8ES/v/C35B1BLAQIUABQAAAAAADKSC11exjIMJwAAACcAAAAIAAAAAAAAAAAAAACAAQAAAABtaW1l"
        "dHlwZVBLAQIUABQAAAAIADKSC13zWRuzGgEAAJsCAAALAAAAAAAAAAAAAACAAU0AAABjb250ZW50LnhtbFBLBQYAAAAAAgACAG8A"
        "AACQAQAAAAA=";
    {
        std::ofstream output(compressed_odt, std::ios::binary);
        output << decode_base64(compressed_odt_base64);
    }
    const bool compressed_odt_read =
        OpusModernOdtToRtfFile(compressed_odt.c_str(),
                               compressed_rtf.c_str()) != 0;
    const std::string compressed_result = compressed_odt_read ?
        read_file(compressed_rtf) : "";
    const bool pdf_written = OpusModernRtfFileToPdf(rtf.c_str(), pdf.c_str()) != 0;
    {
        std::ofstream output(preserved, std::ios::binary);
        output << "ORIGINAL";
    }
    HANDLE oversized_file = CreateFileA(
        oversized.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY, nullptr);
    bool oversized_created = oversized_file != INVALID_HANDLE_VALUE;
    if (oversized_created) {
        LARGE_INTEGER hostile_size{};
        hostile_size.QuadPart = 64ll * 1024ll * 1024ll + 1;
        oversized_created = SetFilePointerEx(oversized_file, hostile_size,
                                             nullptr, FILE_BEGIN) &&
                             SetEndOfFile(oversized_file);
        CloseHandle(oversized_file);
    }
    const bool oversized_rejected = oversized_created &&
        OpusModernRtfFileToPdf(oversized.c_str(), preserved.c_str()) == 0 &&
        read_file(preserved) == "ORIGINAL";
    const bool invalid_snapshot_rejected =
        OpusPdfSnapshotBegin(INT_MAX, INT_MAX, INT_MAX, INT_MAX,
                             INT_MAX, INT_MAX) == 0 &&
        OpusPdfSnapshotBegin(12240, 15840, 1440, 1440, 1440, 1440) != 0 &&
        OpusPdfSnapshotAddParagraph(0, 0, 0, 0, 0, 0, 0,
                                    0, 0, 0, 0) != 0 &&
        OpusPdfSnapshotAddParagraph(0, 0, 0, 0, 0, 0, -240,
                                    0, 0, 0, 0) != 0 &&
        OpusPdfSnapshotAddRun("x", INT_MAX, "Arial", 20,
                              0, 0, 0, 0, 0, 0, 0, 0) == 0;
    std::string result;
    if (read) {
        std::ifstream input(roundtrip, std::ios::binary);
        result.assign(std::istreambuf_iterator<char>(input), {});
    }
    const std::string odt_result = odt_read ? read_file(odt_roundtrip) : "";
    const std::string odt_package = odt_written ? read_file(odt) : "";
    std::string pdf_data;
    if (pdf_written) {
        std::ifstream input(pdf, std::ios::binary);
        pdf_data.assign(std::istreambuf_iterator<char>(input), {});
    }
    DeleteFileA(rtf.c_str());
    DeleteFileA(docx.c_str());
    DeleteFileA(odt.c_str());
    DeleteFileA(roundtrip.c_str());
    DeleteFileA(odt_roundtrip.c_str());
    DeleteFileA(imported_text.c_str());
    DeleteFileA(odt_imported_text.c_str());
    DeleteFileA(compressed_odt.c_str());
    DeleteFileA(compressed_rtf.c_str());
    DeleteFileA(oversized.c_str());
    DeleteFileA(preserved.c_str());
    if (!keep_pdf) DeleteFileA(pdf.c_str());
    if (!written || !read || !unicode_imported || !cyrillic_sidecar ||
        !greek_sidecar || !arabic_sidecar || !japanese_sidecar ||
        !odt_written || !odt_read || !odt_unicode_imported ||
        !odt_cyrillic_sidecar || !odt_greek_sidecar ||
        !odt_arabic_sidecar || !odt_japanese_sidecar ||
        odt_result.find("Bold") == std::string::npos ||
        odt_result.find("Second paragraph") == std::string::npos ||
        odt_result.find("\\u1055") == std::string::npos ||
        odt_result.find("\\u915") == std::string::npos ||
        odt_result.find("\\u1605") == std::string::npos ||
        odt_result.find("\\u12371") == std::string::npos ||
        odt_result.find("\\b") == std::string::npos ||
        odt_result.find("\\cf1") == std::string::npos ||
        !odt_package.starts_with("PK") ||
        odt_package.find("application/vnd.oasis.opendocument.text") ==
            std::string::npos ||
        odt_package.find("META-INF/manifest.xml") == std::string::npos ||
        !compressed_odt_read ||
        compressed_result.find("Compressed") == std::string::npos ||
        compressed_result.find("ODT test") == std::string::npos ||
        compressed_result.find("\\b") == std::string::npos ||
        compressed_result.find("\\cf1") == std::string::npos ||
        result.find("Bold") == std::string::npos ||
        result.find("Second paragraph") == std::string::npos ||
        result.find("\\u1055") == std::string::npos ||
        result.find("\\u915") == std::string::npos ||
        result.find("\\u1605") == std::string::npos ||
        result.find("\\u12371") == std::string::npos ||
        result.find("\\b") == std::string::npos ||
        result.find("\\cf1") == std::string::npos || !pdf_written ||
        !pdf_data.starts_with("%PDF-") ||
        pdf_data.find("Plain") == std::string::npos ||
        pdf_data.find("Bold") == std::string::npos ||
        pdf_data.find("Second paragraph") == std::string::npos ||
        pdf_data.find("/Encoding /Identity-H") == std::string::npos ||
        pdf_data.find("/ToUnicode") == std::string::npos ||
        pdf_data.find("/Helvetica-Bold") == std::string::npos ||
        pdf_data.find("xref") == std::string::npos ||
        pdf_data.find("%%EOF") == std::string::npos ||
        !oversized_rejected || !invalid_snapshot_rejected) {
        std::cerr << "modern format round trip failed: docxWrite=" << written
                  << " read=" << read << " pdf=" << pdf_written
                  << " odtWrite=" << odt_written
                  << " odtRead=" << odt_read
                  << " bytes=" << result.size()
                  << " oversizedRejected=" << oversized_rejected
                  << " invalidSnapshotRejected="
                  << invalid_snapshot_rejected << '\n';
        return 2;
    }
    std::cout << "DOCX/ODT round trips passed (" << result.size()
              << " DOCX RTF bytes, " << odt_result.size()
              << " ODT RTF bytes)\n";
    return 0;
}
