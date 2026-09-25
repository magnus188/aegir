#!/usr/bin/env python3
"""
Font conversion script for LVGL
Converts TTF fonts to LVGL C format using lv_font_conv

Prerequisites:
- Node.js installed
- lv_font_conv installed globally: npm install -g lv_font_conv

Usage:
python convert_fonts.py
"""

import os
import subprocess
import sys

# Font conversion configurations
FONT_CONFIGS = [
    # Normal weight fonts
    {
        'input': '../../assets/fonts/normal.ttf',
        'output': 'custom_font_normal_16.c',
        'size': 16,
        'format': 'lvgl',
        'bpp': 4,
        'name': 'custom_font_normal_16'
    },
    {
        'input': '../../assets/fonts/normal.ttf',
        'output': 'custom_font_normal_20.c',
        'size': 20,
        'format': 'lvgl',
        'bpp': 4,
        'name': 'custom_font_normal_20'
    },
    {
        'input': '../../assets/fonts/normal.ttf',
        'output': 'custom_font_normal_24.c',
        'size': 24,
        'format': 'lvgl',
        'bpp': 4,
        'name': 'custom_font_normal_24'
    },
    {
        'input': '../../assets/fonts/normal.ttf',
        'output': 'custom_font_normal_28.c',
        'size': 28,
        'format': 'lvgl',
        'bpp': 4,
        'name': 'custom_font_normal_28'
    },
    {
        'input': '../../assets/fonts/normal.ttf',
        'output': 'custom_font_normal_32.c',
        'size': 32,
        'format': 'lvgl',
        'bpp': 4,
        'name': 'custom_font_normal_32'
    },
    # Bold weight fonts
    {
        'input': '../../assets/fonts/bold.ttf',
        'output': 'custom_font_bold_16.c',
        'size': 16,
        'format': 'lvgl',
        'bpp': 4,
        'name': 'custom_font_bold_16'
    },
    {
        'input': '../../assets/fonts/bold.ttf',
        'output': 'custom_font_bold_20.c',
        'size': 20,
        'format': 'lvgl',
        'bpp': 4,
        'name': 'custom_font_bold_20'
    },
    {
        'input': '../../assets/fonts/bold.ttf',
        'output': 'custom_font_bold_24.c',
        'size': 24,
        'format': 'lvgl',
        'bpp': 4,
        'name': 'custom_font_bold_24'
    },
    {
        'input': '../../assets/fonts/bold.ttf',
        'output': 'custom_font_bold_28.c',
        'size': 28,
        'format': 'lvgl',
        'bpp': 4,
        'name': 'custom_font_bold_28'
    },
    {
        'input': '../../assets/fonts/bold.ttf',
        'output': 'custom_font_bold_32.c',
        'size': 32,
        'format': 'lvgl',
        'bpp': 4,
        'name': 'custom_font_bold_32'
    },
    # Light weight fonts
    {
        'input': '../../assets/fonts/light.ttf',
        'output': 'custom_font_light_14.c',
        'size': 14,
        'format': 'lvgl',
        'bpp': 4,
        'name': 'custom_font_light_14'
    },
    {
        'input': '../../assets/fonts/light.ttf',
        'output': 'custom_font_light_16.c',
        'size': 16,
        'format': 'lvgl',
        'bpp': 4,
        'name': 'custom_font_light_16'
    },
    {
        'input': '../../assets/fonts/light.ttf',
        'output': 'custom_font_light_20.c',
        'size': 20,
        'format': 'lvgl',
        'bpp': 4,
        'name': 'custom_font_light_20'
    }
]

def check_lv_font_conv():
    """Check if lv_font_conv is installed"""
    try:
        result = subprocess.run(['lv_font_conv', '--help'], capture_output=True, text=True)
        return result.returncode == 0
    except FileNotFoundError:
        return False

def convert_font(config):
    """Convert a single font using lv_font_conv"""
    cmd = [
        'lv_font_conv',
        '--font', config['input'],
        '--size', str(config['size']),
        '--format', config['format'],
        '--bpp', str(config['bpp']),
        '--output', config['output'],
        '--force-fast-kern-format'
    ]
    
    print(f"Converting {config['input']} -> {config['output']} (size: {config['size']})")
    
    try:
        result = subprocess.run(cmd, check=True, capture_output=True, text=True)
        print(f"✅ Successfully converted {config['output']}")
        return True
    except subprocess.CalledProcessError as e:
        print(f"❌ Failed to convert {config['output']}: {e}")
        print(f"Error output: {e.stderr}")
        return False

def main():
    print("LVGL Font Converter")
    print("==================")
    
    # Check if lv_font_conv is available
    if not check_lv_font_conv():
        print("❌ lv_font_conv not found!")
        print("Please install it with: npm install -g lv_font_conv")
        print("Make sure Node.js is installed first.")
        sys.exit(1)
    
    print("✅ lv_font_conv found")
    
    # Create fonts directory if it doesn't exist
    os.makedirs('.', exist_ok=True)
    
    success_count = 0
    total_count = len(FONT_CONFIGS)
    
    # Convert each font
    for config in FONT_CONFIGS:
        if convert_font(config):
            success_count += 1
    
    print(f"\nConversion complete: {success_count}/{total_count} fonts converted successfully")
    
    if success_count == total_count:
        print("🎉 All fonts converted successfully!")
        print("Don't forget to add the generated .c files to your CMakeLists.txt")
    else:
        print("⚠️  Some fonts failed to convert. Check the error messages above.")

if __name__ == '__main__':
    main()
