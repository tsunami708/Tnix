#!/usr/bin/env python3
import subprocess
import time
import re
import sys

expected_patterns = [
    r"fsinit done , superblock info:",
    r"fs_name:tsunami",
    r"imap:2",
    r"inodes:6",
    r"bmap:115206",
    r"blocks:115334",
    r"max_i:32767",
    r"max_b:1163909",
    r"CPU 0 first-sche systemd",
    r"cpu0 trigger syn_syscall_u",
]

def check_output(output_text, test_num):
    missing_patterns = []
    for pattern in expected_patterns:
        if not re.search(pattern, output_text):
            missing_patterns.append(pattern)
    if missing_patterns:
        print(f"测试 #{test_num:03d} FAILED - 缺少模式: {missing_patterns}")
        return False, output_text
    else:
        print(f"测试 #{test_num:03d} PASSED")
        return True, output_text

def run_single_test(test_num):
    try:
        process = subprocess.Popen(
            ["make", "qemu", "CPUS=1"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            universal_newlines=True
        )
        start_time = time.time()
        output_lines = []
        timeout = 0.1
        while True:
            if time.time() - start_time > timeout:
                print(f"测试 #{test_num:03d} TIMEOUT - 在{timeout}秒内未完成输出")
                process.terminate()
                time.sleep(0.1)
                if process.poll() is None:
                    process.kill()
                final_output = ''.join(output_lines)
                return False, final_output
            line = process.stdout.readline()
            if line:
                output_lines.append(line)
                current_output = ''.join(output_lines)
                if all(re.search(pattern, current_output) for pattern in expected_patterns):
                    print(f"测试 #{test_num:03d} SUCCESS - 在{time.time()-start_time:.3f}秒内完成")
                    process.terminate()
                    time.sleep(0.1)
                    if process.poll() is None:
                        process.kill()
                    return True, current_output
            else:
                if process.poll() is not None:
                    break
                time.sleep(0.001)
        final_output = ''.join(output_lines)
        return check_output(final_output, test_num)
    except Exception as e:
        print(f"测试 #{test_num:03d} ERROR - {e}")
        return False, str(e)

def main():
    total_tests = 100
    passed_tests = 0
    failed_tests = 0
    failed_output = ""
    print(f"开始执行 {total_tests} 次测试...")
    print("期望输出包含:")
    for pattern in expected_patterns:
        print(f"  - {pattern}")
    print("-" * 50)
    for i in range(1, total_tests + 1):
        result, output = run_single_test(i)
        if result:
            passed_tests += 1
        else:
            failed_tests += 1
            failed_output = output
            print(f"\n发现失败测试，终止后续测试!")
            break
    print("-" * 50)
    print(f"测试完成!")
    print(f"执行测试: {passed_tests + failed_tests}, 通过: {passed_tests}, 失败: {failed_tests}")
    
    if failed_tests > 0:
        print(f"\n失败测试 #{passed_tests + failed_tests} 的完整输出内容:")
        print("=" * 80)
        print(failed_output)
        print("=" * 80)
        print(f"在第 {passed_tests + failed_tests} 次测试时失败! ✗")
        sys.exit(1)
    else:
        print(f"所有 {passed_tests} 次测试都通过了! ✓")
        sys.exit(0)

if __name__ == "__main__":
    main()

