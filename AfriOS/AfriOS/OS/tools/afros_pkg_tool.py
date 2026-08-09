#!/usr/bin/env python3
import json
import os
import sys
import zipfile

"""
AfriOS Package Tool (apkg-tool)
Used to package multi-platform applications into the universal .apkg format.
"""

def create_manifest(name, version, pkg_type, entry_point):
    manifest = {
        "name": name,
        "version": version,
        "type": pkg_type,
        "entry_point": entry_point,
        "requirements": {
            "min_afros_version": "1.0.0",
            "capabilities": ["network", "storage"]
        }
    }
    return manifest

def package_app(source_dir, output_name):
    if not os.path.exists(source_dir):
        print(f"Error: Source directory {source_dir} not found.")
        return

    manifest_path = os.path.join(source_dir, "manifest.json")
    if not os.path.exists(manifest_path):
        print("Error: manifest.json missing in source directory.")
        return

    output_file = f"{output_name}.apkg"
    print(f"Packaging {source_dir} into {output_file}...")
    
    with zipfile.ZipFile(output_file, 'w') as apkg:
        for root, dirs, files in os.walk(source_dir):
            for file in files:
                file_path = os.path.join(root, file)
                arcname = os.path.relpath(file_path, source_dir)
                apkg.write(file_path, arcname)
    
    print(f"Successfully created {output_file}")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python afros_pkg_tool.py <source_dir> <output_name>")
        sys.exit(1)
        
    package_app(sys.argv[1], sys.argv[2])
