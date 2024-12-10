---
date: 2024/08/02 15:40:15
updated: 2024/08/14 21:50:39
---

# ksmd-taskset打包说明

要将`ksmd_taskset.sh`、`ksmd-taskset.service`这两个文件打成RPM包，可以按以下步骤操作：

1. **准备目录结构**：RPM包需要特定的目录结构。首先，在一个工作目录中创建需要的目录和文件。
2. **编写SPEC文件**：SPEC文件定义了如何构建和安装RPM包。
3. **构建RPM包**：使用`rpmbuild`工具构建RPM包。

## 步骤1：准备目录结构

在一个工作目录中创建以下目录结构：

```bash
my_package/
├── BUILD
├── RPMS
├── SOURCES
│   ├── ksmd_taskset.sh
│   ├── ksmd-taskset.service
├── SPECS
│   └── ksmd-taskset.spec
└── SRPMS
```

## 步骤2：编写SPEC文件

在`SPECS`目录中创建一个名为`ksmd-taskset.spec`的文件，内容如下：

```spec
Name:           ksmd-taskset
Version:        1.0
Release:        1%{?dist}
Summary:        KSMD Taskset Service

License:        GPL
URL:            http://example.com/
Source0:        ksmd_taskset.sh
Source1:        ksmd-taskset.service

%description
This package provides a systemd service to set the CPU affinity of ksmd using taskset.

%prep

%build

%install
mkdir -p %{buildroot}/usr/lib/ctyunos/
install -m 755 %{SOURCE0} %{buildroot}/usr/lib/ctyunos/ksmd_taskset.sh

mkdir -p %{buildroot}/etc/systemd/system/
install -m 644 %{SOURCE1} %{buildroot}/etc/systemd/system/ksmd-taskset.service

%files
/usr/lib/ctyunos/ksmd_taskset.sh
/etc/systemd/system/ksmd-taskset.service

%post
/bin/systemctl daemon-reload
/bin/systemctl enable --now ksmd-taskset.service

%preun
# Actions before package is uninstalled
# Stop the service if it is running
if [ $1 -eq 0 ]; then
    /bin/systemctl stop ksmd-taskset.service
fi

%postun
# Actions after package is uninstalled
# Disable the service
if [ $1 -eq 0 ]; then
    /bin/systemctl disable ksmd-taskset.service
fi
# Remove the service file and script
rm -f /usr/lib/ctyunos/ksmd_taskset.sh
rm -f /etc/systemd/system/ksmd-taskset.service
/bin/systemctl daemon-reload

%changelog
* Fri Aug 01 2024 Your Name <your.email@example.com> - 1.0-1
- Initial package
```

## 步骤3：构建RPM包

确保你已经安装了`rpm-build`工具包，然后执行以下命令来构建RPM包：

```bash
cd ksmd-taskset
rpmbuild --define "_topdir $(pwd)" -ba SPECS/ksmd-taskset.spec
```

这将会在`RPMS`目录下生成一个`.rpm`文件。

## 说明

- `Source0`和`Source1`指定了源文件（即脚本和服务文件）。
- `%install`部分定义了安装脚本和服务文件的位置。
- `%files`部分列出了在RPM包中包含的文件和它们的安装位置。

你可以根据需要调整SPEC文件中的细节，例如版本号、发布号、摘要、描述和作者信息等。
