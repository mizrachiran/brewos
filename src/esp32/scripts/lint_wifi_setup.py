#!/usr/bin/env python3
"""
Lint the WiFi setup page HTML file.
Checks for common issues and validates structure.
"""

import sys
import re
from pathlib import Path

def lint_html(html_file):
    """Lint the HTML file and return list of issues"""
    issues = []
    
    with open(html_file, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Check 1: Ensure mock API code is present (for dev file)
    if '// Mock API endpoints for local development' not in content:
        issues.append(("WARNING", "Mock API code not found - ensure this is the dev file"))
    
    # Check 2: Ensure closing tags match
    open_tags = re.findall(r'<([a-zA-Z][a-zA-Z0-9]*)[^>]*>', content)
    close_tags = re.findall(r'</([a-zA-Z][a-zA-Z0-9]*)>', content)
    
    # Count tag pairs
    tag_counts = {}
    for tag in open_tags:
        tag_counts[tag] = tag_counts.get(tag, 0) + 1
    for tag in close_tags:
        tag_counts[tag] = tag_counts.get(tag, 0) - 1
    
    # Check for unclosed tags (excluding self-closing and void elements)
    void_elements = {'br', 'hr', 'img', 'input', 'meta', 'link', 'area', 'base', 'col', 'embed', 'source', 'track', 'wbr'}
    for tag, count in tag_counts.items():
        if tag.lower() not in void_elements and count > 0:
            issues.append(("ERROR", f"Unclosed tag: <{tag}> ({count} unclosed)"))
        elif tag.lower() not in void_elements and count < 0:
            issues.append(("ERROR", f"Extra closing tag: </{tag}> ({abs(count)} extra)"))
    
    # Check 3: Ensure essential elements exist
    required_elements = {
        '<!DOCTYPE html>': 'DOCTYPE declaration',
        '<html': 'HTML tag',
        '<head': 'HEAD tag',
        '<body': 'BODY tag',
        '</html>': 'Closing HTML tag'
    }
    
    for element, name in required_elements.items():
        if element not in content:
            issues.append(("ERROR", f"Missing required element: {name}"))
    
    # Check 4: Ensure script tag is properly closed
    script_open = content.count('<script>')
    script_close = content.count('</script>')
    if script_open != script_close:
        issues.append(("ERROR", f"Script tag mismatch: {script_open} open, {script_close} close"))
    
    # Check 5: Check for common JavaScript issues
    if 'function scanNetworks()' not in content:
        issues.append(("ERROR", "Missing required function: scanNetworks()"))
    if 'function connectWiFi()' not in content:
        issues.append(("ERROR", "Missing required function: connectWiFi()"))
    if 'function selectNetwork(' not in content:
        issues.append(("ERROR", "Missing required function: selectNetwork()"))
    
    # Check 6: Ensure API endpoints are referenced
    if '/api/wifi/networks' not in content:
        issues.append(("WARNING", "API endpoint '/api/wifi/networks' not found"))
    if '/api/wifi/connect' not in content:
        issues.append(("WARNING", "API endpoint '/api/wifi/connect' not found"))
    
    # Check 7: Check for unescaped characters that might break C++ raw string
    dangerous_chars = {
        '`': 'backtick (may break raw string)',
    }
    for char, desc in dangerous_chars.items():
        if char in content and char not in ['`']:  # Allow backticks in template literals
            # Only warn if it's not in a JavaScript template literal context
            if not re.search(r'`[^`]*`', content):
                issues.append(("WARNING", f"Found {desc} - ensure it's properly escaped"))
    
    return issues

def main():
    script_dir = Path(__file__).parent.parent
    html_file = script_dir / "wifi_setup_page_dev.html"
    
    if not html_file.exists():
        print(f"Error: {html_file} not found")
        return 1
    
    print(f"Linting {html_file.name}...")
    issues = lint_html(html_file)
    
    if not issues:
        print("✓ No issues found")
        return 0
    
    # Group issues by severity
    errors = [i for i in issues if i[0] == "ERROR"]
    warnings = [i for i in issues if i[0] == "WARNING"]
    
    if errors:
        print(f"\n✗ Found {len(errors)} error(s):")
        for severity, message in errors:
            print(f"  [{severity}] {message}")
    
    if warnings:
        print(f"\n⚠ Found {len(warnings)} warning(s):")
        for severity, message in warnings:
            print(f"  [{severity}] {message}")
    
    # Return non-zero exit code if there are errors
    return 1 if errors else 0

if __name__ == "__main__":
    sys.exit(main())

