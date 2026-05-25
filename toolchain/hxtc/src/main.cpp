/*
 * hxtc — HyperXTalk compiled library compiler
 *
 * Usage:
 *   hxtc [options] <input.livecodescript> -o <output.hxtlib>
 *
 * Options:
 *   -o, --output <path>            Output .hxtlib file path (required)
 *       --name <string>            Library display name (default: parsed from script header)
 *       --version <semver>         Library version, e.g. "1.0.0"  (default: "0.0.0")
 *       --author <string>          Author name
 *       --identifier <reverse-dns> Reverse-domain identifier, e.g. "com.example.mylib"
 *       --description <string>     Short description
 *       --min-engine <version>     Minimum HyperXTalk engine version required
 *       --no-clobber               Fail if output file already exists
 *   -v, --verbose                  Print progress to stderr
 *   -h, --help                     Show this help
 *
 * Exit codes:
 *   0  Success
 *   1  Bad arguments / missing input
 *   2  Could not read input file
 *   3  hxtlib write error
 *
 * Current status:
 *   Phase 2 — generates a structurally valid .hxtlib file containing META,
 *   STRT, SRCS, and an empty ASTN section.  The engine's AST serialisation
 *   layer (MCHXTASTWriter/Reader, Tasks 22-31) is now fully defined, but hxtc
 *   links only against hxtlib and cannot invoke the engine parser to produce
 *   binary ASTN bytes.  The engine loader reads SRCS and re-parses source text
 *   at load time as a fallback.
 *
 *   To produce a source-stripped library with ASTN populated, use the engine's
 *   built-in save-as-hxtlib facility (planned for a future task), which runs
 *   inside the engine process with full access to the parsed AST.
 *
 * Build:
 *   See toolchain/hxtc/hxtc.gyp  (links only against hxtlib, no engine).
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "hxtlib.h"

// ── helpers ──────────────────────────────────────────────────────────────────

static void print_help(const char *argv0)
{
    std::fprintf(stderr,
        "Usage: %s [options] <input.livecodescript> -o <output.hxtlib>\n"
        "\n"
        "Options:\n"
        "  -o, --output <path>            Output .hxtlib file path (required)\n"
        "      --name <string>            Library display name\n"
        "      --version <semver>         Library version   (default: 0.0.0)\n"
        "      --author <string>          Author\n"
        "      --identifier <rev-dns>     Reverse-domain identifier\n"
        "      --description <string>     Short description\n"
        "      --min-engine <version>     Minimum engine version required\n"
        "      --no-clobber               Fail if output already exists\n"
        "  -v, --verbose                  Verbose output\n"
        "  -h, --help                     This help\n",
        argv0);
}

// Return true if str starts with prefix.
static bool starts_with(const std::string &str, const char *prefix)
{
    return str.rfind(prefix, 0) == 0;
}

// Trim leading/trailing whitespace from s.
static std::string trim(const std::string &s)
{
    const char *ws = " \t\r\n";
    size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos)
        return {};
    size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

// ── source header parser ──────────────────────────────────────────────────────
//
// A .livecodescript file starts with a single header line:
//
//   script "Stack Name" [with behavior "OtherStack"]
//
// We extract the name string from it.  If the file is not a script-only
// stack (no header line) we return false and leave r_name empty.

static bool parse_script_header(const std::string &source, std::string &r_name)
{
    // Find the first non-blank line.
    size_t pos = 0;
    while (pos < source.size() && (source[pos] == '\r' || source[pos] == '\n'))
        ++pos;

    size_t line_end = source.find('\n', pos);
    if (line_end == std::string::npos)
        line_end = source.size();

    std::string first_line = trim(source.substr(pos, line_end - pos));

    // Must start with "script" (case-insensitive).
    if (first_line.size() < 6)
        return false;
    std::string kw = first_line.substr(0, 6);
    for (auto &c : kw) c = (char)std::tolower((unsigned char)c);
    if (kw != "script")
        return false;

    // Skip whitespace after "script".
    size_t i = 6;
    while (i < first_line.size() && first_line[i] == ' ')
        ++i;

    // Expect a quoted string (single or double quotes).
    if (i >= first_line.size())
        return false;
    char quote = first_line[i];
    if (quote != '"' && quote != '\'')
        return false;
    ++i;

    size_t name_start = i;
    while (i < first_line.size() && first_line[i] != quote)
        ++i;
    if (i >= first_line.size())
        return false; // unclosed quote — malformed header

    r_name = first_line.substr(name_start, i - name_start);
    return true;
}

// ── file I/O ──────────────────────────────────────────────────────────────────

static bool read_file(const std::string &path, std::string &r_contents)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        return false;
    r_contents.assign(std::istreambuf_iterator<char>(f),
                      std::istreambuf_iterator<char>());
    return f.good() || f.eof();
}

static bool file_exists(const std::string &path)
{
    std::ifstream f(path);
    return f.is_open();
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        print_help(argv[0]);
        return 1;
    }

    // --- argument parsing ---

    std::string input_path;
    std::string output_path;
    bool        verbose    = false;
    bool        no_clobber = false;

    hxtlib::Document doc;
    doc.meta.version    = "0.0.0";   // default; overridden by --version
    doc.eng_major       = 0;
    doc.eng_minor       = 0;
    doc.eng_patch       = 0;
    doc.compiled_at     = 0;         // hxtlib::write() fills in current time

    bool name_explicit = false;      // --name was given on the command line

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        auto next_arg = [&](const char *flag) -> std::string {
            if (++i >= argc) {
                std::fprintf(stderr, "hxtc: %s requires an argument\n", flag);
                std::exit(1);
            }
            return argv[i];
        };

        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
        else if (arg == "-v" || arg == "--verbose")
        {
            verbose = true;
        }
        else if (arg == "--no-clobber")
        {
            no_clobber = true;
        }
        else if (arg == "-o" || arg == "--output")
        {
            output_path = next_arg(arg.c_str());
        }
        else if (arg == "--name")
        {
            doc.meta.name  = next_arg("--name");
            name_explicit  = true;
        }
        else if (arg == "--version")
        {
            doc.meta.version = next_arg("--version");
        }
        else if (arg == "--author")
        {
            doc.meta.author = next_arg("--author");
        }
        else if (arg == "--identifier")
        {
            doc.meta.identifier = next_arg("--identifier");
        }
        else if (arg == "--description")
        {
            doc.meta.description = next_arg("--description");
        }
        else if (arg == "--min-engine")
        {
            doc.meta.min_engine = next_arg("--min-engine");
        }
        else if (starts_with(arg, "-"))
        {
            std::fprintf(stderr, "hxtc: unknown option '%s'\n", arg.c_str());
            return 1;
        }
        else
        {
            if (!input_path.empty())
            {
                std::fprintf(stderr, "hxtc: multiple input files are not supported\n");
                return 1;
            }
            input_path = arg;
        }
    }

    // --- validate required args ---

    if (input_path.empty())
    {
        std::fprintf(stderr, "hxtc: no input file specified\n");
        print_help(argv[0]);
        return 1;
    }
    if (output_path.empty())
    {
        std::fprintf(stderr, "hxtc: no output file specified (use -o)\n");
        return 1;
    }
    if (no_clobber && file_exists(output_path))
    {
        std::fprintf(stderr, "hxtc: output file already exists: %s\n",
                     output_path.c_str());
        return 1;
    }

    // --- read source ---

    if (verbose)
        std::fprintf(stderr, "hxtc: reading %s\n", input_path.c_str());

    std::string source;
    if (!read_file(input_path, source))
    {
        std::perror(("hxtc: cannot read " + input_path).c_str());
        return 2;
    }

    // --- parse name from header (if not given explicitly) ---

    if (!name_explicit)
    {
        std::string parsed_name;
        if (parse_script_header(source, parsed_name))
        {
            doc.meta.name = parsed_name;
            if (verbose)
                std::fprintf(stderr, "hxtc: stack name from header: %s\n",
                             parsed_name.c_str());
        }
        else
        {
            // Fall back to the input filename stem.
            std::string stem = input_path;
            size_t slash = stem.find_last_of("/\\");
            if (slash != std::string::npos)
                stem = stem.substr(slash + 1);
            size_t dot = stem.rfind('.');
            if (dot != std::string::npos)
                stem = stem.substr(0, dot);
            doc.meta.name = stem;
            if (verbose)
                std::fprintf(stderr, "hxtc: no script header found; using "
                                     "filename stem: %s\n", stem.c_str());
        }
    }

    // If identifier was not provided, derive one from the name.
    if (doc.meta.identifier.empty() && !doc.meta.name.empty())
    {
        // Convert name to lowercase, replace spaces with dots.
        std::string id = doc.meta.name;
        for (auto &c : id)
            c = (char)(c == ' ' ? '.' : std::tolower((unsigned char)c));
        doc.meta.identifier = "com.hyperxtalk.library." + id;
    }

    // --- populate string table with meta strings ---
    //   (so the table has at least the library name interned)

    if (!doc.meta.name.empty())        doc.intern(doc.meta.name);
    if (!doc.meta.identifier.empty())  doc.intern(doc.meta.identifier);

    // --- source script (SRCS section) ---
    //
    // Store the full source text so the engine loader can call SetScript()
    // to populate the handler list.  This is the interim approach until the
    // AST serialisation layer is defined, at which point SRCS can be omitted
    // and ASTN nodes will carry the pre-parsed representation instead.
    doc.source_script = source;

    if (verbose)
        std::fprintf(stderr,
            "hxtc: stored %zu bytes of source script in SRCS section\n",
            source.size());

    // --- ASTN bytes ---
    //
    // hxtc is a standalone tool that does not link against the engine, so it
    // cannot call MCHandlerlist::hxt_serialize() to produce an ASTN blob here.
    // doc.astn_bytes is left empty; the engine loader will fall back to the
    // SRCS section and re-parse the source text at load time.
    //
    // To produce a source-stripped .hxtlib with a populated ASTN section, use
    // the engine's own save-as-hxtlib facility (to be added in a future task),
    // which runs inside the engine process and has access to the full AST.

    if (verbose && doc.astn_bytes.empty())
        std::fprintf(stderr,
            "hxtc: note: ASTN is empty — binary AST not available from "
            "standalone hxtc; engine will re-parse from SRCS\n");

    // --- write ---

    if (verbose)
        std::fprintf(stderr, "hxtc: writing %s\n", output_path.c_str());

    hxtlib::Error err = hxtlib::write(doc, output_path);
    if (err != hxtlib::Error::Ok)
    {
        std::fprintf(stderr, "hxtc: write failed: %s\n", hxtlib::strerror(err));
        return 3;
    }

    if (verbose)
        std::fprintf(stderr, "hxtc: done — %s -> %s\n",
                     input_path.c_str(), output_path.c_str());

    return 0;
}
