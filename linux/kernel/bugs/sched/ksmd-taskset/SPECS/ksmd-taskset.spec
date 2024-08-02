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
