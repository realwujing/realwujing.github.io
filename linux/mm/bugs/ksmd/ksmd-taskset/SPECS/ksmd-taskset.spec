Name:           ksmd-taskset
Version:        1.0
Release:        1%{?dist}
Summary:        KSMD Taskset Service

License:        GPL
URL:             https://ctyunos.ctyun.cn/
Source0:        ksmd_taskset.sh
Source1:        ksmd-taskset.service

BuildArch:      noarch

%description
This package provides a systemd service to set the CPU affinity of ksmd using taskset.

%prep

%build

%install
mkdir -p %{buildroot}/usr/local/ctyunos/
install -m 755 %{SOURCE0} %{buildroot}/usr/local/ctyunos/ksmd_taskset.sh

mkdir -p %{buildroot}/usr/lib/systemd/system/
install -m 644 %{SOURCE1} %{buildroot}/usr/lib/systemd/system/ksmd-taskset.service

%files
/usr/local/ctyunos/ksmd_taskset.sh
/usr/lib/systemd/system/ksmd-taskset.service

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
/bin/systemctl daemon-reload

%changelog
* Thu Aug 01 2024 QiLiang Yuan <yuanql9@chinatelecom.cn> - 1.0-1
- Initial package
