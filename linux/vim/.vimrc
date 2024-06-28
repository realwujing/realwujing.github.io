""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""  
" 显示相关    
""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""  
"set shortmess=atI   " 启动的时候不显示那个援助乌干达儿童的提示    
"winpos 5 5          " 设定窗口位置    
"set lines=40 columns=155    " 设定窗口大小    
set nu              " 显示行号    
set go=             " 不要图形按钮    
"color asmanian2     " 设置背景主题    
set guifont=Courier_New:h10:cANSI   " 设置字体    
syntax on           " 语法高亮    
autocmd InsertLeave * se nocul  " 用浅色高亮当前行    
autocmd InsertEnter * se cul    " 用浅色高亮当前行    
"set ruler           " 显示标尺    
set showcmd         " 输入的命令显示出来，看的清楚些    
"set cmdheight=1     " 命令行（在状态行下）的高度，设置为1    
"set whichwrap+=<,>,h,l   " 允许backspace和光标键跨越行边界(不建议)    
"set scrolloff=3     " 光标移动到buffer的顶部和底部时保持3行距离    
set novisualbell    " 不要闪烁(不明白)    
"set statusline=%F%m%r%h%w\ [FORMAT=%{&ff}]\ [TYPE=%Y]\ [POS=%l,%v][%p%%]\ %{strftime(\"%d/%m/%y\ -\ %H:%M\")}   "状态行显示的内容    
set laststatus=1    " 启动显示状态行(1),总是显示状态行(2)    
set foldenable      " 允许折叠    
set foldmethod=indent   " 缩进折叠    
"set background=dark "背景使用黑色   
set nocompatible  "去掉讨厌的有关vi一致性模式，避免以前版本的一些bug和局限    
  
" 显示中文帮助  
if version >= 603  
    set helplang=cn  
    set encoding=utf-8  
endif  
  
" 设置配色方案  
colorscheme murphy  
"colorscheme molokai

"字体   
"if (has("gui_running"))   
"   set guifont=Bitstream\ Vera\ Sans\ Mono\ 10   
"endif   
  
set fencs=utf-8,ucs-bom,shift-jis,gb18030,gbk,gb2312,cp936  
set termencoding=utf-8  
set encoding=utf-8  
set fileencodings=ucs-bom,utf-8,cp936  
set fileencoding=utf-8  
  
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""  
"""""新文件标题""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""  
"新建.c,.h,.sh,.java文件，自动插入文件头   
"autocmd BufNewFile *.cpp,*.[ch],*.sh,*.java exec ":call SetTitle()"   
""定义函数SetTitle，自动插入文件头   
func SetTitle()   
    "如果文件类型为.sh文件   
    if &filetype == 'sh'   
        call setline(1,"\#########################################################################")   
        call append(line("."), "\# File Name: ".expand("%"))   
        call append(line(".")+1, "\# Author: wujing")   
        call append(line(".")+2, "\# mail: realwujing@qq.com")   
        call append(line(".")+3, "\# Created Time: ".strftime("%c"))   
        call append(line(".")+4, "\#########################################################################")   
        call append(line(".")+5, "\#!/bin/bash")   
        call append(line(".")+6, "")   
    else   
        call setline(1, "/*************************************************************************")   
        call append(line("."), "    > File Name: ".expand("%"))   
        call append(line(".")+1, "    > Author: wujing")   
        call append(line(".")+2, "    > Mail: realwujing@qq.com ")   
        call append(line(".")+3, "    > Created Time: ".strftime("%c"))   
        call append(line(".")+4, " ************************************************************************/")   
        call append(line(".")+5, "")  
    endif  
  
    if &filetype == 'cpp'  
        call append(line(".")+6, "#include<iostream>")  
        call append(line(".")+7, "using namespace std;")  
        call append(line(".")+8, "")  
    endif  
  
    if &filetype == 'c'  
        call append(line(".")+6, "#include<stdio.h>")  
        call append(line(".")+7, "")  
    endif  
  
    "新建文件后，自动定位到文件末尾  
    autocmd BufNewFile * normal G  
endfunc   
  
""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""  
"键盘命令  
""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""  
nmap <leader>w :w!<cr>  
nmap <leader>f :find<cr>  
" 映射全选+复制 ctrl+a  
map <C-A> ggVGY  
map! <C-A> <Esc>ggVGY  
map <F12> gg=G  
" 选中状态下 Ctrl+c 复制  
vmap <C-c> "+y  
  
"去空行    
nnoremap <F2> :g/^\s*$/d<CR>   
  
"比较文件    
nnoremap <C-F2> :vert diffsplit   
  
"新建标签    
map <M-F2> :tabnew<CR>    
  
"列出当前目录文件    
map <F3> :tabnew .<CR>    

"打开树状文件目录    
map <C-F3> \be    

"C，C++ 按F5编译运行  
map <F5> :call CompileRunGcc()<CR>  
func! CompileRunGcc()  
    exec "w"  
    if &filetype == 'c'  
        exec "!g++ % -o %<"  
        exec "! ./%<"  
    elseif &filetype == 'cpp'  
        exec "!g++ % -o %<"  
        exec "! ./%<"  
    elseif &filetype == 'java'   
        exec "!javac %"   
        exec "!java %<"  
    elseif &filetype == 'sh'  
        :!./%  
    endif  
endfunc  
  
"C,C++的调试  
map <F8> :call Rungdb()<CR>  
func! Rungdb()  
    exec "w"  
    exec "!g++ % -g -o %<"  
    exec "!gdb ./%<"  
endfunc  
  
""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""  
""实用设置  
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""  
  
" 设置当文件被改动时自动载入  
set autoread  
" quickfix模式  
autocmd FileType c,cpp map <buffer> <leader><space> :w<cr>:make<cr>  
  
"代码补全   
set completeopt=preview,menu   
  
"允许插件    
filetype plugin on  
  
"共享剪贴板    
set clipboard+=unnamed   
  
"从不备份    
set nobackup  
  
"make 运行  
:set makeprg=g++\ -Wall\ \ %  
  
"自动保存  
  
set autowrite  
set ruler                   " 打开状态栏标尺  
set cursorline              " 突出显示当前行  
set magic                   " 设置魔术  
set guioptions-=T           " 隐藏工具栏  
set guioptions-=m           " 隐藏菜单栏  
"set statusline=\ %<%F[%1*%M%*%n%R%H]%=\ %y\ %0(%{&fileformat}\ %{&encoding}\ %c:%l/%L%)\  
  
" 设置在状态行显示的信息  
set foldcolumn=0  
set foldmethod=indent   
set foldlevel=3   
set foldenable              " 开始折叠  
  
" 不要使用vi的键盘模式，而是vim自己的  
set nocompatible  
  
" 语法高亮  
set syntax=on  
  
" 去掉输入错误的提示声音  
set noeb  
  
" 在处理未保存或只读文件的时候，弹出确认  
set confirm  
  
" 自动缩进  
set autoindent  
set cindent  
  
" Tab键的宽度  
set tabstop=4  
  
" 统一缩进为4  
set softtabstop=4  
set shiftwidth=4  
  
" 不要用空格代替制表符  
set noexpandtab  
  
" 在行和段开始处使用制表符  
set smarttab  
  
" 显示行号  
set number  
  
" 历史记录数  
set history=1000  
  
"禁止生成临时文件  
set nobackup  
set noswapfile  
  
"搜索忽略大小写  
set ignorecase  
  
"搜索逐字符高亮  
set hlsearch  
set incsearch  
  
"行内替换  
set gdefault  
  
"编码设置  
set enc=utf-8  
set fencs=utf-8,ucs-bom,shift-jis,gb18030,gbk,gb2312,cp936  
  
"语言设置  
set langmenu=zh_CN.UTF-8  
set helplang=cn  
  
" 我的状态行显示的内容（包括文件类型和解码）  
"set statusline=%F%m%r%h%w\ [FORMAT=%{&ff}]\ [TYPE=%Y]\ [POS=%l,%v][%p%%]\ %{strftime(\"%d/%m/%y\ -\ %H:%M\")}  
"set statusline=[%F]%y%r%m%*%=[Line:%l/%L,Column:%c][%p%%]  
  
" 总是显示状态行  
set laststatus=2  
  
" 命令行（在状态行下）的高度，默认为1，这里是2  
set cmdheight=2  
  
" 侦测文件类型  
filetype on  
  
" 载入文件类型插件  
filetype plugin on  
  
" 为特定文件类型载入相关缩进文件  
filetype indent on  
  
" 保存全局变量  
set viminfo+=!  
  
" 带有如下符号的单词不要被换行分割  
set iskeyword+=_,$,@,%,#,-  
  
" 字符间插入的像素行数目  
set linespace=0  
  
" 增强模式中的命令行自动完成操作  
set wildmenu  
  
" 使回格键（backspace）正常处理indent, eol, start等  
set backspace=2  
  
" 允许backspace和光标键跨越行边界  
set whichwrap+=<,>,h,l  
  
" 可以在buffer的任何地方使用鼠标（类似office中在工作区双击鼠标定位）  
set mouse=a  
set selection=exclusive  
set selectmode=mouse,key  
  
" 通过使用: commands命令，告诉我们文件的哪一行被改变过  
set report=0  
  
" 在被分割的窗口间显示空白，便于阅读  
set fillchars=vert:\ ,stl:\ ,stlnc:\  
  
" 高亮显示匹配的括号  
set showmatch  
  
" 匹配括号高亮的时间（单位是十分之一秒）  
set matchtime=1  
  
" 光标移动到buffer的顶部和底部时保持3行距离  
set scrolloff=3  
  
" 为C程序提供自动缩进  
set smartindent  
  
" 高亮显示普通txt文件（需要txt.vim脚本）  
au BufRead,BufNewFile *  setfiletype txt  
  
"自动补全  
:inoremap ( ()<ESC>i  
:inoremap ) <c-r>=ClosePair(')')<CR>  
:inoremap { {<CR>}<ESC>O  
:inoremap } <c-r>=ClosePair('}')<CR>  
:inoremap [ []<ESC>i  
:inoremap ] <c-r>=ClosePair(']')<CR>  
:inoremap " ""<ESC>i  
:inoremap ' ''<ESC>i  
function! ClosePair(char)  
    if getline('.')[col('.') - 1] == a:char  
        return "\<Right>"  
    else  
        return a:char  
    endif  
endfunction  
filetype plugin indent on   
  
"打开文件类型检测, 加了这句才可以用智能补全  
set completeopt=longest,menu 

"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
" 插件安装管理 
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

"set nocompatible              " be iMproved, required
"filetype off                  " required
 
" set the runtime path to include Vundle and initialize
set rtp+=~/.vim/bundle/Vundle.vim
call vundle#begin()
" alternatively, pass a path where Vundle should install plugins
"call vundle#begin('~/some/path/here')
" let Vundle manage Vundle, required
Plugin 'VundleVim/Vundle.vim'

Plugin 'scrooloose/nerdtree'

Plugin 'majutsushi/tagbar'

Plugin 'jiangmiao/auto-pairs'

Plugin 'minibufexpl.vim'

Plugin 'taglist.vim'

Plugin 'winmanager'

Plugin 'godlygeek/tabular'

Plugin 'plasticboy/vim-markdown'

Plugin 'tpope/vim-fugitive'

Plugin 'tpope/vim-surround'

Plugin 'scrooloose/syntastic'

Plugin 'vim-airline/vim-airline'

Plugin 'altercation/vim-colors-solarized'

Plugin 'valloric/youcompleteme'

Plugin 'kien/ctrlp.vim'

" All of your Plugins must be added before the following line
call vundle#end()            " required
filetype plugin indent on    " required
" To ignore plugin indent changes, instead use:
"filetype plugin on
"
" Brief help
" :PluginList       - lists configured plugins
" :PluginInstall    - installs plugins; append `!` to update or just :PluginUpdate
" :PluginSearch foo - searches for foo; append `!` to refresh local cache
" :PluginClean      - confirms removal of unused plugins; append `!` to auto-approve removal
"
" see :h vundle for more details or wiki for FAQ
" Put your non-Plugin stuff after this line

"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
"   nerdtree
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
" 启动vim时自动打开NERDTree，并将光标放在Tree的Tag
"autocmd VimEnter * NERDTree

" 启动vim时自动打开NERDTree，并将光标放在vim打开的文件
"autocmd VimEnter * NERDTree | wincmd p

" 如果退出vim后只剩Tree的Tag的话，则自动退出Tree的Tag
"autocmd BufEnter * if winnr('$') == 1 && exists('b:NERDTree') && b:NERDTree.isTabTree() | quit | endif

" let NERDTreeQuitOnOpen=1 "打开文件时关闭树
let NERDTreeShowBookmarks=1 "显示书签


let mapleader = ","
map <leader>ne :NERDTreeToggle<CR>
map <leader>tl :TlistToggle<CR>
nnoremap <leader>ma :set mouse=a<cr>
nnoremap <leader>mu :set mouse=<cr>

"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
"   tagbar
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
let mapleader = ","
nmap <leader>tb :TagbarToggle<CR>

  
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""  
" CTags的设定    
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""  
let Tlist_Sort_Type = "name"    " 按照名称排序    
let Tlist_Use_Right_Window = 1  " 在右侧显示窗口    
let Tlist_Compart_Format = 1    " 压缩方式    
let Tlist_Exist_OnlyWindow = 1  " 如果只有一个buffer，kill窗口也kill掉buffer    
let Tlist_File_Fold_Auto_Close = 0  " 不要关闭其他文件的tags    
let Tlist_Enable_Fold_Column = 0    " 不要显示折叠树    
"autocmd FileType java set tags+=D:\tools\java\tags    
"autocmd FileType h,cpp,cc,c set tags+=D:\tools\cpp\tags    
"let Tlist_Show_One_File=1            "不同时显示多个文件的tag，只显示当前文件的  
  
"设置tags    
set tags=tags    
"set autochdir   

""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""  
"其他东东  
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""  
  
"默认打开Taglist   
let Tlist_Auto_Open=1   
  
""""""""""""""""""""""""""""""   
" Tag list (ctags)   
""""""""""""""""""""""""""""""""   
let Tlist_Ctags_Cmd = '/usr/bin/ctags'   
let Tlist_Show_One_File = 1 "不同时显示多个文件的tag，只显示当前文件的   
let Tlist_Exit_OnlyWindow = 1 "如果taglist窗口是最后一个窗口，则退出vim   
let Tlist_Use_Right_Window = 1 "在右侧窗口中显示taglist窗口  
  
" minibufexpl插件的一般设置  
let g:miniBufExplMapWindowNavVim = 1  
let g:miniBufExplMapWindowNavArrows = 1  
let g:miniBufExplMapCTabSwitchBufs = 1  
let g:miniBufExplModSelTarget = 1


"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
" cscope的设定
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""
if has("cscope")
set csprg=/usr/bin/cscope
"指定:cstag的搜索顺序。0表示先搜索cscope数据库，若不匹配，再搜索tag文件，1
"则相反
set csto=0
":tag/Ctrl-]/vim -t将使用:cstag，而不是默认的:tag
set cst
" +(将结果追加到quickfix窗口)、-(清空上一次的结果)、0(不使用quickfix。没有指定也相当于标志为0)))
set cscopequickfix=s-,c-,d-,i-,t-,e- " 使用QuickFix窗口来显示cscope查找结果
set nocsverb "增加cscope数据库时，将不会打印成功或失败信息
set cspc=3 "指定在查找结果中显示多少级文件路径,默认值0表示显示全路径,1表示只显示文件名"
if filereadable("cscope.out")
cs add $PWD/cscope.out $PWD
"cs add cscope.out
else " 子目录打开，向上查找
let cscope_file=findfile("cscope.out", ".;")
let cscope_pre=matchstr(cscope_file, ".*/")
if !empty(cscope_file) && filereadable(cscope_file)
exe "cs add" cscope_file cscope_pre
endif
endif
set nocsverb
endif
set cscopequickfix=s-,c-,d-,i-,t-,e-
nmap <C-\>s :cs find s <C-R>=expand("<cword>")<CR><CR>
nmap <C-\>g :cs find g <C-R>=expand("<cword>")<CR><CR>
nmap <C-\>c :cs find c <C-R>=expand("<cword>")<CR><CR>
nmap <C-\>t :cs find t <C-R>=expand("<cword>")<CR><CR>
nmap <C-\>e :cs find e <C-R>=expand("<cword>")<CR><CR>
nmap <C-\>f :cs find f <C-R>=expand("<cfile>")<CR><CR>
nmap <C-\>i :cs find i ^<C-R>=expand("<cfile>")<CR>$<CR>
nmap <C-\>d :cs find d <C-R>=expand("<cword>")<CR><CR>
" 使用时，将光标停留在要查找的对象上，按下<C->g，即先按“Ctrl+\”，然后很快再按“g”，将会查找该对象的定义。

