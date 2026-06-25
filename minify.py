#!/usr/bin/python3

import sys
import minify_html

if (len(sys.argv) <= 2):
    print('Usage: minify.py input.html output.html')
    sys.exit(1)

input_file = open(sys.argv[1], 'r', encoding='utf-8')
output_file = open(sys.argv[2], 'w', encoding='utf-8')

html = input_file.read()
input_file.close()

html_minified = minify_html.minify(html, minify_css=True, minify_js=True)
output_file.write(html_minified)

output_file.close()
print('Done.')