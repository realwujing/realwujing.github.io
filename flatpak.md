# flatpak

```plantuml
@startuml
start
:flatpak 查询命令行;
if (解析命令？) then (yes)
        if (检查本地xml描述文件更新时间是否已超时?) then (yes)
            :flatpak\n发起业务请求，附带gpg key作为token附带在http请求header中;
            if (ostree判断\n请求header中是否有token?) then (yes)
                ::nginx静态文件服务器处理业务逻辑;
                :ostree验证token有效性;
                if (token有效?) then (yes)
                    :返回业务逻辑处理结果;
                    :flatpak下载包元数据到本地;
                else (no)
                endif
            else (no)
            endif
        else (no)
        endif
        :使用appstream库解析xml获取包元数据;
else (no)
endif
:终端提示\nflaptpak命令行执行结果;
stop
@enduml
```

```bash
uos@uos:~$ flatpak search zoom --columns=all
Description                                                                       Application                      Version      Branch Remotes
Zoom - Video Conferencing, Web Conferencing, Webinars, Screen Sharing             us.zoom.Zoom                     5.11.10.4400 stable flathub
FIPS - OpenGL-based FITS image viewer                                             space.fips.Fips                  3.4.0        stable flathub
PhotoQt Image Viewer - View and manage images                                     org.photoqt.PhotoQt              2.9.1        stable flathub
KmPlot - Mathematical Function Plotter                                            org.kde.kmplot                   1.3.22080    stable flathub
XaoS - Fast interactive real-time fractal zoomer/morpher                          io.github.xaos_project.XaoS      4.2.1        stable flathub
Vieb - Vim Inspired Electron Browser                                              dev.vieb.Vieb                    9.0.0        stable flathub
sleek - todo manager based on the todo.txt syntax for Linux, free and open-sourc… com.github.ransome1.sleek        1.2.1        stable flathub
Minder - Create, develop and visualize your ideas                                 com.github.phase1geo.minder      1.14.0       stable flathub
Gnome Next Meeting Applet - Show your next events in your panel                   ….chmouel.gnomeNextMeetingApplet 2.8.1        stable flathub
wavbreaker - GUI tool to split WAV, MP2 and MP3 files 
```
