set nocompatible              " be iMproved, required
filetype off                  " required

" set the runtime path to include Vundle and initialize
set rtp+=~/.vim/bundle/Vundle.vim
call vundle#begin()
" alternatively, pass a path where Vundle should install plugins
"call vundle#begin('~/some/path/here')

" let Vundle manage Vundle, required
Plugin 'VundleVim/Vundle.vim'

" The following are examples of different formats supported.
" Keep Plugin commands between vundle#begin/end.
" plugin on GitHub repo
Plugin 'tpope/vim-fugitive'
" plugin from http://vim-scripts.org/vim/scripts.html
" Plugin 'L9'
" Git plugin not hosted on GitHub
Plugin 'git://git.wincent.com/command-t.git'
" git repos on your local machine (i.e. when working on your own plugin)
Plugin 'file:///home/gmarik/path/to/plugin'
" The sparkup vim script is in a subdirectory of this repo called vim.
" Pass the path to set the runtimepath properly.
Plugin 'rstacruz/sparkup', {'rtp': 'vim/'}
" Install L9 and avoid a Naming conflict if you've already installed a
" different version somewhere else.
" Plugin 'ascenator/L9', {'name': 'newL9'}

Plugin 'preservim/nerdtree'
Plugin 'taglist.vim'
Plugin 'winmanager'

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

""将F2设置为开关NERDTree的快捷键
map <F2> :NERDTreeToggle
""修改树的显示图标
let g:NERDTreeDirArrowExpandable = '+'
let g:NERDTreeDirArrowCollapsible = '-'
""窗口位置
let g:NERDTreeWinPos='left'
""窗口尺寸
let g:NERDTreeSize=30
""窗口是否显示行号
let g:NERDTreeShowLineNumbers=1
""不显示隐藏文件
let g:NERDTreeHidden=0
""打开vim时如果没有文件自动打开NERDTree
autocmd vimenter * if !argc()|NERDTree|endif
""当NERDTree为剩下的唯一窗口时自动关闭
 autocmd bufenter * if (winnr("$") == 1 && exists("b:NERDTreeType") &&b:NERDTreeType == "primary") | q | endif
""打开vim时自动打开NERDTree
"autocmd vimenter * NERDTree

 """"配置taglist

 let Tlist_Show_One_File=1     "不同时显示多个文件的tag，只显示当前文件的
 let Tlist_Exit_OnlyWindow=1   "如果taglist窗口是最后一个窗口，则退出vim
 let Tlist_Ctags_Cmd="/usr/bin/ctags" "将taglist与ctags关联

 "==========================
 "        1. General
 "==========================
 " set to auto read when a file is changed from the outside
 set autoread


 "==========================
 "    2. Colors and Fonts
 "==========================
 " enable syntax highlight
 syntax enable


 "==========================
 "    3. VIM UserInterface
 "==========================
 " show line number
 set nu

 " show matching bracets
 set showmatch

 " highlight search things
 set hlsearch


 "=========================
 "    4. Text Options
 "=========================
 " set Tab = 4 spaces
 set ts=4


 "=========================
 "       5. others
 "=========================
 let Tlist_Ctags_Cmd = '/usr/bin/ctags'
 let Tlist_WinWidth=40
 let Tlist_Auto_Highlight_Tag=1

 "设置taglist自动打开
 let Tlist_Auto_Open=0

 let Tlist_Auto_Update=1
 let Tlist_Display_Tag_Scope=1
 let Tlist_Exit_OnlyWindow=1
 let Tlist_Enable_Dold_Column=1
 let Tlist_File_Fold_Auto_Close=1
 let Tlist_Show_One_File=1
 "let Tlist_Use_Right_Window=1
 let Tlist_Use_SingleClick=1
 nnoremap <F3> :TlistToggle<CR>

 filetype plugin on
 autocmd FileType python set omnifunc=pythoncomplete#Complete
 autocmd FileType javascrīpt set omnifunc=javascriptcomplete#CompleteJS
 autocmd FileType html set omnifunc=htmlcomplete#CompleteTags
 autocmd FileType css set omnifunc=csscomplete#CompleteCSS
 autocmd FileType xml set omnifunc=xmlcomplete#CompleteTags
 autocmd FileType php set omnifunc=phpcomplete#CompletePHP
 autocmd FileType c set omnifunc=ccomplete#Complete

 """配置winmanager
"""""""""""""""""""""""""""""""
"" winManager setting
"""""""""""""""""""""""""""""""
"设置界面分割
let g:winManagerWindowLayout = "TagList|FileExplorer,BufExplorer"
"设置winmanager的宽度，默认为25
let g:winManagerWidth = 30
"定义打开关闭winmanager按键
nmap <silent> <F4> :WMToggle<cr>

