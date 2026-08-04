#!/usr/bin/env python3
"""Python replacement for weak_stub_maker.pl - generates weak-linking stub C++ code from .stubs spec files"""

import sys
import os
import re
import shlex

# Parse command-line arguments
foundation = False
args = sys.argv[1:]
filtered_args = []
for a in args:
    if a in ('--foundation', '-f', '-foundation'):
        foundation = True
    else:
        filtered_args.append(a)

if len(filtered_args) < 2:
    print("Usage: weak_stub_maker.py [--foundation] <input.stubs> <output.cpp>", file=sys.stderr)
    sys.exit(1)

input_file = filtered_args[0]
output_file = filtered_args[1]

# Read all of the input data
with open(input_file, 'r') as f:
    spec_lines = f.readlines()

# Output accumulator
output_lines = []

def out(line=""):
    output_lines.append(line + "\n")

def trim(s):
    return s.strip()

# Modules
module_symbols = {}   # module -> concatenated symbol lines (newline separated)
module_details = {}   # module -> details string (library paths)
module_order = []     # preserve insertion order
current_module = None

# Parse the spec file
for line in spec_lines:
    # Ignore empty lines
    if re.match(r'^\s*$', line):
        continue
    # Ignore comment lines
    if re.match(r'^\s*#', line):
        continue
    # Lines not beginning with tab define modules
    if line[0] != '\t':
        m = re.match(r'^([^\s]*)(.*$)', line)
        module = trim(m.group(1))
        details = trim(m.group(2))
        current_module = module
        module_details[module] = details
        if module not in module_symbols:
            module_symbols[module] = ""
            module_order.append(module)
    else:
        # Line defines a symbol for the current module
        stripped = line.strip()
        module_symbols[current_module] += stripped + "\n"

# ---- Helper functions ----

def symbol_name(symbol):
    items = symbol.split(':', 1)
    words = items[0].split()
    if words and words[0] in ('?', '@'):
        return words[1] if len(words) > 1 else ''
    return items[0]

def symbol_is_optional(symbol):
    return symbol.startswith('?')

def symbol_is_reference(symbol):
    return symbol.startswith('@')

def symbol_inputs(symbol):
    items = symbol.split(':', 1)
    spec = items[1] if len(items) >= 2 else "() -> ()"
    arrow = spec.index('->')
    inputs = spec[:arrow]
    inputs = trim(inputs)
    # Remove parens
    inputs = inputs[1:-1]
    parts = inputs.split(',')
    result = ""
    for part in parts:
        t = trim(part)
        if t:
            result += t + "\n"
    return result

def symbol_outputs(symbol):
    items = symbol.split(':', 1)
    spec = items[1] if len(items) >= 2 else "() -> ()"
    arrow = spec.index('->') + 3
    outputs = spec[arrow:]
    outputs = trim(outputs)
    # Remove parens
    outputs = outputs[1:-1]
    parts = outputs.split(',')
    result = ""
    for part in parts:
        t = trim(part)
        if t:
            result += t + "\n"
    return result

TYPE_MAP = {
    'pointer':   'void *',
    'integer':   'int ',
    'double':    'double ',
    'integer64': 'long long int ',
    'intsize':   'size_t ',
}

def type_list_to_proto(type_list, with_args):
    proto = ""
    index = 0
    lines = [l for l in type_list.split("\n") if l]
    for line in lines:
        if line not in TYPE_MAP:
            raise ValueError("Unknown type specified: %s" % line)
        proto += TYPE_MAP[line]
        if with_args:
            proto += "pArg%d" % index
        proto += ", "
        index += 1
    # Remove trailing ", "
    if proto.endswith(", "):
        proto = proto[:-2]
    if proto == "":
        if with_args:
            proto = "void"
        else:
            proto = "void "
    return proto


# ---- Emit boilerplate ----

out("#include <stdlib.h>")
out("#include <stdio.h>")
out("#include <cstring>")
out()
out("#define SYMBOL_PREFIX")
out()
out("typedef void *module_t;")
out("typedef void *handler_t;")
out()
out("#ifdef _DEBUG")
out("#include <stdint.h>")
out("extern void __MCLog(const char *file, uint32_t line, const char *format, ...);")
out("#define MCLog(...) __MCLog(__FILE__, __LINE__, __VA_ARGS__)")
out("#else")
out("#define MCLog(...) (void) (__VA_ARGS__)")
out("#endif")

if foundation:
    out("#define kMCStringEncodingUTF8 4")
    out('extern "C" bool MCStringCreateWithBytes(const char *, unsigned int, int, bool, void*&);')
    out('extern "C" void MCValueRelease(void *);')
    out('extern "C" bool MCSLibraryCreateWithPath(void *, void*&);')
    out('extern "C" void *MCSLibraryLookupSymbol(void *, void *);')
    out()
    out("static void *MCSupportLibraryLoad(const char *p_name_cstr)")
    out("{")
    out("  void *t_name;")
    out('  if (!MCStringCreateWithBytes(p_name_cstr, strlen(p_name_cstr), kMCStringEncodingUTF8, false, t_name))')
    out("    return NULL;")
    out("  void *t_handle;")
    out("  if (!MCSLibraryCreateWithPath(t_name, t_handle))")
    out("  {")
    out("    t_handle = NULL;")
    out("  }")
    out("  MCValueRelease(t_name);")
    out("  return t_handle;")
    out("}")
    out()
    out("static void MCSupportLibraryUnload(void *p_handle)")
    out("{")
    out("  if (p_handle != NULL)")
    out("    MCValueRelease(p_handle);")
    out("}")
    out()
    out("static void *MCSupportLibraryLookupSymbol(void *p_handle, const char *p_name_cstr)")
    out("{")
    out("  void *t_symbol_name;")
    out('  if (!MCStringCreateWithBytes(p_name_cstr, strlen(p_name_cstr), kMCStringEncodingUTF8, false, t_symbol_name))')
    out("    return NULL;")
    out("  void *t_symbol;")
    out("  t_symbol = MCSLibraryLookupSymbol(p_handle, t_symbol_name);")
    out("  MCValueRelease(t_symbol_name);")
    out("  return t_symbol;")
    out("}")
else:
    out('extern "C" void *MCSupportLibraryLoad(const char *);')
    out('extern "C" void MCSupportLibraryUnload(void *);')
    out('extern "C" void *MCSupportLibraryLookupSymbol(void *, const char *);')

out()
out('extern "C"')
out("{")
out()
out("static int module_load(const char *p_source, module_t *r_module)")
out("{")
out("  module_t t_module;")
out(" \tt_module = (module_t)MCSupportLibraryLoad(p_source);")
out("  if (t_module == NULL)")
out("    return 0;")
out("  *r_module = t_module;")
out("  return 1;")
out("}")
out()
out("static int module_unload(module_t p_module)")
out("{")
out("  MCSupportLibraryUnload(p_module);")
out("  return 1;")
out("}")
out()
out("static int module_resolve(module_t p_module, const char *p_name, handler_t *r_handler)")
out("{")
out("  handler_t t_handler;")
out("    t_handler = MCSupportLibraryLookupSymbol(p_module, p_name);")
out("  if (t_handler == NULL)")
out("    return 0;")
out("  *r_handler = t_handler;")
out("  return 1;")
out("}")
out()
out()
out("#if defined(_LINUX)")
out("static void failed_required_link(const char* libname, const char* liblist)")
out("{")
out('  const char* dialog =')
out('    "zenity --error --title \\"$TITLE\\" --text \\"$TEXT\\" 2>/dev/null 1>/dev/null && "')
out('    "echo \\"wm state . withdrawn ; tk_messageBox -icon error -message \\\\\\"$TEXT\\\\\\" -title \\\\\\"$TITLE\\\\\\" -type ok ; exit \\" | wish && "')
out('    "xmessage -center -buttons OK -default OK \\"$TITLE:\\" \\"$TEXT\\"" ')
out('\t;')
out()
out("  char* error = new char[65536];")
out("  char* command = new char[65536];")
out()
out('  snprintf(error, 65536, "Failed to load library \\\'%s\\\' (tried %s)", libname, liblist);')
out('  snprintf(command, 65536, "TITLE=\\"HyperXTalk startup error\\" TEXT=\\"%s\\" /bin/sh -c \\\'%s\\\' &", error, dialog);')
out('  MCLog( "Fatal: failed to load library \\\'%s\\\' (tried %s)\\n", libname, liblist);')
out('  int ignored = system(command); (void)ignored;')
out("  exit(-1);")
out("}")
out("#endif")
out()


# ---- Generate per-module code ----

def generate_module(module):
    libraries = shlex.split(module_details[module]) if module_details[module] else []
    unix_library   = libraries[0] if len(libraries) > 0 else ""
    darwin_library = libraries[1] if len(libraries) > 1 else ""
    windows_library= libraries[2] if len(libraries) > 2 else ""

    symbols_raw = module_symbols.get(module, "")
    symbols = [s for s in symbols_raw.split("\n") if s.strip()]

    for symbol in symbols:
        if not symbol_is_reference(symbol):
            outputs_proto = type_list_to_proto(symbol_outputs(symbol), 0)
            inputs_proto  = type_list_to_proto(symbol_inputs(symbol), 1)
            out("typedef %s(*%s_t)(%s);" % (outputs_proto, symbol_name(symbol), inputs_proto))
            out("%s_t %s_ptr = NULL;" % (symbol_name(symbol), symbol_name(symbol)))
        else:
            out("void *%s_ptr = NULL;" % symbol_name(symbol))

    module_upper = module.upper()
    out()
    out("#if defined(_MACOSX) || defined(TARGET_SUBPLATFORM_IPHONE)")
    out('#define MODULE_%s_NAME "%s"' % (module_upper, darwin_library))
    out("#elif defined(_LINUX) || defined(__EMSCRIPTEN__)")
    out('#define MODULE_%s_NAME "%s"' % (module_upper, unix_library))
    out("#elif defined(TARGET_SUBPLATFORM_ANDROID)")
    out('#define MODULE_%s_NAME "%s"' % (module_upper, unix_library))
    out("#elif defined(_WINDOWS)")
    out('#define MODULE_%s_NAME "%s"' % (module_upper, windows_library))
    out("#endif")
    out()
    out("static module_t module_%s = NULL;" % module)
    out()

    out("void finalise_weak_link_%s(void)" % module)
    out("{")
    out("  module_unload(module_%s);" % module)
    out("  module_%s = NULL;" % module)
    out("}")
    out()
    out("int initialise_weak_link_%s_with_path(const char *p_path)" % module)
    out("{")
    out("  module_%s = NULL;" % module)
    out("  if (!module_load(p_path, &module_%s))" % module)
    out("  {")
    out("  #ifdef _DEBUG")
    out('    MCLog( "Unable to load library: %s\\n", p_path);')
    out("  #endif")
    out("    goto err;")
    out("  }")

    for symbol in symbols:
        if symbol_is_optional(symbol):
            out('module_resolve(module_%s, SYMBOL_PREFIX "%s", (handler_t *)&%s_ptr);' % (
                module, symbol_name(symbol), symbol_name(symbol)))
        else:
            out('  if (!module_resolve(module_%s, SYMBOL_PREFIX "%s", (handler_t *)&%s_ptr))' % (
                module, symbol_name(symbol), symbol_name(symbol)))
            out("{")
            out("#ifdef _DEBUG")
            out('MCLog( "Unable to load: %s\\n");' % symbol_name(symbol))
            out("#endif")
            out("goto err; ")
            out("}")

    out()
    out("  return 1;")
    out()
    out("err:")
    out("  if (module_%s != NULL)" % module)
    out("    module_unload(module_%s);" % module)
    out()
    out("  return 0;")
    out("}")
    out()

    out("int initialise_weak_link_%s(void)" % module)
    out("{")
    out("#if defined(_LINUX)")
    for item in unix_library.split(','):
        out('  if(!initialise_weak_link_%s_with_path("%s"))' % (module, item))
    out("#else")
    out("  if (!initialise_weak_link_%s_with_path(MODULE_%s_NAME))" % (module, module_upper))
    out("#endif")
    out("{")
    out("#ifdef _DEBUG")
    out('    MCLog( "Warning: could not load library: %s\\n");' % unix_library)
    out("#endif")
    out("return 0;")
    out("}")
    out("return -1;")
    out("}")

    out()
    out("#if defined(_LINUX)")
    out("void initialise_required_weak_link_%s(void)" % module)
    out("{")
    out("  if (!initialise_weak_link_%s())" % module)
    out("  {")
    out('    failed_required_link("%s", "%s");' % (module, unix_library))
    out("  }")
    out("}")
    out("#endif")
    out()

    for symbol in symbols:
        if symbol_is_reference(symbol):
            continue
        outputs_proto = type_list_to_proto(symbol_outputs(symbol), 0)
        inputs_proto  = type_list_to_proto(symbol_inputs(symbol), 1)

        args = ""
        if inputs_proto != "void":
            input_items = inputs_proto.split(',')
            for idx in range(len(input_items)):
                args += "pArg%d, " % idx
            if args.endswith(", "):
                args = args[:-2]

        out("%s%s(%s)" % (outputs_proto, symbol_name(symbol), inputs_proto))
        out("{")
        if outputs_proto != "void ":
            out("  return %s_ptr(%s);" % (symbol_name(symbol), args))
        else:
            out("  %s_ptr(%s);" % (symbol_name(symbol), args))
        out("}")
        out()


for module in module_order:
    generate_module(module)

out("}")

# Write output
os.makedirs(os.path.dirname(os.path.abspath(output_file)), exist_ok=True)
with open(output_file, 'w') as f:
    f.write(''.join(output_lines))
