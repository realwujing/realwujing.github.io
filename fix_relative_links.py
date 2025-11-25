#!/usr/bin/env python3
"""
修复Hexo source目录中所有markdown文件的相对链接
将相对链接转换为GitHub绝对链接,以便在Hexo静态网站中正确显示
"""

import os
import re
import sys
from pathlib import Path


def get_base_dir(file_path: str) -> str:
    """
    计算文件在main分支的基准目录
    
    Args:
        file_path: source目录下的文件路径,如 source/_posts/linux/README.md
    
    Returns:
        基准目录,如 linux 或 空字符串(根目录)
    """
    # 移除source/_posts/前缀
    if file_path.startswith('source/_posts/'):
        main_path = file_path[len('source/_posts/'):]
    elif file_path == 'source/about/index.md':
        return ''  # about/index.md对应main分支根目录
    else:
        main_path = file_path
    
    # 获取文件所在目录
    dir_path = os.path.dirname(main_path)
    
    # 如果是当前目录或空,返回空字符串
    return '' if dir_path in ['.', ''] else dir_path


def fix_links_in_file(file_path: str, base_url: str = 'https://github.com/realwujing/realwujing.github.io'):
    """
    修复单个markdown文件中的相对链接
    
    Args:
        file_path: markdown文件路径
        base_url: GitHub仓库基础URL
    """
    base_dir = get_base_dir(file_path)
    base_path = f"{base_dir}/" if base_dir else ""
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original_content = content
        
        # 0. 图片文件 -> 转换为Hexo绝对路径
        # 将相对路径的图片转换为从网站根目录开始的绝对路径
        if base_path:
            content = re.sub(
                r'!\[([^\]]*)\]\(([^/][^\)]+?)\.(png|jpg|jpeg|gif|svg|webp)\)',
                f'![\\1](/{base_path}\\2.\\3)',
                content
            )
        
        # 1. 文档文件(pdf/pptx/docx/epub/mobi) -> GitHub raw链接(下载)
        content = re.sub(
            r'\]\(\./([^\)]+?)\.(pdf|pptx|docx|epub|mobi)\)',
            f']({base_url}/raw/main/{base_path}\\1.\\2)',
            content
        )
        
        # 2. Markdown文件 -> GitHub blob链接(查看)
        content = re.sub(
            r'\]\(\./([^\)]+?)\.md\)',
            f']({base_url}/blob/main/{base_path}\\1.md)',
            content
        )
        
        # 3. 目录链接 -> GitHub tree链接(浏览)
        content = re.sub(
            r'\]\(\./([^\)]+?)/\)',
            f']({base_url}/tree/main/{base_path}\\1)',
            content
        )
        
        # 只有内容发生变化时才写入文件
        if content != original_content:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            print(f"✓ Fixed: {file_path}")
        
    except Exception as e:
        print(f"✗ Error processing {file_path}: {e}", file=sys.stderr)


def main():
    """主函数:处理source目录下所有markdown文件"""
    source_dir = Path('source')
    
    if not source_dir.exists():
        print(f"Error: {source_dir} directory not found", file=sys.stderr)
        sys.exit(1)
    
    # 查找所有markdown文件
    md_files = list(source_dir.rglob('*.md'))
    
    if not md_files:
        print("No markdown files found in source directory")
        return
    
    print(f"Found {len(md_files)} markdown files")
    print(f"Processing relative links...")
    
    for md_file in md_files:
        fix_links_in_file(str(md_file))
    
    print("Done!")


if __name__ == '__main__':
    main()
