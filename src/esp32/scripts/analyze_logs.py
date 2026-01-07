#!/usr/bin/env python3
"""
ESP32 Log Analyzer

Analyzes ESP32 serial output logs to identify:
- PSRAM allocation failures
- Memory issues
- Slow loops
- Error patterns
- System initialization status

Usage:
    python analyze_logs.py < log_file.txt
    pio device monitor | python analyze_logs.py
    cat serial.log | python analyze_logs.py
"""

import sys
import re
from collections import defaultdict
from datetime import datetime
from typing import Dict, List, Tuple

class LogAnalyzer:
    def __init__(self):
        self.psram_failures = []
        self.slow_loops = []
        self.errors = []
        self.warnings = []
        self.memory_stats = []
        self.init_steps = []
        self.timestamps = []
        self.line_count = 0
        
    def parse_line(self, line: str):
        """Parse a single log line and extract relevant information."""
        self.line_count += 1
        
        # Extract timestamp if present [timestamp]
        timestamp_match = re.search(r'\[(\d+)\]', line)
        if timestamp_match:
            self.timestamps.append(int(timestamp_match.group(1)))
        
        # PSRAM allocation failures
        if '[PSRAM] Alloc failed' in line:
            match = re.search(r'Alloc failed: (\d+) bytes \(PSRAM free: (\d+), internal free: (\d+)\)', line)
            if match:
                size = int(match.group(1))
                psram_free = int(match.group(2))
                internal_free = int(match.group(3))
                self.psram_failures.append({
                    'size': size,
                    'psram_free': psram_free,
                    'internal_free': internal_free,
                    'line': self.line_count,
                    'raw': line.strip()
                })
        
        # Slow loops
        if 'SLOW LOOP' in line:
            match = re.search(r'SLOW LOOP: (\d+) ms', line)
            if match:
                duration = int(match.group(1))
                self.slow_loops.append({
                    'duration': duration,
                    'line': self.line_count,
                    'raw': line.strip()
                })
        
        # Memory statistics
        if 'Memory:' in line and 'heap=' in line:
            match = re.search(
                r'heap=(\d+)/(\d+) \(frag=(\d+)%, largest=(\d+)\), '
                r'PSRAM=(\d+)KB/(\d+)KB, maxLoop=(\d+)ms, slowLoops=(\d+)',
                line
            )
            if match:
                self.memory_stats.append({
                    'heap_free': int(match.group(1)),
                    'heap_total': int(match.group(2)),
                    'fragmentation': int(match.group(3)),
                    'largest_block': int(match.group(4)),
                    'psram_free': int(match.group(5)),
                    'psram_total': int(match.group(6)),
                    'max_loop': int(match.group(7)),
                    'slow_loops': int(match.group(8)),
                    'line': self.line_count,
                    'raw': line.strip()
                })
        
        # Errors
        if '[E]' in line or 'ERROR' in line.upper():
            self.errors.append({
                'line': self.line_count,
                'raw': line.strip()
            })
        
        # Warnings
        if '[W]' in line or 'WARNING' in line.upper():
            self.warnings.append({
                'line': self.line_count,
                'raw': line.strip()
            })
        
        # Initialization steps
        if 'Initializing' in line or 'initialized OK' in line:
            self.init_steps.append({
                'line': self.line_count,
                'raw': line.strip()
            })
    
    def analyze(self) -> Dict:
        """Perform analysis and return summary statistics."""
        analysis = {
            'total_lines': self.line_count,
            'time_range': None,
            'psram_analysis': self._analyze_psram(),
            'memory_analysis': self._analyze_memory(),
            'slow_loop_analysis': self._analyze_slow_loops(),
            'error_summary': self._analyze_errors(),
            'warning_summary': self._analyze_warnings(),
            'init_status': self._analyze_init(),
        }
        
        if self.timestamps:
            analysis['time_range'] = {
                'start': min(self.timestamps),
                'end': max(self.timestamps),
                'duration_ms': max(self.timestamps) - min(self.timestamps)
            }
        
        return analysis
    
    def _analyze_psram(self) -> Dict:
        """Analyze PSRAM allocation failures."""
        if not self.psram_failures:
            return {'total': 0, 'status': 'OK'}
        
        sizes = [f['size'] for f in self.psram_failures]
        internal_free = [f['internal_free'] for f in self.psram_failures]
        
        # Group by size ranges
        size_groups = defaultdict(int)
        for size in sizes:
            if size < 100:
                size_groups['<100B'] += 1
            elif size < 1024:
                size_groups['100B-1KB'] += 1
            elif size < 10240:
                size_groups['1KB-10KB'] += 1
            else:
                size_groups['>10KB'] += 1
        
        return {
            'total': len(self.psram_failures),
            'status': 'CRITICAL' if len(self.psram_failures) > 100 else 'WARNING',
            'total_bytes_requested': sum(sizes),
            'avg_size': sum(sizes) / len(sizes),
            'min_size': min(sizes),
            'max_size': max(sizes),
            'size_distribution': dict(size_groups),
            'avg_internal_free': sum(internal_free) / len(internal_free),
            'min_internal_free': min(internal_free),
        }
    
    def _analyze_memory(self) -> Dict:
        """Analyze memory statistics."""
        if not self.memory_stats:
            return {'status': 'No memory stats found'}
        
        latest = self.memory_stats[-1]
        return {
            'status': 'OK',
            'latest': latest,
            'heap_fragmentation': latest['fragmentation'],
            'psram_usage': {
                'free': latest['psram_free'],
                'total': latest['psram_total'],
                'used_percent': ((latest['psram_total'] - latest['psram_free']) / latest['psram_total'] * 100) if latest['psram_total'] > 0 else 0
            }
        }
    
    def _analyze_slow_loops(self) -> Dict:
        """Analyze slow loop warnings."""
        if not self.slow_loops:
            return {'total': 0, 'status': 'OK'}
        
        durations = [s['duration'] for s in self.slow_loops]
        return {
            'total': len(self.slow_loops),
            'status': 'WARNING',
            'max_duration': max(durations),
            'avg_duration': sum(durations) / len(durations),
            'min_duration': min(durations),
        }
    
    def _analyze_errors(self) -> Dict:
        """Analyze error patterns."""
        if not self.errors:
            return {'total': 0, 'status': 'OK'}
        
        # Categorize errors
        error_types = defaultdict(int)
        for error in self.errors:
            raw = error['raw']
            if 'NOT_FOUND' in raw:
                error_types['NOT_FOUND'] += 1
            elif 'failed' in raw.lower():
                error_types['FAILED'] += 1
            elif 'nvs' in raw.lower():
                error_types['NVS'] += 1
            else:
                error_types['OTHER'] += 1
        
        return {
            'total': len(self.errors),
            'status': 'WARNING' if len(self.errors) < 50 else 'CRITICAL',
            'categories': dict(error_types),
        }
    
    def _analyze_warnings(self) -> Dict:
        """Analyze warning patterns."""
        if not self.warnings:
            return {'total': 0, 'status': 'OK'}
        
        return {
            'total': len(self.warnings),
            'status': 'INFO',
        }
    
    def _analyze_init(self) -> Dict:
        """Analyze initialization status."""
        init_complete = any('SETUP COMPLETE' in step['raw'] for step in self.init_steps)
        return {
            'complete': init_complete,
            'steps_found': len(self.init_steps),
        }
    
    def print_report(self, analysis: Dict):
        """Print a formatted analysis report."""
        print("=" * 70)
        print("ESP32 Log Analysis Report")
        print("=" * 70)
        print()
        
        # Summary
        print("📊 SUMMARY")
        print("-" * 70)
        print(f"Total lines analyzed: {analysis['total_lines']}")
        if analysis['time_range']:
            duration_s = analysis['time_range']['duration_ms'] / 1000
            print(f"Time range: {analysis['time_range']['start']}ms - {analysis['time_range']['end']}ms ({duration_s:.1f}s)")
        print()
        
        # PSRAM Analysis
        psram = analysis['psram_analysis']
        print("💾 PSRAM ALLOCATION ANALYSIS")
        print("-" * 70)
        if psram['total'] == 0:
            print("✅ No PSRAM allocation failures")
        else:
            print(f"❌ Total failures: {psram['total']}")
            print(f"   Status: {psram['status']}")
            print(f"   Total bytes requested: {psram['total_bytes_requested']:,}")
            print(f"   Average size: {psram['avg_size']:.0f} bytes")
            print(f"   Size range: {psram['min_size']} - {psram['max_size']} bytes")
            print(f"   Average internal RAM free: {psram['avg_internal_free']:,.0f} bytes")
            print(f"   Minimum internal RAM free: {psram['min_internal_free']:,} bytes")
            print(f"   Size distribution:")
            for size_range, count in sorted(psram['size_distribution'].items()):
                print(f"     {size_range}: {count} allocations")
        print()
        
        # Memory Analysis
        mem = analysis['memory_analysis']
        print("🧠 MEMORY STATISTICS")
        print("-" * 70)
        if 'latest' in mem:
            latest = mem['latest']
            print(f"Heap free: {latest['heap_free']:,} / {latest['heap_total']:,} bytes")
            print(f"Fragmentation: {mem['heap_fragmentation']}%")
            print(f"Largest free block: {latest['largest_block']:,} bytes")
            print(f"PSRAM: {mem['psram_usage']['free']}KB / {mem['psram_usage']['total']}KB free")
            if mem['psram_usage']['total'] > 0:
                print(f"PSRAM usage: {mem['psram_usage']['used_percent']:.1f}%")
            print(f"Max loop time: {latest['max_loop']}ms")
            print(f"Slow loops detected: {latest['slow_loops']}")
        else:
            print("No memory statistics found")
        print()
        
        # Slow Loops
        slow = analysis['slow_loop_analysis']
        print("⏱️  SLOW LOOP ANALYSIS")
        print("-" * 70)
        if slow['total'] == 0:
            print("✅ No slow loops detected")
        else:
            print(f"⚠️  Total slow loops: {slow['total']}")
            print(f"   Max duration: {slow['max_duration']}ms")
            print(f"   Average duration: {slow['avg_duration']:.0f}ms")
            print(f"   Min duration: {slow['min_duration']}ms")
        print()
        
        # Errors
        errors = analysis['error_summary']
        print("❌ ERROR ANALYSIS")
        print("-" * 70)
        if errors['total'] == 0:
            print("✅ No errors found")
        else:
            print(f"⚠️  Total errors: {errors['total']}")
            print(f"   Status: {errors['status']}")
            if 'categories' in errors:
                print(f"   Categories:")
                for category, count in sorted(errors['categories'].items(), key=lambda x: x[1], reverse=True):
                    print(f"     {category}: {count}")
        print()
        
        # Warnings
        warnings = analysis['warning_summary']
        print("⚠️  WARNING ANALYSIS")
        print("-" * 70)
        print(f"Total warnings: {warnings['total']}")
        print()
        
        # Initialization
        init = analysis['init_status']
        print("🚀 INITIALIZATION STATUS")
        print("-" * 70)
        if init['complete']:
            print("✅ System initialization completed")
        else:
            print("⚠️  System initialization may not have completed")
        print(f"Initialization steps found: {init['steps_found']}")
        print()
        
        # Recommendations
        print("💡 RECOMMENDATIONS")
        print("-" * 70)
        recommendations = []
        
        if psram['total'] > 0:
            recommendations.append(
                f"⚠️  PSRAM is exhausted ({psram['total']} allocation failures). "
                "Consider reducing memory usage or enabling PSRAM in sdkconfig."
            )
        
        if slow['total'] > 0:
            recommendations.append(
                f"⚠️  Slow loops detected ({slow['total']} occurrences). "
                "This may cause network issues and watchdog resets. "
                "Optimize blocking operations."
            )
        
        if errors['total'] > 50:
            recommendations.append(
                f"⚠️  High error count ({errors['total']}). "
                "Review error patterns above."
            )
        
        if mem.get('heap_fragmentation', 0) > 50:
            recommendations.append(
                f"⚠️  High heap fragmentation ({mem['heap_fragmentation']}%). "
                "Consider memory pool allocation strategies."
            )
        
        if not recommendations:
            recommendations.append("✅ No critical issues detected. System appears healthy.")
        
        for i, rec in enumerate(recommendations, 1):
            print(f"{i}. {rec}")
        
        print()
        print("=" * 70)


def main():
    """Main entry point."""
    analyzer = LogAnalyzer()
    
    # Read from stdin
    try:
        for line in sys.stdin:
            analyzer.parse_line(line)
    except KeyboardInterrupt:
        print("\n\n⚠️  Analysis interrupted by user", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"\n\n❌ Error reading input: {e}", file=sys.stderr)
        sys.exit(1)
    
    # Perform analysis
    analysis = analyzer.analyze()
    
    # Print report
    analyzer.print_report(analysis)


if __name__ == '__main__':
    main()

