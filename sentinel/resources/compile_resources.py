#!/bin/python3

import sys
import pathlib
import re

def sanitize_name(name):
    # Turns "cortex-note.cls" into "cortex_note_cls"
    return re.sub(r'[^a-zA-Z0-9_]', '_', name)

def generate_resources(resource_dir, output_path,ignore):
    res_path = pathlib.Path(resource_dir)
    script_name = pathlib.Path(__file__).name
    
    lines = [
        "#pragma once",
        "#include <string_view>",
        "",
        "namespace Resources {",
    ]

    # Iterate through all files in the directory
    for file in res_path.iterdir():
        if file.is_file() and file.name != script_name and ".h" not in file.name and ".cpp" not in file.name:
            print("Spooling ",file,ignore)
            var_name = sanitize_name(file.name)
            content = file.read_text()
            
            lines.append(f"    // From {file.name}")
            # We use the filename as the raw string delimiter for safety
            lines.append(f"    inline constexpr std::string_view {var_name} = R\"{var_name}({content}){var_name}\";")
            lines.append("")

    lines.append("}")

    with open(output_path, 'w') as f:
        f.write("\n".join(lines))

if __name__ == "__main__":
    # Usage: python3 generate_resources.py [dir_to_scan] [output_hpp]
    generate_resources(sys.argv[1], sys.argv[2],sys.argv[0])