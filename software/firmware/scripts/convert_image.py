#!/usr/bin/env python3
"""
Convert PNG image to LVGL-compatible C array for ESP32.
Uses ARGB8888 format (32-bit with alpha) for best quality.
"""

import sys
from PIL import Image

def convert_png_to_c_array(input_path, output_path, target_width=150):
    """Convert PNG to LVGL C array format."""
    
    # Open and resize image
    img = Image.open(input_path)
    
    # Calculate aspect ratio
    aspect = img.height / img.width
    target_height = int(target_width * aspect)
    
    # Resize with high quality
    img = img.resize((target_width, target_height), Image.LANCZOS)
    
    # Ensure RGBA format
    img = img.convert('RGBA')
    
    width, height = img.size
    pixels = list(img.getdata())
    
    print(f"Converting {input_path}")
    print(f"Output size: {width}x{height}")
    print(f"Pixel count: {len(pixels)}")
    
    # Generate C header file
    c_code = f"""// Auto-generated LVGL image - DO NOT EDIT
// Source: {input_path}
// Size: {width}x{height}
// Format: ARGB8888

#pragma once
#include "lvgl.h"

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

// Image data array
static const uint8_t ziphius_logo_data[] LV_ATTRIBUTE_MEM_ALIGN = {{
"""
    
    # Convert pixels to ARGB8888 format (LVGL uses BGRA byte order)
    byte_data = []
    for r, g, b, a in pixels:
        # LVGL ARGB8888 is stored as B, G, R, A in memory
        byte_data.extend([b, g, r, a])
    
    # Write bytes in rows of 16
    for i in range(0, len(byte_data), 16):
        chunk = byte_data[i:i+16]
        hex_str = ", ".join(f"0x{b:02x}" for b in chunk)
        c_code += f"    {hex_str},\n"
    
    c_code += f"""}};

// LVGL image descriptor
const lv_image_dsc_t ziphius_logo = {{
    .header = {{
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_ARGB8888,
        .w = {width},
        .h = {height},
    }},
    .data_size = {len(byte_data)},
    .data = ziphius_logo_data,
}};
"""
    
    with open(output_path, 'w') as f:
        f.write(c_code)
    
    print(f"Generated: {output_path}")
    print(f"Data size: {len(byte_data)} bytes ({len(byte_data)/1024:.1f} KB)")

if __name__ == "__main__":
    input_file = "assets/images/ziphius-mark-white.png"
    output_file = "main/ui/images/ziphius_logo.h"
    
    if len(sys.argv) > 1:
        input_file = sys.argv[1]
    if len(sys.argv) > 2:
        output_file = sys.argv[2]
    
    convert_png_to_c_array(input_file, output_file)
