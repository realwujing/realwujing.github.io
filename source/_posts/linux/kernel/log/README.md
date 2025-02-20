---
date: 2023/10/16 14:50:23
updated: 2023/10/16 14:50:23
---

# kern.log

```bash
cat /proc/cmdline 
BOOT_IMAGE=/vmlinuz-4.19.0-arm64-desktop root=UUID=fb063bb0-c792-424a-a779-05f5cdab3dcf ro video=efifb:nobgrt splash console=tty plymouth.ignore-serial-consoles ignore_loglevel initcall_debug DEEPIN_GFXMODE= ima_appraise=off libahci.ignore_sss=1
```

```bash
sed -n '4662,$p' /var/log/kern.log > ~/kern.log # 4662是Booting Linux所在行
```

```bash
cat ~/kern.log | grep "initcall" | sed "s/\(.*\)after\(.*\)/\2 \1/g" | sort -n
```

## More

- [initcall_debug来查看开机慢问题](https://blog.csdn.net/rikeyone/article/details/84258391)
